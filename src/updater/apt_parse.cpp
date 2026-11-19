/*
 * SPDX-License-Identifier: MIT
 */

#include "apt_parse.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace updater {
namespace {

std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }
    return s.substr(a, b - a);
}

bool starts_with(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

// Text between the first `open` and the following `close`, or empty.
std::string between(const std::string& s, char open, char close) {
    const auto a = s.find(open);
    if (a == std::string::npos) {
        return {};
    }
    const auto b = s.find(close, a + 1);
    if (b == std::string::npos) {
        return {};
    }
    return s.substr(a + 1, b - a - 1);
}

std::string first_word(const std::string& s) {
    const auto end = s.find(' ');
    return end == std::string::npos ? s : s.substr(0, end);
}

bool parse_number(const std::string& s, double& out) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0';
}

} // namespace

const char* operation_label(Operation op) {
    switch (op) {
        case Operation::Upgrade:
            return "Upgrade";
        case Operation::FullUpgrade:
            return "Full upgrade";
        case Operation::Autoremove:
            return "Autoremove";
        case Operation::Refresh:
            return "Refresh lists";
        case Operation::Clean:
            return "Clean caches";
    }
    return "";
}

const char* operation_apt_verb(Operation op) {
    switch (op) {
        case Operation::Upgrade:
            return "upgrade";
        case Operation::FullUpgrade:
            return "dist-upgrade";
        case Operation::Autoremove:
            return "autoremove";
        case Operation::Refresh:
            return "update";
        case Operation::Clean:
            return "clean";
    }
    return "";
}

bool operation_downloads(Operation op) {
    return op == Operation::Upgrade || op == Operation::FullUpgrade;
}

bool operation_is_direct(Operation op) {
    return op == Operation::Refresh || op == Operation::Clean;
}

int Plan::upgrades() const {
    int n = 0;
    for (const auto& a : actions) {
        n += a.kind == PlanAction::Kind::Install && !a.old_version.empty();
    }
    return n;
}

int Plan::new_installs() const {
    int n = 0;
    for (const auto& a : actions) {
        n += a.kind == PlanAction::Kind::Install && a.old_version.empty();
    }
    return n;
}

int Plan::removals() const {
    int n = 0;
    for (const auto& a : actions) {
        n += a.kind == PlanAction::Kind::Remove || a.kind == PlanAction::Kind::Purge;
    }
    return n;
}

Plan parse_simulation(const std::string& output) {
    Plan plan;
    std::istringstream in(output);
    std::string line;
    bool in_kept_back = false;

    while (std::getline(in, line)) {
        if (in_kept_back) {
            if (!line.empty() && line[0] == ' ') {
                std::istringstream words(line);
                std::string word;
                while (words >> word) {
                    plan.kept_back.push_back(word);
                }
                continue;
            }
            in_kept_back = false;
        }
        if (starts_with(line, "The following packages have been kept back:")) {
            in_kept_back = true;
            continue;
        }

        PlanAction action;
        std::string rest;
        if (starts_with(line, "Inst ")) {
            action.kind = PlanAction::Kind::Install;
        }
        else if (starts_with(line, "Conf ")) {
            action.kind = PlanAction::Kind::Configure;
        }
        else if (starts_with(line, "Remv ")) {
            action.kind = PlanAction::Kind::Remove;
        }
        else if (starts_with(line, "Purg ")) {
            action.kind = PlanAction::Kind::Purge;
        }
        else {
            continue;
        }
        rest = line.substr(5);
        action.package = first_word(rest);

        // "[old]" appears before "(new suite [arch])"; only look at the part before '('.
        const auto paren = rest.find('(');
        const auto head = rest.substr(0, paren);
        action.old_version = between(head, '[', ']');
        if (paren != std::string::npos) {
            action.new_version = first_word(between(rest.substr(paren), '(', ')'));
        }
        if (!action.package.empty()) {
            plan.actions.push_back(std::move(action));
        }
    }
    return plan;
}

std::vector<DownloadItem> parse_print_uris(const std::string& output) {
    std::vector<DownloadItem> items;
    std::istringstream in(output);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.size() < 2 || line[0] != '\'') {
            continue;
        }
        const auto uri_end = line.find('\'', 1);
        if (uri_end == std::string::npos) {
            continue;
        }
        DownloadItem item;
        item.uri = line.substr(1, uri_end - 1);

        std::istringstream fields(line.substr(uri_end + 1));
        std::string size;
        std::string hash;
        fields >> item.filename >> size >> hash;
        if (item.filename.empty() || size.empty()) {
            continue;
        }
        item.size = std::strtoull(size.c_str(), nullptr, 10);
        const auto colon = hash.find(':');
        if (colon != std::string::npos) {
            item.hash_type = hash.substr(0, colon);
            item.hash = hash.substr(colon + 1);
        }
        items.push_back(std::move(item));
    }
    return items;
}

std::string aria2_checksum(const DownloadItem& item) {
    if (item.hash.empty()) {
        return {};
    }
    std::string type;
    for (char c : item.hash_type) {
        type += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const char* aria = nullptr;
    if (type == "sha512") {
        aria = "sha-512";
    }
    else if (type == "sha256") {
        aria = "sha-256";
    }
    else if (type == "sha1") {
        aria = "sha-1";
    }
    else if (type == "md5sum" || type == "md5") {
        aria = "md5";
    }
    if (!aria) {
        return {};
    }
    return std::string(aria) + "=" + item.hash;
}

bool parse_status_line(const std::string& line, StatusLine& out) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    const auto kind = line.substr(0, colon);
    if (kind == "pmstatus") {
        out.kind = StatusLine::Kind::PmStatus;
    }
    else if (kind == "dlstatus") {
        out.kind = StatusLine::Kind::DlStatus;
    }
    else if (kind == "pmerror") {
        out.kind = StatusLine::Kind::PmError;
    }
    else if (kind == "pmconffile") {
        out.kind = StatusLine::Kind::PmConffile;
    }
    else {
        return false;
    }

    // item may itself contain ':' (e.g. "libc6:arm64"), so the percent is the first field
    // after it that parses as a number; everything after that is the message.
    std::vector<std::string> fields;
    size_t start = colon + 1;
    while (true) {
        const auto next = line.find(':', start);
        if (next == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, next - start));
        start = next + 1;
    }

    for (size_t i = 1; i < fields.size(); ++i) {
        double value = 0;
        if (!parse_number(fields[i], value)) {
            continue;
        }
        out.item.clear();
        for (size_t j = 0; j < i; ++j) {
            if (j) {
                out.item += ':';
            }
            out.item += fields[j];
        }
        out.percent = value;
        out.message.clear();
        for (size_t j = i + 1; j < fields.size(); ++j) {
            if (j > i + 1) {
                out.message += ':';
            }
            out.message += fields[j];
        }
        return true;
    }
    return false;
}

std::string format_bytes(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024ULL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    }
    else if (bytes >= 1024ULL * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    }
    else if (bytes >= 1024) {
        std::snprintf(buf, sizeof(buf), "%.0f kB", bytes / 1024.0);
    }
    else {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return buf;
}

} // namespace updater
