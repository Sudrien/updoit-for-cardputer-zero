/*
 * SPDX-License-Identifier: MIT
 */

// Headless driver for UpgradeJob, for testing on a host or over SSH on the device:
//   updoit-cli [upgrade|full-upgrade|autoremove|refresh|clean|download] [--yes] [--plan-only]

#include "upgrade_job.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

using namespace updater;

int main(int argc, char** argv) {
    Operation op = Operation::Upgrade;
    bool yes = false;
    bool plan_only = false;
    std::string limit;
    int cancel_after_ms = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "upgrade") op = Operation::Upgrade;
        else if (a == "full-upgrade") op = Operation::FullUpgrade;
        else if (a == "autoremove") op = Operation::Autoremove;
        else if (a == "refresh") op = Operation::Refresh;
        else if (a == "clean") op = Operation::Clean;
        else if (a == "download") op = Operation::Download;
        else if (a == "--yes") yes = true;
        else if (a == "--plan-only") plan_only = true;
        else if (a.rfind("--limit=", 0) == 0) limit = a.substr(8);
        else if (a.rfind("--cancel-after-ms=", 0) == 0) cancel_after_ms = std::stoi(a.substr(18));
        else {
            std::fprintf(stderr, "usage: %s [upgrade|full-upgrade|autoremove|refresh|clean|download] [--yes] [--plan-only] [--limit=500K] [--cancel-after-ms=N]\n", argv[0]);
            return 2;
        }
    }

    Privilege privilege;
    privilege.detect();
    if (!plan_only && privilege.needs_password()) {
        std::string password;
        std::fprintf(stderr, "sudo password: ");
        std::getline(std::cin, password);
        if (!privilege.authenticate(password)) {
            std::fprintf(stderr, "authentication failed\n");
            return 1;
        }
    }

    JobConfig config;
    config.max_download_rate = limit;
    UpgradeJob job(privilege, config);
    auto download_started = std::chrono::steady_clock::time_point{};
    job.start(op);

    size_t printed_log = 0;
    uint32_t last_rev = 0;
    Phase last_phase = Phase::Idle;
    while (true) {
        const auto s = job.snapshot();
        if (s.revision != last_rev) {
            last_rev = s.revision;
            // print new log lines (approximate: tail may have rolled)
            if (s.log_tail.size() < printed_log) printed_log = 0;
            for (size_t i = printed_log; i < s.log_tail.size(); ++i) {
                std::printf("  | %s\n", s.log_tail[i].c_str());
            }
            printed_log = s.log_tail.size();
            if (s.phase != last_phase) {
                std::printf("[%s]\n", phase_label(s.phase));
                last_phase = s.phase;
            }
            if (s.phase == Phase::Downloading && download_started.time_since_epoch().count() == 0) {
                download_started = std::chrono::steady_clock::now();
            }
            if (s.phase == Phase::Downloading) {
                std::printf("  dl %d/%d files %s/%s", s.download.files_complete,
                            s.download.files_total, format_bytes(s.download.done_bytes).c_str(),
                            format_bytes(s.download.total_bytes).c_str());
                for (const auto& f : s.download.active) {
                    std::printf(" [%s %d%%]", f.name.substr(0, 18).c_str(),
                                f.size ? static_cast<int>(f.done * 100 / f.size) : 0);
                }
                std::printf("\n");
            }
            if (s.phase == Phase::Installing && !s.install_status.empty()) {
                std::printf("  %5.1f%% %s\n", s.install_percent, s.install_status.c_str());
            }
        }

        if (s.phase == Phase::AwaitingConfirm) {
            std::printf("%d upgrade, %d new, %d remove, %d kept back, %s to download\n",
                        s.upgrades, s.new_installs, s.removals, s.kept_back,
                        format_bytes(s.download_bytes).c_str());
            for (const auto& line : job.plan_lines()) std::printf("  %s\n", line.c_str());
            if (plan_only) return 0;
            if (!yes) {
                std::printf("Proceed? [y/N] ");
                std::string answer;
                std::getline(std::cin, answer);
                if (answer != "y" && answer != "Y") {
                    job.cancel();
                    return 0;
                }
            }
            job.confirm();
        }
        if (cancel_after_ms >= 0 && s.phase == Phase::Downloading &&
            std::chrono::steady_clock::now() - download_started >
                std::chrono::milliseconds(cancel_after_ms)) {
            job.cancel();
            cancel_after_ms = -1;
        }
        if (s.phase == Phase::Done) return 0;
        if (s.phase == Phase::Failed || s.phase == Phase::Cancelled) {
            std::fprintf(stderr, "%s: %s\n", phase_label(s.phase), s.error.c_str());
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
