/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace updater {

enum class Operation {
    Upgrade,     // apt-get upgrade
    FullUpgrade, // apt-get dist-upgrade
    Autoremove,  // apt-get autoremove
    Refresh,     // apt-get update
};

const char* operation_label(Operation op);
const char* operation_apt_verb(Operation op);
bool operation_downloads(Operation op);

// ---- `apt-get -s <verb>` -------------------------------------------------------------

struct PlanAction {
    enum class Kind { Install, Configure, Remove, Purge };
    Kind kind = Kind::Install;
    std::string package;
    std::string old_version; // empty for new installs
    std::string new_version; // empty for removals
};

struct Plan {
    std::vector<PlanAction> actions;
    std::vector<std::string> kept_back;

    int upgrades() const;      // Inst with an old version
    int new_installs() const;  // Inst without an old version
    int removals() const;      // Remv + Purg
    bool empty() const { return upgrades() + new_installs() + removals() == 0; }
};

// Needs LC_ALL=C output.
Plan parse_simulation(const std::string& output);

// ---- `apt-get -qq --print-uris <verb>` -----------------------------------------------

struct DownloadItem {
    std::string uri;
    std::string filename;
    uint64_t size = 0;
    std::string hash_type; // as printed by apt, e.g. "SHA256", "MD5Sum"
    std::string hash;
};

std::vector<DownloadItem> parse_print_uris(const std::string& output);

// aria2c `checksum=` value ("sha-256=<hex>"), or empty if the hash type is unknown.
std::string aria2_checksum(const DownloadItem& item);

// ---- apt `-o APT::Status-Fd` lines -----------------------------------------------------

struct StatusLine {
    enum class Kind { PmStatus, DlStatus, PmError, PmConffile, Other };
    Kind kind = Kind::Other;
    std::string item;   // package name, or file index for dlstatus
    double percent = 0; // 0..100
    std::string message;
};

// Returns false for lines that are not status lines (normal apt/dpkg output).
bool parse_status_line(const std::string& line, StatusLine& out);

// ---- misc ---------------------------------------------------------------------------

std::string format_bytes(uint64_t bytes);

} // namespace updater
