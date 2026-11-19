/*
 * SPDX-License-Identifier: MIT
 */

#include "status_cache.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace updater {

StatusCache parse_status_cache(const std::string& text) {
    StatusCache cache;
    std::istringstream in(text);
    std::string line;
    bool has_count = false;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const auto key = line.substr(0, eq);
        const auto value = line.substr(eq + 1);
        if (key == "checked") {
            cache.checked = std::strtoll(value.c_str(), nullptr, 10);
        }
        else if (key == "upgradable") {
            cache.upgradable = std::atoi(value.c_str());
            has_count = true;
        }
        else if (key == "packages") {
            std::istringstream words(value);
            std::string word;
            while (words >> word) {
                cache.packages.push_back(word);
            }
        }
        else if (key == "error") {
            cache.error = value;
        }
    }
    cache.valid = has_count;
    return cache;
}

StatusCache read_status_cache(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream text;
    text << file.rdbuf();
    return parse_status_cache(text.str());
}

} // namespace updater
