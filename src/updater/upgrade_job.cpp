/*
 * SPDX-License-Identifier: MIT
 */

#include "upgrade_job.h"

#include "process.h"

#include <cstdlib>
#include <filesystem>
#include <sys/statvfs.h>
#include <unistd.h>

namespace updater {
namespace fs = std::filesystem;

namespace {

// Common prefix for every apt-get call: C locale for parsing, no prompts.
std::vector<std::string> apt_env() {
    return {"env", "LC_ALL=C", "LANG=C", "DEBIAN_FRONTEND=noninteractive", "NEEDRESTART_MODE=a"};
}

std::vector<std::string> concat(std::vector<std::string> a, const std::vector<std::string>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

void wipe(std::string& s) {
    volatile char* p = s.empty() ? nullptr : &s[0];
    for (size_t i = 0; i < s.size(); ++i) {
        p[i] = 0;
    }
    s.clear();
}

uint64_t free_bytes(const std::string& path) {
    struct statvfs st {};
    if (::statvfs(path.c_str(), &st) != 0) {
        return UINT64_MAX;
    }
    return static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
}

} // namespace

const char* phase_label(Phase phase) {
    switch (phase) {
        case Phase::Idle:
            return "Idle";
        case Phase::Planning:
            return "Checking";
        case Phase::AwaitingConfirm:
            return "Confirm";
        case Phase::Downloading:
            return "Downloading";
        case Phase::Installing:
            return "Installing";
        case Phase::Done:
            return "Done";
        case Phase::Failed:
            return "Failed";
        case Phase::Cancelled:
            return "Cancelled";
    }
    return "";
}

std::string default_download_dir() {
    if (::geteuid() == 0) {
        return "/var/cache/updoit/archives";
    }
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && xdg[0] == '/') {
        return std::string(xdg) + "/updoit/archives";
    }
    if (const char* home = std::getenv("HOME"); home && home[0] == '/') {
        return std::string(home) + "/.cache/updoit/archives";
    }
    return "/tmp/updoit-archives";
}

UpgradeJob::UpgradeJob(Privilege& privilege, JobConfig config)
    : privilege_(privilege), config_(std::move(config)) {
    if (config_.download_dir.empty()) {
        config_.download_dir = default_download_dir();
    }
}

UpgradeJob::~UpgradeJob() {
    cancel_ = true;
    join();
}

void UpgradeJob::join() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool UpgradeJob::start(Operation op) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto p = state_.phase;
        if (p == Phase::Planning || p == Phase::Downloading || p == Phase::Installing ||
            p == Phase::AwaitingConfirm) {
            return false;
        }
    }
    join();
    cancel_ = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto rev = state_.revision;
        state_ = JobSnapshot{};
        state_.revision = rev + 1;
        state_.operation = op;
        log_.clear();
        plan_ = Plan{};
        downloads_.clear();
    }
    if (op == Operation::Refresh) {
        worker_ = std::thread(&UpgradeJob::execute_worker, this, op);
    }
    else {
        worker_ = std::thread(&UpgradeJob::plan_worker, this, op);
    }
    return true;
}

bool UpgradeJob::confirm() {
    Operation op;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_.phase != Phase::AwaitingConfirm) {
            return false;
        }
        op = state_.operation;
    }
    join();
    worker_ = std::thread(&UpgradeJob::execute_worker, this, op);
    return true;
}

void UpgradeJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.phase == Phase::AwaitingConfirm) {
        state_.phase = Phase::Cancelled;
        state_.can_cancel = false;
        ++state_.revision;
        return;
    }
    if (state_.can_cancel) {
        cancel_ = true;
    }
}

void UpgradeJob::reset() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto p = state_.phase;
        if (p != Phase::Done && p != Phase::Failed && p != Phase::Cancelled) {
            return;
        }
    }
    join();
    std::lock_guard<std::mutex> lock(mutex_);
    const auto rev = state_.revision;
    state_ = JobSnapshot{};
    state_.revision = rev + 1;
}

