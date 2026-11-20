/*
 * SPDX-License-Identifier: MIT
 */

#include "daily_refresh.h"

#include "privilege.h"
#include "process.h"

namespace updater {

bool daily_refresh_enabled() {
    // Prints e.g.: V='1'   (or nothing when unset)
    std::string out;
    if (run_capture({"apt-config", "shell", "V", "APT::Periodic::Update-Package-Lists"}, out) != 0) {
        return false;
    }
    const auto q = out.find('\'');
    if (q == std::string::npos) {
        return false;
    }
    // Any non-zero number of days means the refresh is on.
    for (size_t i = q + 1; i < out.size() && out[i] != '\''; ++i) {
        if (out[i] >= '1' && out[i] <= '9') {
            return true;
        }
    }
    return false;
}

bool set_daily_refresh(const Privilege& privilege, bool enabled, std::string& error) {
    const std::string comment =
        "// Written by Updoit (updoit-for-cardputer-zero): daily apt list refresh on/off.";
    const std::string setting = std::string("APT::Periodic::Update-Package-Lists \"") +
                                (enabled ? "1" : "0") + "\";";
    // Write a temp file apt ignores (.dpkg-tmp), then rename it into place.
    std::vector<std::string> argv = {
        "sh", "-c",
        "umask 022 && printf '%s\\n' \"$1\" \"$2\" >\"$3.dpkg-tmp\" && mv -f \"$3.dpkg-tmp\" \"$3\"",
        "sh", comment, setting, kDailyRefreshConf,
    };
    RunOptions run;
    run.argv = privilege.wrap(argv, run.stdin_data);
    std::string output;
    run.on_line = [&output](const std::string& line) {
        if (!output.empty()) {
            output += "\n";
        }
        output += line;
    };
    const int code = run_process(run);
    if (code != 0) {
        error = output.empty() ? "Could not write " + std::string(kDailyRefreshConf) : output;
        return false;
    }
    if (daily_refresh_enabled() != enabled) {
        error = "Wrote the setting, but apt still reports it as " +
                std::string(enabled ? "off" : "on") + ".";
        return false;
    }
    return true;
}

} // namespace updater
