/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace updater {

constexpr const char* kStatusCachePath = "/run/updoit/apt_updates";

// Written by updoit-check (root, systemd timer). Format, one key=value per line:
//   checked=<unix seconds>
//   upgradable=<count>
//   packages=<space separated names>
//   error=<message, optional>
struct StatusCache {
    bool valid = false;
    int64_t checked = 0;
    int upgradable = 0;
    std::vector<std::string> packages;
    std::string error;
};

StatusCache parse_status_cache(const std::string& text);
StatusCache read_status_cache(const std::string& path = kStatusCachePath);

} // namespace updater
