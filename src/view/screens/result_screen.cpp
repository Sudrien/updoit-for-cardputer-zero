/*
 * SPDX-License-Identifier: MIT
 */

#include "updoit_screens.h"

#include "screen_style.h"

namespace screen {
namespace {

// Fires for both the tone and the theme subject, so read both explicitly.
void tone_observer(lv_observer_t* observer, lv_subject_t*) {
    auto* title = lv_observer_get_target_obj(observer);
    auto& vm = *static_cast<viewmodel::BaseViewModel*>(lv_observer_get_user_data(observer));
    const auto colors = view::palette(vm.is_dark_mode());
    const int tone = lv_subject_get_int(vm.result_tone_subject());
    lv_obj_set_style_text_color(title, tone == 0 ? colors.success
                                       : tone == 1 ? colors.error
                                                   : colors.warning,
                                0);
}

} // namespace

ResultScreen::ResultScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void ResultScreen::build_content(lv_obj_t* content) {
    auto& vm = view_model();
    auto* dark = vm.dark_mode_subject();

    auto* column = style::box(content);
    lv_obj_set_size(column, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(column, 8, 0);
    lv_obj_set_style_pad_top(column, 4, 0);
    lv_obj_set_style_pad_row(column, 3, 0);

    auto* title = lv_label_create(column);
    lv_obj_set_style_text_font(title, style::font(assets(), 20, true), 0);
    reactive::bind_label_text(title, vm.result_title_subject());
    reactive::observe_obj(title, vm.result_tone_subject(), tone_observer, &vm);
    reactive::observe_obj(title, dark, tone_observer, &vm);

    scroller_ = lv_obj_create(column);
    lv_obj_remove_style_all(scroller_);
    lv_obj_set_width(scroller_, LV_PCT(100));
    lv_obj_set_flex_grow(scroller_, 1);
    lv_obj_set_scroll_dir(scroller_, LV_DIR_VER);

    auto* text = style::label(scroller_, dark, style::font(assets(), 12));
    lv_obj_set_width(text, LV_PCT(100));
    lv_label_set_long_mode(text, LV_LABEL_LONG_MODE_WRAP);
    reactive::bind_label_text(text, vm.result_text_subject());
}

bool ResultScreen::handle_key(uint32_t key) {
    switch (key) {
        case LV_KEY_UP:
        case ';':
            lv_obj_scroll_by_bounded(scroller_, 0, 30, LV_ANIM_ON);
            return true;
        case LV_KEY_DOWN:
        case '.':
            lv_obj_scroll_by_bounded(scroller_, 0, -30, LV_ANIM_ON);
            return true;
        case LV_KEY_ENTER:
        case LV_KEY_ESC:
            view_model().back();
            return true;
        default:
            return false;
    }
}

} // namespace screen
