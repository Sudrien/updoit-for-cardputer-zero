/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "daily_refresh.h"
#include "i18n.h"
#include "status_cache.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <ctime>

namespace viewmodel {
namespace {

using i18n::tr;
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
    if (checked <= 0 || age < 0) {
        return "";
    }
    if (age < 90) {
        return tr("just now");
    }
    if (age < 90 * 60) {
        return i18n::format("%lld min ago", static_cast<long long>(age / 60));
    }
    if (age < 36 * 3600) {
        return i18n::format("%lld h ago", static_cast<long long>(age / 3600));
    }
    return i18n::format("%lld d ago", static_cast<long long>(age / 86400));
}

// The last `max` bytes of `text`, without starting inside a UTF-8 sequence.
std::string utf8_tail(const std::string& text, size_t max) {
    if (text.size() <= max) {
        return text;
    }
    size_t start = text.size() - max;
    while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
        ++start;
    }
    return text.substr(start);
}

// The first `max` bytes of `text`, without ending inside a UTF-8 sequence.
std::string utf8_head(const std::string& text, size_t max) {
    if (text.size() <= max) {
        return text;
    }
    size_t end = max;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
        --end;
    }
    return text.substr(0, end);
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

// The last row (kMenuDailyRefresh) is not an apt operation; menu_select handles it.
constexpr Operation kMenuOps[model::kMenuDailyRefresh] = {
    Operation::Refresh,
    Operation::Download,
    Operation::Upgrade,
    Operation::FullUpgrade,
    Operation::Autoremove,
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
      password_message_subject_(tr("Enter your sudo password")),
      password_busy_subject_(false),
      menu_index_subject_(0),
      updates_text_subject_(""),
      menu_hint_subject_{reactive::StringSubject<24>(""), reactive::StringSubject<24>(""),
                         reactive::StringSubject<24>(""), reactive::StringSubject<24>(""),
                         reactive::StringSubject<24>(""), reactive::StringSubject<24>(""),
                         reactive::StringSubject<24>("")},
      daily_label_subject_(""),
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
            result_title_subject_.set(tr("No root access"));
            result_text_subject_.set(tr("Updoit needs sudo (or to run as root) to install "
                                        "packages, and sudo was not found."));
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
            password_message_subject_.set(tr("Enter your sudo password"));
            show_page(model::AppPage::Menu);
        }
        else {
            password_message_subject_.set(tr("Wrong password, try again"));
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
    password_message_subject_.set(tr("Checking..."));
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
    // Show the real command, so the menu teaches what it runs (apt-get under the hood).
    // The daily-refresh row's label is live: see daily_label_subject().
    static const char* const kLabels[model::kMenuItemCount] = {
        "apt update",     "apt dist-upgrade -d", "apt upgrade",
        "apt dist-upgrade", "apt autoremove",    "apt clean",
        "Turn daily check on",
    };
    return tr(kLabels[index]); // the apt commands are not in the table and stay as they are
}

lv_subject_t* BaseViewModel::menu_hint_subject(int index) {
    return menu_hint_subject_[index].native();
}

lv_subject_t* BaseViewModel::daily_label_subject() { return daily_label_subject_.native(); }

void BaseViewModel::refresh_daily_row() {
    daily_refresh_on_ = updater::daily_refresh_enabled();
    daily_label_subject_.set(tr(daily_refresh_on_ ? "Turn daily check off" : "Turn daily check on"));
    menu_hint_subject_[model::kMenuDailyRefresh].set(tr(daily_refresh_on_ ? "is on" : "is off"));
}

void BaseViewModel::toggle_daily_refresh() {
    std::string error;
    if (updater::set_daily_refresh(privilege_, !daily_refresh_on_, error)) {
        refresh_daily_row();
        return;
    }
    refresh_daily_row();
    result_title_subject_.set(tr("Failed"));
    result_tone_subject_.set(1);
    result_text_subject_.set(
        utf8_head(std::string(tr("Could not change the daily check.")) + "\n" + error, 500).c_str());
    show_page(model::AppPage::Result);
}

