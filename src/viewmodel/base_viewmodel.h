/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "privilege.h"
#include "upgrade_job.h"

#include "lvgl.h"

#include <future>
#include <memory>
#include <string>
#include <vector>

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    // Detects root/sudo and picks the first page. Call once after construction.
    void start();
    // Polls the worker thread and publishes its state. Call from an LVGL timer (UI thread).
    void tick();

    // ---- shared ----
    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();
    model::AppPage current_page() const;
    void request_quit();
    // False while apt/dpkg is running: quitting then would only hide the window, but the
    // user should see the result.
    bool can_quit() const;

    // ---- password page ----
    lv_subject_t* password_mask_subject();
    lv_subject_t* password_message_subject();
    lv_subject_t* password_busy_subject();
    void password_append(char c);
    void password_backspace();
    void password_submit();

    // ---- menu page ----
    lv_subject_t* menu_index_subject();
    lv_subject_t* updates_text_subject();
    static const char* menu_label(int index);
    static const char* menu_hint(int index);
    void menu_move(int delta);
    void menu_select(int index = -1);

    // ---- confirm page ----
    lv_subject_t* confirm_summary_subject();
    std::vector<std::string> plan_lines() const;
    void confirm();

    // ---- progress page ----
    lv_subject_t* progress_title_subject();
    lv_subject_t* progress_detail_subject();
    lv_subject_t* progress_value_subject(); // 0..1000
    lv_subject_t* file_name_subject(int slot);
    lv_subject_t* file_value_subject(int slot); // 0..1000, -1 = hidden
    lv_subject_t* log_line_subject();
    lv_subject_t* can_cancel_subject();

    // ---- result page ----
    lv_subject_t* result_title_subject();
    lv_subject_t* result_text_subject();
    lv_subject_t* result_tone_subject(); // 0 ok, 1 failed, 2 cancelled

    // Esc on any page: cancel / back / quit, depending on where we are.
    void back();

    static constexpr int kFileSlots = 2;

private:
    void show_page(model::AppPage page);
    void refresh_updates_text();
    void publish_job(const updater::JobSnapshot& s);
    void publish_password_mask();

    model::BaseModel model_;
    updater::Privilege privilege_;
    std::unique_ptr<updater::UpgradeJob> job_;
    uint32_t job_revision_ = 0;
    updater::Phase last_phase_ = updater::Phase::Idle;

    std::string password_input_;
    std::future<bool> auth_future_;

    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::BoolSubject quit_requested_subject_;

    reactive::StringSubject<64> password_mask_subject_;
    reactive::StringSubject<64> password_message_subject_;
    reactive::BoolSubject password_busy_subject_;

    reactive::IntSubject menu_index_subject_;
    reactive::StringSubject<64> updates_text_subject_;

    reactive::StringSubject<96> confirm_summary_subject_;

    reactive::StringSubject<48> progress_title_subject_;
    reactive::StringSubject<96> progress_detail_subject_;
    reactive::IntSubject progress_value_subject_;
    reactive::StringSubject<64> file_name_subject_[kFileSlots];
    reactive::IntSubject file_value_subject_[kFileSlots];
    reactive::StringSubject<128> log_line_subject_;
    reactive::BoolSubject can_cancel_subject_;

    reactive::StringSubject<32> result_title_subject_;
    reactive::StringSubject<512> result_text_subject_;
    reactive::IntSubject result_tone_subject_;
};

} // namespace viewmodel
