/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {

const char* BaseModel::app_title() const {
    return "Updoit";
}

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void BaseModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

AppPage BaseModel::current_page() const {
    return current_page_;
}

void BaseModel::set_current_page(AppPage page) {
    current_page_ = page;
}

int BaseModel::menu_index() const {
    return menu_index_;
}

void BaseModel::move_menu(int delta) {
    set_menu_index(menu_index_ + delta);
}

void BaseModel::set_menu_index(int index) {
    // wrap around
    menu_index_ = ((index % kMenuItemCount) + kMenuItemCount) % kMenuItemCount;
}

} // namespace model
