/*
 * SPDX-License-Identifier: MIT
 */

#include "process.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace updater {
namespace {

using Clock = std::chrono::steady_clock;

void close_fd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void write_all(int fd, const std::string& data) {
    size_t written = 0;
    while (written < data.size()) {
        const auto n = ::write(fd, data.data() + written, data.size() - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        written += static_cast<size_t>(n);
    }
}

void flush_lines(std::string& buffer, const std::function<void(const std::string&)>& on_line,
                 bool final_flush) {
    size_t start = 0;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '\n' || buffer[i] == '\r') {
            if (i > start && on_line) {
                on_line(buffer.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    buffer.erase(0, start);
    if (final_flush && !buffer.empty()) {
        if (on_line) {
            on_line(buffer);
        }
        buffer.clear();
    }
}

} // namespace

int run_process(const RunOptions& options) {
    if (options.argv.empty()) {
        return kExecFailed;
    }

    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (::pipe2(in_pipe, O_CLOEXEC) != 0) {
        return kExecFailed;
    }
    if (::pipe2(out_pipe, O_CLOEXEC) != 0) {
        close_fd(in_pipe[0]);
        close_fd(in_pipe[1]);
        return kExecFailed;
    }

    std::vector<char*> argv;
    argv.reserve(options.argv.size() + 1);
    for (const auto& arg : options.argv) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) {
        close_fd(in_pipe[0]);
        close_fd(in_pipe[1]);
        close_fd(out_pipe[0]);
        close_fd(out_pipe[1]);
        return kExecFailed;
    }

    if (pid == 0) {
        // Child: own session (and process group) so cancel can stop the whole tree
        // (sudo -> apt -> dpkg), and no controlling terminal for maintainer scripts.
        ::setsid();
        ::dup2(in_pipe[0], STDIN_FILENO);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(out_pipe[1], STDERR_FILENO);
        for (const auto& [name, value] : options.env) {
            ::setenv(name.c_str(), value.c_str(), 1);
        }
        ::execvp(argv[0], argv.data());
        ::_exit(kExecFailed);
    }

    close_fd(in_pipe[0]);
    close_fd(out_pipe[1]);

    // Small payloads (a password line) fit the pipe buffer; write then close for EOF.
    write_all(in_pipe[1], options.stdin_data);
    close_fd(in_pipe[1]);

    std::string buffer;
    char chunk[4096];
    bool cancelled = false;
    bool term_sent = false;
    auto term_sent_at = Clock::now();
    auto last_tick = Clock::now();

    while (out_pipe[0] >= 0) {
        if (options.cancel && options.cancel->load() && !term_sent) {
            ::kill(-pid, SIGTERM);
            term_sent = true;
            term_sent_at = Clock::now();
            cancelled = true;
        }
        if (term_sent && Clock::now() - term_sent_at > std::chrono::seconds(5)) {
            ::kill(-pid, SIGKILL);
        }

        pollfd pfd{out_pipe[0], POLLIN, 0};
        const int ready = ::poll(&pfd, 1, options.tick_ms);
        if (ready > 0) {
            const auto n = ::read(out_pipe[0], chunk, sizeof(chunk));
            if (n > 0) {
                buffer.append(chunk, static_cast<size_t>(n));
                flush_lines(buffer, options.on_line, false);
            }
            else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                close_fd(out_pipe[0]);
            }
        }

        if (options.on_tick &&
            Clock::now() - last_tick >= std::chrono::milliseconds(options.tick_ms)) {
            last_tick = Clock::now();
            options.on_tick();
        }
    }
    flush_lines(buffer, options.on_line, true);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (options.on_tick) {
        options.on_tick();
    }

    if (cancelled) {
        return kCancelled;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return -WTERMSIG(status);
    }
    return kExecFailed;
}

int run_capture(const std::vector<std::string>& argv, std::string& output,
                const std::atomic<bool>* cancel) {
    output.clear();
    RunOptions options;
    options.argv = argv;
    options.cancel = cancel;
    options.on_line = [&output](const std::string& line) {
        output += line;
        output += '\n';
    };
    return run_process(options);
}

} // namespace updater
