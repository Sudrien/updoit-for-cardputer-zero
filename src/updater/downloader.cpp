/*
 * SPDX-License-Identifier: MIT
 */

#include "downloader.h"

#include "process.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace updater {
namespace fs = std::filesystem;

namespace {

uint64_t file_size_or_zero(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(size);
}

// Bytes actually written. aria2 downloads split segments at their offsets, so the file
// size reaches the end long before the data does; allocated blocks track real progress.
uint64_t written_bytes(const fs::path& path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_blocks) * 512;
}

bool path_exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// Complete = expected size reached and aria2's ".aria2" control file is gone.
bool is_complete(const fs::path& path, uint64_t size) {
    return file_size_or_zero(path) == size && !path_exists(fs::path(path.string() + ".aria2"));
}

} // namespace

Downloader::Downloader(DownloaderOptions options) : options_(std::move(options)) {}

DownloadProgress Downloader::poll(const std::vector<DownloadItem>& items) const {
    DownloadProgress progress;
    progress.files_total = static_cast<int>(items.size());
    const fs::path dir(options_.directory);
    for (const auto& item : items) {
        const auto path = dir / item.filename;
        progress.total_bytes += item.size;
        if (is_complete(path, item.size)) {
            progress.done_bytes += item.size;
            ++progress.files_complete;
            continue;
        }
        if (path_exists(fs::path(path.string() + ".aria2"))) {
            DownloadFileState state;
            state.name = item.filename;
            state.size = item.size;
            state.done = std::min(written_bytes(path), item.size);
            progress.done_bytes += state.done;
            progress.active.push_back(std::move(state));
        }
    }
    return progress;
}

bool Downloader::run(const std::vector<DownloadItem>& items,
                     const std::atomic<bool>* cancel,
                     const std::function<void(const DownloadProgress&)>& on_progress,
                     const std::function<void(const std::string&)>& on_log,
                     std::string& error) {
    files_.clear();
    const fs::path dir(options_.directory);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        error = "cannot create " + dir.string() + ": " + ec.message();
        return false;
    }

    // Only hand aria2 the files that are not already complete (resumes are kept).
    std::vector<const DownloadItem*> pending;
    for (const auto& item : items) {
        files_.push_back((dir / item.filename).string());
        if (!is_complete(dir / item.filename, item.size)) {
            pending.push_back(&item);
        }
    }

    if (on_progress) {
        on_progress(poll(items));
    }
    if (pending.empty()) {
        return true;
    }

    const auto input_path = dir / "updoit-aria2-input.txt";
    {
        std::ofstream input(input_path, std::ios::trunc);
        if (!input) {
            error = "cannot write " + input_path.string();
            return false;
        }
        for (const auto* item : pending) {
            input << item->uri << '\n';
            input << "  out=" << item->filename << '\n';
            const auto checksum = aria2_checksum(*item);
            if (!checksum.empty()) {
                input << "  checksum=" << checksum << '\n';
            }
        }
    }

    const int conns = options_.connections_per_file < 1 ? 1 : options_.connections_per_file;
    RunOptions run;
    run.argv = {
        options_.aria2c,
        "--input-file=" + input_path.string(),
        "--dir=" + dir.string(),
        "--max-concurrent-downloads=" + std::to_string(options_.parallel_files),
        "--max-connection-per-server=" + std::to_string(conns),
        "--split=" + std::to_string(conns),
        "--min-split-size=1M",
        "--continue=true",
        "--auto-file-renaming=false",
        "--allow-overwrite=true",
        "--file-allocation=none",
        "--check-integrity=true",
        "--max-tries=3",
        "--retry-wait=2",
        "--console-log-level=warn",
        "--summary-interval=0",
        "--download-result=hide",
        "--show-console-readout=false",
        "--enable-color=false",
    };
    if (!options_.proxy.empty()) {
        run.argv.push_back("--all-proxy=" + options_.proxy);
    }
    if (!options_.max_rate.empty()) {
        run.argv.push_back("--max-overall-download-limit=" + options_.max_rate);
    }
    run.cancel = cancel;
    run.on_line = on_log;
    run.on_tick = [&]() {
        if (on_progress) {
            on_progress(poll(items));
        }
    };

    const int code = run_process(run);
    fs::remove(input_path, ec);

    if (code == kCancelled) {
        error = "cancelled";
        return false;
    }
    if (code == kExecFailed) {
        error = "aria2c not found (install the aria2 package)";
        return false;
    }

    const auto final_state = poll(items);
    if (on_progress) {
        on_progress(final_state);
    }
    if (code != 0 || final_state.files_complete != final_state.files_total) {
        std::ostringstream msg;
        msg << "download failed (aria2c exit " << code << ", " << final_state.files_complete
            << "/" << final_state.files_total << " files)";
        error = msg.str();
        return false;
    }
    return true;
}

std::string apt_http_proxy() {
    std::string output;
    if (run_capture({"apt-config", "shell", "PROXY", "Acquire::http::Proxy"}, output) != 0) {
        return {};
    }
    // Output looks like: PROXY='http://host:3142/'
    const auto a = output.find('\'');
    const auto b = output.rfind('\'');
    if (a == std::string::npos || b <= a) {
        return {};
    }
    auto proxy = output.substr(a + 1, b - a - 1);
    if (proxy == "DIRECT" || proxy == "false") {
        return {};
    }
    return proxy;
}

} // namespace updater