void BaseViewModel::refresh_updates_text() {
    const auto cache = updater::read_status_cache();
    std::string text;
    if (!cache.valid) {
        text = tr("Not checked yet - try Refresh lists");
    }
    else {
        if (cache.upgradable == 0) {
            text = tr("Up to date");
        }
        else {
            text = i18n::format(cache.upgradable == 1 ? "%d update available" : "%d updates available",
                                cache.upgradable);
        }
        const auto age = age_text(cache.checked);
        if (!age.empty()) {
            text += " - " + age;
        }
        if (!cache.error.empty()) {
            text = i18n::format("Last check failed - %s", age.c_str());
        }
    }
    updates_text_subject_.set(text.c_str());

    // Per-row hints: how many packages each operation would touch (cached by updoit-check).
    const auto count_text = [&](int n) {
        return !cache.valid ? std::string("?")
               : n == 0     ? std::string(tr("none"))
                            : i18n::format(n == 1 ? "%d pkg" : "%d pkgs", n);
    };
    const auto updated = age_text(cache.updated);
    menu_hint_subject_[0].set(updated.empty() ? tr("never updated") : updated.c_str());
    menu_hint_subject_[1].set(count_text(cache.full_upgradable).c_str());
    menu_hint_subject_[2].set(count_text(cache.upgradable).c_str());
    menu_hint_subject_[3].set(count_text(cache.full_upgradable).c_str());
    menu_hint_subject_[4].set(count_text(cache.autoremovable).c_str());
    menu_hint_subject_[5].set(!cache.valid || !cache.has_cache_bytes
                                  ? "?"
                                  : (cache.cache_bytes == 0 ? tr("empty")
                                                            : updater::format_bytes(cache.cache_bytes).c_str()));
    refresh_daily_row();
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
    if (model_.menu_index() == model::kMenuDailyRefresh) {
        toggle_daily_refresh();
        return;
    }
    const auto op = kMenuOps[model_.menu_index()];
    if (job_->start(op)) {
        // The first snapshot moves us on; show the progress page while planning.
        progress_title_subject_.set(tr(op == Operation::Refresh ? "Refreshing lists"
                                       : op == Operation::Clean ? "Cleaning caches"
                                                                : "Checking"));
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
    // The updater (shared with updoit-cli) writes English; translate its one word here.
    static const std::string kKeptBack = " (kept back)";
    auto lines = job_->plan_lines();
    for (auto& line : lines) {
        if (line.size() >= kKeptBack.size() &&
            line.compare(line.size() - kKeptBack.size(), kKeptBack.size(), kKeptBack) == 0) {
            line.replace(line.size() - kKeptBack.size(), kKeptBack.size(), tr(" (kept back)"));
        }
    }
    return lines;
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

    switch (s.phase) {
        case Phase::Idle:
            break;

        case Phase::Planning:
            progress_title_subject_.set(tr("Checking"));
            break;

        case Phase::AwaitingConfirm: {
            std::string summary = s.operation == Operation::Download ? tr("Download only: ") : "";
            summary += i18n::format("%d upgrade", s.upgrades);
            if (s.new_installs) {
                summary += i18n::format(", %d new", s.new_installs);
            }
            if (s.removals) {
                summary += i18n::format(", %d remove", s.removals);
            }
            if (s.download_bytes) {
                summary += i18n::format(", %s", updater::format_bytes(s.download_bytes).c_str());
            }
            confirm_summary_subject_.set(summary.c_str());
            if (previous != Phase::AwaitingConfirm) {
                show_page(model::AppPage::Confirm);
            }
            break;
        }

        case Phase::Downloading: {
            const auto& d = s.download;
            progress_title_subject_.set(
                i18n::format("Downloading %d/%d", d.files_complete, d.files_total).c_str());
            progress_detail_subject_.set(i18n::format("%s of %s",
                                                      updater::format_bytes(d.done_bytes).c_str(),
                                                      updater::format_bytes(d.total_bytes).c_str())
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
            progress_title_subject_.set(tr(s.operation == Operation::Refresh ? "Refreshing lists"
                                           : s.operation == Operation::Clean ? "Cleaning caches"
                                                                             : "Installing"));
            progress_detail_subject_.set(s.install_status.empty() ? tr("Starting apt...")
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
                result_title_subject_.set(tr(s.operation == Operation::Refresh ? "Lists refreshed"
                                             : s.operation == Operation::Clean ? "Caches cleaned"
                                                                               : "Done"));
                result_tone_subject_.set(0);
                if (s.operation == Operation::Download) {
                    result_title_subject_.set(tr("Downloaded"));
                    text = s.downloaded_count == 0
                               ? std::string(tr("Nothing to download."))
                               : i18n::format("%d packages cached. Run apt upgrade to install them.",
                                              static_cast<int>(s.downloaded_count));
                }
                else if (s.operation == Operation::Clean) {
                    text = s.freed_bytes
                               ? i18n::format("Freed %s.", updater::format_bytes(s.freed_bytes).c_str())
                               : std::string(tr("Nothing to clean."));
                }
                else if (s.upgrades + s.new_installs + s.removals == 0 &&
                    s.operation != Operation::Refresh) {
                    text = tr("Nothing to do.");
                }
                else if (!updater::operation_is_direct(s.operation)) {
                    text = i18n::format("%d upgraded", s.upgrades);
                    if (s.new_installs) {
                        text += i18n::format(", %d installed", s.new_installs);
                    }
                    if (s.removals) {
                        text += i18n::format(", %d removed", s.removals);
                    }
                    text += tr(".");
                }
            }
            else if (s.phase == Phase::Failed) {
                result_title_subject_.set(tr("Failed"));
                result_tone_subject_.set(1);
                text = s.error;
                // last few log lines help explain apt/dpkg errors
                const size_t n = s.log_tail.size();
                for (size_t i = n > 4 ? n - 4 : 0; i < n; ++i) {
                    text += "\n" + s.log_tail[i];
                }
            }
            else {
                result_title_subject_.set(tr("Cancelled"));
                result_tone_subject_.set(2);
                text = tr("Nothing was installed.");
            }
            text = utf8_tail(text, 500);
            result_text_subject_.set(text.c_str());
            show_page(model::AppPage::Result);
            break;
        }
    }
}

} // namespace viewmodel
