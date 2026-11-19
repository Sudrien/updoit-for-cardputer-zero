/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

enum class AppPage {
    Password = 0, // sudo password entry
    Menu     = 1, // pick an apt operation
    Confirm  = 2, // review the plan
    Progress = 3, // downloading / installing
    Result   = 4, // done / failed / cancelled
};

constexpr int kMenuItemCount = 4;

class BaseModel {
public:
    const char* app_title() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    AppPage current_page() const;
    void set_current_page(AppPage page);

    int menu_index() const;
    void move_menu(int delta);
    void set_menu_index(int index);

private:
    bool dark_mode_ = false;
    AppPage current_page_ = AppPage::Menu;
    int menu_index_ = 0;
};

} // namespace model
