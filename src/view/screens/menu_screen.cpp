/*
 * SPDX-License-Identifier: MIT
 */

#include "updoit_screens.h"

#include "screen_style.h"

namespace screen {
namespace {

void row_selected_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* row = lv_observer_get_target_obj(observer);
    const auto index = static_cast<int>(reinterpret_cast<intptr_t>(lv_observer_get_user_data(observer)));
    if (lv_subject_get_int(subject) == index) {
        lv_obj_add_state(row, LV_STATE_CHECKED);
        lv_obj_scroll_to_view(row, LV_ANIM_OFF); // keep the selection visible in the list
    }
    else {
        lv_obj_remove_state(row, LV_STATE_CHECKED);
    }
}

} // namespace

MenuScreen::MenuScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void MenuScreen::build_content(lv_obj_t* content) {
    auto* dark = view_model().dark_mode_subject();

    auto* column = style::box(content);
    lv_obj_set_size(column, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(column, 8, 0);
    lv_obj_set_style_pad_top(column, 3, 0);
    lv_obj_set_style_pad_row(column, 0, 0);

    auto* status = style::label(column, dark, style::font(assets(), 12));
    reactive::bind_label_text(status, view_model().updates_text_subject());
    style::accent(status, dark, style::Accent::Part::Text, style::Accent::Tone::Muted);

    // Rows live in a scrolling list: six rows do not fit the content area at once.
    auto* list = style::box(column);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_height(list, 5 * 17); // exactly five rows, so none is half cut off
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    for (int i = 0; i < model::kMenuItemCount; ++i) {
        auto* row = style::box(list);
        lv_obj_set_size(row, LV_PCT(100), 17); // five rows fit the 110 px content area
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 6, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_20, LV_STATE_CHECKED);
        style::accent(row, dark, style::Accent::Part::Bg, style::Accent::Tone::Primary);

        // Hotkey number in the accent colour, like the key names in the bottom bar.
        char number[4];
        lv_snprintf(number, sizeof(number), "%d", i + 1);
        auto* key = style::label(row, dark, style::font(assets(), 14, true), number);
        style::accent(key, dark, style::Accent::Part::Text, style::Accent::Tone::Primary);
        lv_obj_align(key, LV_ALIGN_LEFT_MID, 0, 0);

        auto* name = style::label(row, dark, style::font(assets(), 14, true),
                                  viewmodel::BaseViewModel::menu_label(i));
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 16, 0);

        auto* hint = style::label(row, dark, style::font(assets(), 12));
        reactive::bind_label_text(hint, view_model().menu_hint_subject(i));
        lv_obj_align(hint, LV_ALIGN_RIGHT_MID, 0, 0);
        style::accent(hint, dark, style::Accent::Part::Text, style::Accent::Tone::Muted);

        reactive::observe_obj(row, view_model().menu_index_subject(), row_selected_observer,
                              reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }
}

bool MenuScreen::handle_key(uint32_t key) {
    auto& vm = view_model();
    switch (key) {
        case LV_KEY_UP:
        case LV_KEY_LEFT:
        case 'f': // f/x: up/down without fn
        case 'F':
        case 'z': // z/c: left/right, as labelled on the keys
        case 'Z':
            vm.menu_move(-1);
            return true;
        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:
        case 'x':
        case 'X':
        case 'c':
        case 'C':
            vm.menu_move(1);
            return true;
        case LV_KEY_ENTER:
            vm.menu_select();
            return true;
        case LV_KEY_ESC:
            vm.back();
            return true;
        default:
            break;
    }
    if (key >= '1' && key < '1' + model::kMenuItemCount) {
        vm.menu_select(static_cast<int>(key - '1'));
        return true;
    }
    return false;
}

} // namespace screen