JobSnapshot UpgradeJob::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    JobSnapshot copy = state_;
    copy.log_tail.assign(log_.begin(), log_.end());
    return copy;
}

std::vector<std::string> UpgradeJob::plan_lines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> lines;
    for (const auto& a : plan_.actions) {
        switch (a.kind) {
            case PlanAction::Kind::Install:
                if (a.old_version.empty()) {
                    lines.push_back("+ " + a.package + " " + a.new_version);
                }
                else {
                    lines.push_back(a.package + " " + a.old_version + " > " + a.new_version);
                }
                break;
            case PlanAction::Kind::Remove:
            case PlanAction::Kind::Purge:
                lines.push_back("- " + a.package + " " + a.old_version);
                break;
            case PlanAction::Kind::Configure:
                break;
        }
    }
    for (const auto& k : plan_.kept_back) {
        lines.push_back("= " + k + " (kept back)");
    }
    return lines;
}

void UpgradeJob::bump() {
    ++state_.revision;
}

void UpgradeJob::set_phase(Phase phase) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.phase = phase;
    state_.can_cancel = phase == Phase::Planning || phase == Phase::Downloading;
    bump();
}

void UpgradeJob::fail(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.phase = cancel_ ? Phase::Cancelled : Phase::Failed;
    state_.error = message;
    state_.can_cancel = false;
    bump();
}

void UpgradeJob::log(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_.push_back(line);
    while (log_.size() > config_.log_lines) {
        log_.pop_front();
    }
    bump();
}

void UpgradeJob::plan_worker(Operation op) {
    set_phase(Phase::Planning);
    const std::string verb = operation_apt_verb(op);

    std::string sim_output;
    const int sim_code =
        run_capture(concat(apt_env(), {"apt-get", "-s", "-o", "Debug::NoLocking=1", verb}),
                    sim_output, &cancel_);
    if (sim_code == kCancelled) {
        fail("cancelled");
        return;
    }
    if (sim_code != 0) {
        // apt prints its reason as "E: ..." lines; surface them.
        std::string reason;
        size_t pos = 0;
        while ((pos = sim_output.find("E: ", pos)) != std::string::npos) {
            const auto end = sim_output.find('\n', pos);
            reason += sim_output.substr(pos, end - pos) + " ";
            pos = end == std::string::npos ? sim_output.size() : end;
        }
        fail(reason.empty() ? "apt-get -s " + verb + " failed" : reason);
        return;
    }
    Plan plan = parse_simulation(sim_output);

    std::vector<DownloadItem> items;
    if (operation_downloads(op) && !plan.empty()) {
        std::string uris;
        const int uri_code =
            run_capture(concat(apt_env(), {"apt-get", "-qq", "--print-uris", "-o",
                                           "Debug::NoLocking=1", verb}),
                        uris, &cancel_);
        if (uri_code == kCancelled) {
            fail("cancelled");
            return;
        }
        if (uri_code != 0) {
            fail("apt-get --print-uris " + verb + " failed");
            return;
        }
        items = parse_print_uris(uris);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    state_.upgrades = plan.upgrades();
    state_.new_installs = plan.new_installs();
    state_.removals = plan.removals();
    state_.kept_back = static_cast<int>(plan.kept_back.size());
    state_.download_bytes = 0;
    for (const auto& item : items) {
        state_.download_bytes += item.size;
    }
    plan_ = std::move(plan);
    downloads_ = std::move(items);
    state_.phase = plan_.empty() ? Phase::Done : Phase::AwaitingConfirm;
    if (plan_.empty()) {
        log_.push_back("Nothing to do.");
    }
    state_.can_cancel = false;
    bump();
}

bool UpgradeJob::run_privileged(const std::vector<std::string>& argv, bool track_status) {
    RunOptions run;
    run.argv = privilege_.wrap(argv, run.stdin_data);
    run.on_line = [this, track_status](const std::string& line) {
        StatusLine status;
        if (track_status && parse_status_line(line, status)) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status.kind == StatusLine::Kind::PmError) {
                log_.push_back("error: " + status.item + ": " + status.message);
            }
            else if (status.kind == StatusLine::Kind::PmConffile) {
                log_.push_back("conffile kept: " + status.item);
            }
            else {
                state_.install_percent = status.percent;
                state_.install_status = status.message;
            }
            bump();
            return;
        }
        log(line);
    };
    const int code = run_process(run);
    wipe(run.stdin_data);
    return code == 0;
}

