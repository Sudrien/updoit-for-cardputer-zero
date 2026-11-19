/*
 * SPDX-License-Identifier: MIT
 */

#include "updoit_screens.h"

#include "screen_style.h"

namespace screen {

PasswordScreen::PasswordScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void PasswordScreen::build_content(lv_obj_t* content) {
    auto* dark = view_model().dark_mode_subject();

    auto* column = style::box(content);
    lv_obj_set_size(column, 260, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(column, 6, 0);
    lv_obj_center(column);

    auto* message = style::label(column, dark, style::font(assets(), 14));
    reactive::bind_label_text(message, view_model().password_message_subject());

    auto* field = style::box(column);
    lv_obj_set_size(field, LV_PCT(100), 30);
    lv_obj_set_style_border_width(field, 2, 0);
    lv_obj_set_style_radius(field, 6, 0);
    lv_obj_set_style_pad_hor(field, 8, 0);
    style::accent(field, dark, style::Accent::Part::Border, style::Accent::Tone::Primary);

    auto* mask = style::label(field, dark, style::font(assets(), 16, true));
    reactive::bind_label_text(mask, view_model().password_mask_subject());
    lv_obj_align(mask, LV_ALIGN_LEFT_MID, 0, 2);

    auto* note = style::label(column, dark, style::font(assets(), 12),
                              "Kept in memory only, never saved.");
    style::accent(note, dark, style::Accent::Part::Text, style::Accent::Tone::Muted);
}

bool PasswordScreen::handle_key(uint32_t key) {
    auto& vm = view_model();
    switch (key) {
        case LV_KEY_ESC:
            vm.back();
            return true;
        case LV_KEY_ENTER:
            vm.password_submit();
            return true;
        case LV_KEY_BACKSPACE:
            vm.password_backspace();
            return true;
        default:
            break;
    }
    if (key >= 32 && key <= 126) {
        vm.password_append(static_cast<char>(key));
        return true;
    }
    return false;
}

} // namespace screen
