/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace updater {

constexpr const char* kStatusCachePath = "/etc/updoit.status";

// Written by updoit-check (root, systemd timer). Format, one key=value per line:
//   checked=<unix seconds>            when the counts were made
//   updated=<unix seconds, optional>  last successful apt-get update
//   upgradable=<count>                apt-get upgrade
//   full_upgradable=<count>           apt-get dist-upgrade
//   autoremovable=<count>             apt-get autoremove
//   cache_bytes=<bytes, optional>     size of /var/cache/apt/archives (apt-get clean)
//   packages=<space separated names>
//   error=<message, optional>
struct StatusCache {
    bool valid = false;
    int64_t checked = 0;
    int64_t updated = 0; // 0 = unknown
    int upgradable = 0;
    int full_upgradable = 0;
    int autoremovable = 0;
    bool has_cache_bytes = false;
    uint64_t cache_bytes = 0;
    std::vector<std::string> packages;
    std::string error;
};

StatusCache parse_status_cache(const std::string& text);
StatusCache read_status_cache(const std::string& path = kStatusCachePath);

} // namespace updater
