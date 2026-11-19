/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "status_cache.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <ctime>

namespace viewmodel {
namespace {

using updater::Operation;
using updater::Phase;

constexpr size_t kMaxPassword = 128;
constexpr size_t kMaskShown = 24;

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

int permille(uint64_t done, uint64_t total) {
    if (total == 0) {
        return 0;
    }
    const auto v = static_cast<int>(done * 1000 / total);
    return v > 1000 ? 1000 : v;
}

std::string age_text(int64_t checked) {
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    const int64_t age = now - checked;
    char buf[32];
    if (checked <= 0 || age < 0) {
        return "";
    }
    if (age < 90) {
        return "just now";
    }
    if (age < 90 * 60) {
        std::snprintf(buf, sizeof(buf), "%lld min ago", static_cast<long long>(age / 60));
    }
    else if (age < 36 * 3600) {
        std::snprintf(buf, sizeof(buf), "%lld h ago", static_cast<long long>(age / 3600));
    }
    else {
        std::snprintf(buf, sizeof(buf), "%lld d ago", static_cast<long long>(age / 86400));
    }
    return buf;
}

void wipe(std::string& s) {
    volatile char* p = s.empty() ? nullptr : &s[0];
    for (size_t i = 0; i < s.size(); ++i) {
        p[i] = 0;
    }
    s.clear();
}

updater::JobConfig job_config() {
    updater::JobConfig config;
    // Optional aria2 rate limit, e.g. UPDOIT_DOWNLOAD_LIMIT=500K
    if (const char* limit = std::getenv("UPDOIT_DOWNLOAD_LIMIT"); limit && *limit) {
        config.max_download_rate = limit;
    }
    return config;
}

constexpr Operation kMenuOps[model::kMenuItemCount] = {
    Operation::Upgrade,
    Operation::FullUpgrade,
    Operation::Autoremove,
    Operation::Refresh,
    Operation::Clean,
};

} // namespace

BaseViewModel::BaseViewModel()
    : job_(std::make_unique<updater::UpgradeJob>(privilege_, job_config())),
      title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false),
      password_mask_subject_(""),
      password_message_subject_("Enter your sudo password"),
      password_busy_subject_(false),
      menu_index_subject_(0),
      updates_text_subject_(""),
      menu_hint_subject_{reactive::StringSubject<24>(""), reactive::StringSubject<24>(""),
                         reactive::StringSubject<24>(""), reactive::StringSubject<24>(""),
                         reactive::StringSubject<24>("")},
      confirm_summary_subject_(""),
      progress_title_subject_(""),
      progress_detail_subject_(""),
      progress_value_subject_(0),
      file_name_subject_{reactive::StringSubject<64>(""), reactive::StringSubject<64>("")},
      file_value_subject_{reactive::IntSubject(-1), reactive::IntSubject(-1)},
      log_line_subject_(""),
      can_cancel_subject_(false),
      result_title_subject_(""),
      result_text_subject_(""),
      result_tone_subject_(0) {}

BaseViewModel::~BaseViewModel() {
    if (auth_future_.valid()) {
        auth_future_.wait();
    }
    wipe(password_input_);
    job_.reset(); // joins the worker before privilege_ goes away
}

// ---------------------------------------------------------------------------- lifecycle

void BaseViewModel::start() {
    privilege_.detect();
    refresh_updates_text();
    switch (privilege_.mode()) {
        case updater::Privilege::Mode::Unavailable:
            result_title_subject_.set("No root access");
            result_text_subject_.set("Updoit needs sudo (or to run as root) to install "
                                     "packages, and sudo was not found.");
            result_tone_subject_.set(1);
            show_page(model::AppPage::Result);
            break;
        case updater::Privilege::Mode::SudoPassword:
            show_page(model::AppPage::Password);
            break;
        default:
            show_page(model::AppPage::Menu);
            break;
    }
}

void BaseViewModel::tick() {
    if (auth_future_.valid() &&
        auth_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const bool ok = auth_future_.get();
        password_busy_subject_.set(false);
        if (ok) {
            password_message_subject_.set("Enter your sudo password");
            show_page(model::AppPage::Menu);
        }
        else {
            password_message_subject_.set("Wrong password, try again");
        }
    }

    const auto s = job_->snapshot();
    if (s.revision != job_revision_) {
        job_revision_ = s.revision;
        publish_job(s);
    }
}

