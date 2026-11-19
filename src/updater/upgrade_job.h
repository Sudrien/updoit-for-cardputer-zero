/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "apt_parse.h"
#include "downloader.h"
#include "privilege.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace updater {

enum class Phase {
    Idle,
    Planning,        // apt-get -s / --print-uris (unprivileged)
    AwaitingConfirm, // plan ready, waiting for confirm() or cancel()
    Downloading,     // aria2c, parallel
    Installing,      // apt-get <verb> as root
    Done,
    Failed,
    Cancelled,
};

const char* phase_label(Phase phase);

struct JobSnapshot {
    Phase phase = Phase::Idle;
    Operation operation = Operation::Upgrade;
    uint32_t revision = 0;

    // Plan summary (valid from AwaitingConfirm on)
    int upgrades = 0;
    int new_installs = 0;
    int removals = 0;
    int kept_back = 0;
    uint64_t download_bytes = 0;  // total size of files to fetch
    uint64_t freed_bytes = 0;     // Clean caches: space released
    int downloaded_count = 0;     // Download updates: packages fetched

    DownloadProgress download;

    double install_percent = 0;   // 0..100 from apt Status-Fd
    std::string install_status;   // e.g. "Unpacking libc6 (arm64)"

    std::vector<std::string> log_tail;
    std::string error;
    bool can_cancel = false;
};

struct JobConfig {
    std::string download_dir;           // user-writable staging dir for .deb files
    std::string apt_archives = "/var/cache/apt/archives";
    std::string status_helper = "/usr/libexec/updoit/updoit-check";
    int parallel_files = 4;
    int connections_per_file = 2;
    std::string max_download_rate; // aria2 syntax ("500K"); empty = unlimited
    size_t log_lines = 200;
};

// Runs one apt operation on a worker thread. The UI polls snapshot(); no callbacks into the
// UI thread are made.
class UpgradeJob {
public:
    UpgradeJob(Privilege& privilege, JobConfig config);
    ~UpgradeJob();

    UpgradeJob(const UpgradeJob&) = delete;
    UpgradeJob& operator=(const UpgradeJob&) = delete;

    // Starts planning (or, for Refresh, runs it straight away).
    bool start(Operation op);
    // Proceeds from AwaitingConfirm.
    bool confirm();
    // Cancels planning/downloading/confirmation. Ignored while installing.
    void cancel();
    // Back to Idle once finished.
    void reset();

    JobSnapshot snapshot() const;
    // Package lines for the confirm screen, e.g. "libc6 2.36-9 -> 2.36-9+deb12u4".
    std::vector<std::string> plan_lines() const;

private:
    void join();
    void plan_worker(Operation op);
    void execute_worker(Operation op);

    bool run_privileged(const std::vector<std::string>& argv, bool track_status);
    void set_phase(Phase phase);
    void fail(const std::string& message);
    void log(const std::string& line);
    void bump();

    Privilege& privilege_;
    JobConfig config_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};

    mutable std::mutex mutex_;
    JobSnapshot state_;
    std::deque<std::string> log_;
    Plan plan_;
    std::vector<DownloadItem> downloads_;
};

// Default staging directory: $XDG_CACHE_HOME/updoit/archives, ~/.cache/updoit/archives,
// or /var/cache/updoit/archives for root.
std::string default_download_dir();

} // namespace updater
