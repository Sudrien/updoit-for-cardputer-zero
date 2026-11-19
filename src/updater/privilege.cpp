/*
 * SPDX-License-Identifier: MIT
 */

#include "privilege.h"

#include "process.h"

#include <cstring>
#include <unistd.h>

namespace updater {
namespace {

void wipe(std::string& s) {
    // volatile write so the compiler cannot drop it
    volatile char* p = s.empty() ? nullptr : &s[0];
    for (size_t i = 0; i < s.size(); ++i) {
        p[i] = 0;
    }
    s.clear();
    s.shrink_to_fit();
}

} // namespace

Privilege::Privilege() = default;

Privilege::~Privilege() {
    forget();
}

void Privilege::detect() {
    if (::geteuid() == 0) {
        mode_ = Mode::Root;
        return;
    }
    std::string output;
    const int code = run_capture({"sudo", "-n", "true"}, output);
    if (code == 0) {
        mode_ = Mode::SudoNoPassword;
    }
    else if (code == kExecFailed) {
        mode_ = Mode::Unavailable;
    }
    else {
        mode_ = Mode::SudoPassword;
    }
}

bool Privilege::ready() const {
    switch (mode_) {
        case Mode::Root:
        case Mode::SudoNoPassword:
            return true;
        case Mode::SudoPassword:
            return !password_.empty();
        case Mode::Unavailable:
            return false;
    }
    return false;
}

bool Privilege::authenticate(const std::string& password) {
    if (mode_ != Mode::SudoPassword) {
        return ready();
    }
    RunOptions run;
    run.argv = {"sudo", "-k", "-S", "-p", "", "--", "true"};
    run.stdin_data = password + "\n";
    const int code = run_process(run);
    wipe(run.stdin_data);
    if (code != 0) {
        return false;
    }
    password_ = password;
    return true;
}

void Privilege::forget() {
    wipe(password_);
}

std::vector<std::string> Privilege::wrap(const std::vector<std::string>& argv,
                                         std::string& stdin_data) const {
    stdin_data.clear();
    std::vector<std::string> out;
    switch (mode_) {
        case Mode::Root:
            return argv;
        case Mode::SudoNoPassword:
            out = {"sudo", "-n", "--"};
            break;
        case Mode::SudoPassword:
            // -k: always read the password from stdin, so it is consumed by sudo and never
            // reaches the wrapped command.
            out = {"sudo", "-k", "-S", "-p", "", "--"};
            stdin_data = password_ + "\n";
            break;
        case Mode::Unavailable:
            return argv;
    }
    out.insert(out.end(), argv.begin(), argv.end());
    return out;
}

} // namespace updater