void BaseViewModel::show_page(model::AppPage page) {
    model_.set_current_page(page);
    current_page_subject_.set(page_to_int(page));
}

// ---------------------------------------------------------------------------- shared

lv_subject_t* BaseViewModel::title_subject() { return title_subject_.native(); }
lv_subject_t* BaseViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }
lv_subject_t* BaseViewModel::current_page_subject() { return current_page_subject_.native(); }
lv_subject_t* BaseViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    dark_mode_subject_.set(model_.dark_mode());
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    dark_mode_subject_.set(model_.dark_mode());
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

bool BaseViewModel::can_quit() const {
    return last_phase_ != Phase::Installing;
}

void BaseViewModel::request_quit() {
    if (!can_quit()) {
        return;
    }
    job_->cancel();
    quit_requested_subject_.set(true);
}

void BaseViewModel::back() {
    switch (model_.current_page()) {
        case model::AppPage::Password:
        case model::AppPage::Menu:
            request_quit();
            break;
        case model::AppPage::Confirm:
            // Nothing has happened yet, so go straight back instead of a "Cancelled" page.
            job_->cancel();
            job_->reset();
            job_revision_ = job_->snapshot().revision;
            last_phase_ = Phase::Idle;
            show_page(model::AppPage::Menu);
            break;
        case model::AppPage::Progress:
            job_->cancel(); // ignored while installing
            break;
        case model::AppPage::Result:
            if (privilege_.mode() == updater::Privilege::Mode::Unavailable) {
                request_quit();
                break;
            }
            job_->reset();
            refresh_updates_text();
            show_page(model::AppPage::Menu);
            break;
    }
}

// ---------------------------------------------------------------------------- password

lv_subject_t* BaseViewModel::password_mask_subject() { return password_mask_subject_.native(); }
lv_subject_t* BaseViewModel::password_message_subject() { return password_message_subject_.native(); }
lv_subject_t* BaseViewModel::password_busy_subject() { return password_busy_subject_.native(); }

void BaseViewModel::publish_password_mask() {
    const size_t n = password_input_.size() < kMaskShown ? password_input_.size() : kMaskShown;
    password_mask_subject_.set(std::string(n, '*').c_str());
}

void BaseViewModel::password_append(char c) {
    if (password_busy_subject_.value() || password_input_.size() >= kMaxPassword) {
        return;
    }
    password_input_.push_back(c);
    publish_password_mask();
}

void BaseViewModel::password_backspace() {
    if (password_busy_subject_.value() || password_input_.empty()) {
        return;
    }
    password_input_.back() = 0;
    password_input_.pop_back();
    publish_password_mask();
}

void BaseViewModel::password_submit() {
    if (password_busy_subject_.value() || password_input_.empty()) {
        return;
    }
    password_busy_subject_.set(true);
    password_message_subject_.set("Checking...");
    // sudo waits ~2 s after a wrong password; keep the UI responsive.
    std::string attempt = password_input_;
    wipe(password_input_);
    publish_password_mask();
    auth_future_ = std::async(std::launch::async, [this, attempt]() mutable {
        const bool ok = privilege_.authenticate(attempt);
        wipe(attempt);
        return ok;
    });
}

// ---------------------------------------------------------------------------- menu

lv_subject_t* BaseViewModel::menu_index_subject() { return menu_index_subject_.native(); }
lv_subject_t* BaseViewModel::updates_text_subject() { return updates_text_subject_.native(); }

const char* BaseViewModel::menu_label(int index) {
    if (index < 0 || index >= model::kMenuItemCount) {
        return "";
    }
    return updater::operation_label(kMenuOps[index]);
}

lv_subject_t* BaseViewModel::menu_hint_subject(int index) {
    return menu_hint_subject_[index].native();
}

