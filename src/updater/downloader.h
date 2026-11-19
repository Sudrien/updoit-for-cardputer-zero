/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "apt_parse.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace updater {

struct DownloadFileState {
    std::string name; // package file name
    uint64_t done = 0;
    uint64_t size = 0;
    bool complete = false;
};

struct DownloadProgress {
    uint64_t done_bytes = 0;
    uint64_t total_bytes = 0;
    int files_complete = 0;
    int files_total = 0;
    std::vector<DownloadFileState> active; // files currently transferring
};

struct DownloaderOptions {
    std::string directory;      // where .deb files are written
    int parallel_files = 4;     // aria2c -j
    int connections_per_file = 2; // aria2c -x / -s
    std::string proxy;          // optional, passed as --all-proxy
    std::string max_rate;       // optional aria2 rate limit, e.g. "500K"; empty = unlimited
    std::string aria2c = "aria2c";
};

// Downloads apt's package list in parallel with aria2c. Progress is measured by polling
// file sizes, so it does not depend on aria2c's console format.
class Downloader {
public:
    explicit Downloader(DownloaderOptions options);

    // Blocks until finished. Returns true when every file is present with the expected size.
    bool run(const std::vector<DownloadItem>& items,
             const std::atomic<bool>* cancel,
             const std::function<void(const DownloadProgress&)>& on_progress,
             const std::function<void(const std::string&)>& on_log,
             std::string& error);

    // Full paths of the files from the last run().
    const std::vector<std::string>& files() const { return files_; }

private:
    DownloadProgress poll(const std::vector<DownloadItem>& items) const;

    DownloaderOptions options_;
    std::vector<std::string> files_;
};

// Reads apt's configured HTTP proxy (Acquire::http::Proxy), or empty.
std::string apt_http_proxy();

} // namespace updater
