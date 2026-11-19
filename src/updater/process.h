/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace updater {

struct RunOptions {
    std::vector<std::string> argv;
    // Written to the child's stdin, then stdin is closed.
    std::string stdin_data;
    // Extra environment variables (name, value) on top of the inherited environment.
    std::vector<std::pair<std::string, std::string>> env;
    // Called for every line of combined stdout/stderr. '\r' also ends a line.
    std::function<void(const std::string&)> on_line;
    // Called roughly every tick_ms while the child runs.
    std::function<void()> on_tick;
    int tick_ms = 250;
    // When set to true, the child's process group gets SIGTERM, then SIGKILL.
    const std::atomic<bool>* cancel = nullptr;
};

constexpr int kExecFailed = 127;
constexpr int kCancelled = -1000;

// Runs a child process to completion. Returns its exit code, -signal if it was killed by a
// signal, kExecFailed if it could not be started, or kCancelled if `cancel` stopped it.
int run_process(const RunOptions& options);

// Convenience: run and capture all output (stdout+stderr) into `output`.
int run_capture(const std::vector<std::string>& argv, std::string& output,
                const std::atomic<bool>* cancel = nullptr);

} // namespace updater