void BaseViewModel::refresh_updates_text() {
    const auto cache = updater::read_status_cache();
    std::string text;
    if (!cache.valid) {
        text = "Not checked yet - try Refresh lists";
    }
    else {
        if (cache.upgradable == 0) {
            text = "Up to date";
        }
        else {
            text = std::to_string(cache.upgradable) +
                   (cache.upgradable == 1 ? " update available" : " updates available");
        }
        const auto age = age_text(cache.checked);
        if (!age.empty()) {
            text += " - " + age;
        }
        if (!cache.error.empty()) {
            text = "Last check failed - " + age;
        }
    }
    updates_text_subject_.set(text.c_str());

    // Per-row hints: how many packages each operation would touch (cached by updoit-check).
    const auto count_text = [&](int n) {
        return !cache.valid ? std::string("?") : n == 0 ? std::string("none") : std::to_string(n) + " pkg" + (n == 1 ? "" : "s");
    };
    menu_hint_subject_[0].set(count_text(cache.upgradable).c_str());
    menu_hint_subject_[1].set(count_text(cache.full_upgradable).c_str());
    menu_hint_subject_[2].set(count_text(cache.autoremovable).c_str());
    const auto updated = age_text(cache.updated);
    menu_hint_subject_[3].set(updated.empty() ? "never updated" : updated.c_str());
    menu_hint_subject_[4].set(!cache.valid || !cache.has_cache_bytes
                                  ? "?"
                                  : (cache.cache_bytes == 0 ? "empty"
                                                            : updater::format_bytes(cache.cache_bytes).c_str()));
}

void BaseViewModel::menu_move(int delta) {
    model_.move_menu(delta);
    menu_index_subject_.set(model_.menu_index());
}

void BaseViewModel::menu_select(int index) {
    if (index >= 0) {
        model_.set_menu_index(index);
        menu_index_subject_.set(model_.menu_index());
    }
    const auto op = kMenuOps[model_.menu_index()];
    if (job_->start(op)) {
        // The first snapshot moves us on; show the progress page while planning.
        progress_title_subject_.set(op == Operation::Refresh ? "Refreshing lists"
                                    : op == Operation::Clean ? "Cleaning caches"
                                                             : "Checking");
        progress_detail_subject_.set(updater::operation_apt_verb(op));
        progress_value_subject_.set(0);
        log_line_subject_.set("");
        for (int i = 0; i < kFileSlots; ++i) {
            file_value_subject_[i].set(-1);
        }
        show_page(model::AppPage::Progress);
    }
}

// ---------------------------------------------------------------------------- confirm

lv_subject_t* BaseViewModel::confirm_summary_subject() { return confirm_summary_subject_.native(); }

std::vector<std::string> BaseViewModel::plan_lines() const {
    return job_->plan_lines();
}

void BaseViewModel::confirm() {
    job_->confirm();
}

// ---------------------------------------------------------------------------- progress / result

lv_subject_t* BaseViewModel::progress_title_subject() { return progress_title_subject_.native(); }
lv_subject_t* BaseViewModel::progress_detail_subject() { return progress_detail_subject_.native(); }
lv_subject_t* BaseViewModel::progress_value_subject() { return progress_value_subject_.native(); }
lv_subject_t* BaseViewModel::file_name_subject(int slot) { return file_name_subject_[slot].native(); }
lv_subject_t* BaseViewModel::file_value_subject(int slot) { return file_value_subject_[slot].native(); }
lv_subject_t* BaseViewModel::log_line_subject() { return log_line_subject_.native(); }
lv_subject_t* BaseViewModel::can_cancel_subject() { return can_cancel_subject_.native(); }
lv_subject_t* BaseViewModel::result_title_subject() { return result_title_subject_.native(); }
lv_subject_t* BaseViewModel::result_text_subject() { return result_text_subject_.native(); }
lv_subject_t* BaseViewModel::result_tone_subject() { return result_tone_subject_.native(); }

