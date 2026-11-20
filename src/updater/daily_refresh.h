/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace updater {

class Privilege;

// APT's daily list refresh: the OS apt-daily.timer only runs "apt update" when
// APT::Periodic::Update-Package-Lists is non-zero. Updoit's switch for it is
// kDailyRefreshConf, named to sort after the stock 10periodic / 20auto-upgrades
// files so its value wins.
inline constexpr const char* kDailyRefreshConf =
    "/etc/apt/apt.conf.d/99updoit-for-cardputer-zero";

// Effective setting, as apt-config reports it (no root needed).
bool daily_refresh_enabled();

// Writes kDailyRefreshConf as root. Returns false and sets `error` on failure.
bool set_daily_refresh(const Privilege& privilege, bool enabled, std::string& error);

} // namespace updater
