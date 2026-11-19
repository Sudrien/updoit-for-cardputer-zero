/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace view::widgets {
namespace {

struct Hint {
    const char* key;
    const char* action;
};

using Hints = std::array<Hint, 3>;

Hints hints_for(model::AppPage page, bool can_cancel) {
    switch (page) {
        case model::AppPage::Password:
            return {{{"ESC", "Quit"}, {"ENTER", "Unlock"}, {nullptr, nullptr}}};
        case model::AppPage::Menu:
            return {{{"ESC", "Quit"}, {"UP/DN", "Choose"}, {"ENTER", "Run"}}};
        case model::AppPage::Confirm:
            return {{{"ESC", "Back"}, {"UP/DN", "Scroll"}, {"ENTER", "Install"}}};
        case model::AppPage::Progress:
            if (can_cancel) {
                return {{{"ESC", "Cancel"}, {nullptr, nullptr}, {nullptr, nullptr}}};
            }
            return {{{"", "Please wait..."}, {nullptr, nullptr}, {nullptr, nullptr}}};
        case model::AppPage::Result:
            return {{{"UP/DN", "Scroll"}, {"ENTER", "Menu"}, {nullptr, nullptr}}};
    }
    return {};
}

} // namespace

NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

NavBar::~NavBar() = default; // observers are bound to core_obj_ and go with it

void NavBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(core_obj_, 10, 0);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);

    const lv_font_t* key_font = assets_.load_standard_font(14);
    const lv_font_t* text_font = assets_.load_standard_font(14, app::StandardFontWeight::Regular);

    for (std::size_t i = 0; i < kMaxHints; ++i) {
        auto* item = lv_obj_create(core_obj_);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_PCT(100));
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(item, 4, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        auto* key = lv_label_create(item);
        lv_obj_set_style_text_font(key, key_font ? key_font : &lv_font_montserrat_14, 0);
        auto* action = lv_label_create(item);
        lv_obj_set_style_text_font(action, text_font ? text_font : &lv_font_montserrat_14, 0);

        items_[i] = item;
        key_labels_[i] = key;
        action_labels_[i] = action;
    }

    reactive::observe_obj(core_obj_, view_model_.current_page_subject(), update_cb, this);
    reactive::observe_obj(core_obj_, view_model_.can_cancel_subject(), update_cb, this);
    reactive::observe_obj(core_obj_, view_model_.dark_mode_subject(), update_cb, this);
    update_hints();
}

void NavBar::update_hints() {
    const auto hints = hints_for(view_model_.current_page(),
                                 lv_subject_get_int(view_model_.can_cancel_subject()) != 0);
    const auto colors = view::palette(view_model_.is_dark_mode());
    for (std::size_t i = 0; i < kMaxHints; ++i) {
        if (!hints[i].action) {
            lv_obj_add_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(key_labels_[i], hints[i].key);
        lv_label_set_text(action_labels_[i], hints[i].action);
        lv_obj_set_style_text_color(key_labels_[i], colors.primary, 0);
        lv_obj_set_style_text_color(action_labels_[i], colors.text, 0);
    }
}

void NavBar::update_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* nav_bar = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav_bar) {
        nav_bar->update_hints();
    }
}

} // namespace view::widgets
