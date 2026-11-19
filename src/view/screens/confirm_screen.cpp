/*
 * SPDX-License-Identifier: MIT
 */

#include "updoit_screens.h"

#include "screen_style.h"

#include <string>

namespace screen {

ConfirmScreen::ConfirmScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void ConfirmScreen::build_content(lv_obj_t* content) {
    auto* dark = view_model().dark_mode_subject();

    auto* column = style::box(content);
    lv_obj_set_size(column, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(column, 8, 0);
    lv_obj_set_style_pad_top(column, 3, 0);
    lv_obj_set_style_pad_row(column, 3, 0);

    auto* summary = style::label(column, dark, style::font(assets(), 14, true));
    reactive::bind_label_text(summary, view_model().confirm_summary_subject());

    list_ = lv_obj_create(column);
    lv_obj_remove_style_all(list_);
    lv_obj_set_width(list_, LV_PCT(100));
    lv_obj_set_flex_grow(list_, 1);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);

    // One label: far cheaper than a widget per package on the CM0.
    std::string text;
    const auto lines = view_model().plan_lines();
    constexpr size_t kMaxLines = 300;
    for (size_t i = 0; i < lines.size() && i < kMaxLines; ++i) {
        if (i) {
            text += '\n';
        }
        text += lines[i];
    }
    if (lines.size() > kMaxLines) {
        text += "\n... and " + std::to_string(lines.size() - kMaxLines) + " more";
    }
    auto* body = style::label(list_, dark, style::font(assets(), 12), text.c_str());
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_MODE_CLIP);
}

bool ConfirmScreen::handle_key(uint32_t key) {
    auto& vm = view_model();
    switch (key) {
        case LV_KEY_UP:
        case ';':
            lv_obj_scroll_by_bounded(list_, 0, 30, LV_ANIM_ON);
            return true;
        case LV_KEY_DOWN:
        case '.':
            lv_obj_scroll_by_bounded(list_, 0, -30, LV_ANIM_ON);
            return true;
        case LV_KEY_ENTER:
            vm.confirm();
            return true;
        case LV_KEY_ESC:
            vm.back();
            return true;
        default:
            return false;
    }
}

} // namespace screen