void BaseViewModel::publish_job(const updater::JobSnapshot& s) {
    const Phase previous = last_phase_;
    last_phase_ = s.phase;
    can_cancel_subject_.set(s.can_cancel);
    if (!s.log_tail.empty()) {
        log_line_subject_.set(s.log_tail.back().c_str());
    }

    char buf[128];
    switch (s.phase) {
        case Phase::Idle:
            break;

        case Phase::Planning:
            progress_title_subject_.set("Checking");
            break;

        case Phase::AwaitingConfirm: {
            std::string summary = std::to_string(s.upgrades) + " upgrade";
            if (s.new_installs) {
                summary += ", " + std::to_string(s.new_installs) + " new";
            }
            if (s.removals) {
                summary += ", " + std::to_string(s.removals) + " remove";
            }
            if (s.download_bytes) {
                summary += ", " + updater::format_bytes(s.download_bytes);
            }
            confirm_summary_subject_.set(summary.c_str());
            if (previous != Phase::AwaitingConfirm) {
                show_page(model::AppPage::Confirm);
            }
            break;
        }

        case Phase::Downloading: {
            const auto& d = s.download;
            std::snprintf(buf, sizeof(buf), "Downloading %d/%d", d.files_complete, d.files_total);
            progress_title_subject_.set(buf);
            progress_detail_subject_.set((updater::format_bytes(d.done_bytes) + " of " +
                                          updater::format_bytes(d.total_bytes))
                                             .c_str());
            progress_value_subject_.set(permille(d.done_bytes, d.total_bytes));
            for (int i = 0; i < kFileSlots; ++i) {
                if (i < static_cast<int>(d.active.size())) {
                    file_name_subject_[i].set(d.active[i].name.c_str());
                    file_value_subject_[i].set(permille(d.active[i].done, d.active[i].size));
                }
                else {
                    file_value_subject_[i].set(-1);
                }
            }
            if (model_.current_page() != model::AppPage::Progress) {
                show_page(model::AppPage::Progress);
            }
            break;
        }

        case Phase::Installing:
            if (previous != Phase::Installing) {
                for (int i = 0; i < kFileSlots; ++i) {
                    file_value_subject_[i].set(-1);
                }
            }
            progress_title_subject_.set(s.operation == Operation::Refresh ? "Refreshing lists"
                                        : s.operation == Operation::Clean ? "Cleaning caches"
                                                                          : "Installing");
            progress_detail_subject_.set(s.install_status.empty() ? "Starting apt..."
                                                                  : s.install_status.c_str());
            progress_value_subject_.set(static_cast<int>(s.install_percent * 10));
            if (model_.current_page() != model::AppPage::Progress) {
                show_page(model::AppPage::Progress);
            }
            break;

        case Phase::Done:
        case Phase::Failed:
        case Phase::Cancelled: {
            if (previous == s.phase) {
                break;
            }
            std::string text;
            if (s.phase == Phase::Done) {
                result_title_subject_.set(s.operation == Operation::Refresh ? "Lists refreshed"
                                          : s.operation == Operation::Clean ? "Caches cleaned"
                                                                            : "Done");
                result_tone_subject_.set(0);
                if (s.operation == Operation::Clean) {
                    text = s.freed_bytes ? "Freed " + updater::format_bytes(s.freed_bytes) + "."
                                         : "Nothing to clean.";
                }
                else if (s.upgrades + s.new_installs + s.removals == 0 &&
                    s.operation != Operation::Refresh) {
                    text = "Nothing to do.";
                }
                else if (!updater::operation_is_direct(s.operation)) {
                    text = std::to_string(s.upgrades) + " upgraded";
                    if (s.new_installs) {
                        text += ", " + std::to_string(s.new_installs) + " installed";
                    }
                    if (s.removals) {
                        text += ", " + std::to_string(s.removals) + " removed";
                    }
                    text += ".";
                }
            }
            else if (s.phase == Phase::Failed) {
                result_title_subject_.set("Failed");
                result_tone_subject_.set(1);
                text = s.error;
                // last few log lines help explain apt/dpkg errors
                const size_t n = s.log_tail.size();
                for (size_t i = n > 4 ? n - 4 : 0; i < n; ++i) {
                    text += "\n" + s.log_tail[i];
                }
            }
            else {
                result_title_subject_.set("Cancelled");
                result_tone_subject_.set(2);
                text = "Nothing was installed.";
            }
            if (text.size() > 500) {
                text = text.substr(text.size() - 500);
            }
            result_text_subject_.set(text.c_str());
            show_page(model::AppPage::Result);
            break;
        }
    }
}

} // namespace viewmodel
