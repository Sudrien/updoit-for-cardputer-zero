/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

namespace updater {

// How privileged commands are run: directly as root, via passwordless sudo, or via
// `sudo -k -S` with a password that is kept only in memory.
class Privilege {
public:
    enum class Mode { Root, SudoNoPassword, SudoPassword, Unavailable };

    Privilege();
    ~Privilege();

    Privilege(const Privilege&) = delete;
    Privilege& operator=(const Privilege&) = delete;

    // Detects root / passwordless sudo. Call once at startup (runs `sudo -n true`).
    void detect();

    Mode mode() const { return mode_; }
    bool needs_password() const { return mode_ == Mode::SudoPassword && password_.empty(); }
    bool ready() const;

    // Checks the password with sudo. On success it is stored for later commands.
    bool authenticate(const std::string& password);
    void forget();

    // Wraps argv so it runs as root. `stdin_data` receives what must be written to stdin.
    std::vector<std::string> wrap(const std::vector<std::string>& argv,
                                  std::string& stdin_data) const;

private:
    Mode mode_ = Mode::Unavailable;
    std::string password_;
};

} // namespace updater