void UpgradeJob::execute_worker(Operation op) {
    if (!privilege_.ready()) {
        fail("root access not available");
        return;
    }
    const std::string verb = operation_apt_verb(op);

    // 1. Parallel download, unprivileged, into the staging directory.
    std::vector<std::string> staged;
    std::vector<DownloadItem> items;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        items = downloads_;
    }
    if (operation_downloads(op) && !items.empty()) {
        set_phase(Phase::Downloading);

        uint64_t needed = 0;
        for (const auto& item : items) {
            needed += item.size;
        }
        std::error_code ec;
        fs::create_directories(config_.download_dir, ec);
        if (free_bytes(config_.download_dir) < needed + needed / 10) {
            fail("not enough free space for " + format_bytes(needed) + " of downloads");
            return;
        }

        DownloaderOptions options;
        options.directory = config_.download_dir;
        options.parallel_files = config_.parallel_files;
        options.connections_per_file = config_.connections_per_file;
        options.proxy = apt_http_proxy();
        options.max_rate = config_.max_download_rate;
        Downloader downloader(options);

        std::string error;
        const bool ok = downloader.run(
            items, &cancel_,
            [this](const DownloadProgress& progress) {
                std::lock_guard<std::mutex> lock(mutex_);
                state_.download = progress;
                bump();
            },
            [this](const std::string& line) { log(line); }, error);
        if (!ok) {
            fail(error);
            return;
        }
        staged = downloader.files();
        log("Downloaded " + std::to_string(items.size()) + " packages (" +
            format_bytes(needed) + ")");
    }

    set_phase(Phase::Installing);

    // 2. Move the verified files into apt's cache (same filesystem: a rename, no copy),
    //    so apt-get finds them and skips its own download.
    if (!staged.empty()) {
        auto mv = std::vector<std::string>{"mv", "-f", "--"};
        mv.insert(mv.end(), staged.begin(), staged.end());
        mv.push_back(config_.apt_archives + "/");
        if (!run_privileged(mv, false)) {
            log("could not move packages into the apt cache; apt will download them");
        }
        else {
            std::vector<std::string> chown = {"chown", "root:root", "--"};
            for (const auto& path : staged) {
                chown.push_back(config_.apt_archives + "/" + fs::path(path).filename().string());
            }
            run_privileged(chown, false);
        }
    }

    // 3. Install. apt re-verifies every hash before dpkg sees the files.
    //    Status goes to fd 3 (a dup of stdout): with APT::Status-Fd=1 apt closes stdout for
    //    dpkg, and maintainer scripts that echo then fail with "I/O error". sudo closes
    //    fds >= 3, so the dup is made by a shell running as root.
    std::string apt_cmd = "exec 3>&1; exec env LC_ALL=C LANG=C DEBIAN_FRONTEND=noninteractive "
                          "NEEDRESTART_MODE=a apt-get -y -o APT::Status-Fd=3 "
                          "-o Dpkg::Use-Pty=0 -o DPkg::Lock::Timeout=120 "
                          "-o Dpkg::Options::=--force-confdef "
                          "-o Dpkg::Options::=--force-confold ";
    apt_cmd += verb;
    if (!run_privileged({"sh", "-c", apt_cmd}, true)) {
        fail("apt-get " + verb + " failed; see log");
        return;
    }

    // 4. Refresh the cached "updates available" count for the launcher/menu.
    std::error_code ec;
    if (fs::exists(config_.status_helper, ec)) {
        // After "Refresh lists" the lists are new: have the helper stamp the update time.
        run_privileged({config_.status_helper, op == Operation::Refresh ? "--updated" : "--no-update"},
                       false);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    state_.phase = Phase::Done;
    state_.install_percent = 100;
    state_.can_cancel = false;
    bump();
}

} // namespace updater
