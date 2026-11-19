/*
 * SPDX-License-Identifier: MIT
 */

#include "updoit_screens.h"

#include "screen_style.h"

namespace screen {
namespace {

void percent_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* label = lv_observer_get_target_obj(observer);
    const int v = lv_subject_get_int(subject);
    // "0%" while checking or waiting for apt reads as stuck; show nothing instead.
    if (v <= 0) {
        lv_label_set_text(label, "");
    }
    else {
        lv_label_set_text_fmt(label, "%d%%", v / 10);
    }
}

// File rows are hidden while their value is -1.
void file_row_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* row = lv_observer_get_target_obj(observer);
    auto* bar = static_cast<lv_obj_t*>(lv_observer_get_user_data(observer));
    const int v = lv_subject_get_int(subject);
    if (v < 0) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(bar, v, LV_ANIM_OFF);
}

} // namespace

ProgressScreen::ProgressScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void ProgressScreen::build_content(lv_obj_t* content) {
    auto& vm = view_model();
    auto* dark = vm.dark_mode_subject();

    auto* column = style::box(content);
    lv_obj_set_size(column, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(column, 8, 0);
    lv_obj_set_style_pad_top(column, 4, 0);
    lv_obj_set_style_pad_row(column, 3, 0);

    auto* head = style::box(column);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    auto* title = style::label(head, dark, style::font(assets(), 14, true));
    reactive::bind_label_text(title, vm.progress_title_subject());
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    auto* percent = style::label(head, dark, style::font(assets(), 14, true));
    lv_obj_align(percent, LV_ALIGN_RIGHT_MID, 0, 0);
    reactive::observe_obj(percent, vm.progress_value_subject(), percent_observer);

    auto* overall = style::bar(column, dark, 8);
    reactive::bind_bar_value(overall, vm.progress_value_subject());

    auto* detail = style::label(column, dark, style::font(assets(), 12));
    lv_obj_set_width(detail, LV_PCT(100));
    lv_obj_set_height(detail, 15); // one line; DOTS only truncates with a fixed height
    lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_DOTS);
    reactive::bind_label_text(detail, vm.progress_detail_subject());

    for (int i = 0; i < viewmodel::BaseViewModel::kFileSlots; ++i) {
        auto* row = style::box(column);
        lv_obj_set_size(row, LV_PCT(100), 14);
        auto* name = style::label(row, dark, style::font(assets(), 10));
        lv_obj_set_size(name, 196, 13);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        reactive::bind_label_text(name, vm.file_name_subject(i));
        style::accent(name, dark, style::Accent::Part::Text, style::Accent::Tone::Muted);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
        auto* bar = style::bar(row, dark, 4);
        lv_obj_set_width(bar, 96);
        lv_obj_align(bar, LV_ALIGN_RIGHT_MID, 0, 0);
        reactive::observe_obj(row, vm.file_value_subject(i), file_row_observer, bar);
    }

    auto* log = style::label(column, dark, style::font(assets(), 10));
    lv_obj_set_size(log, LV_PCT(100), 13);
    lv_label_set_long_mode(log, LV_LABEL_LONG_MODE_DOTS);
    reactive::bind_label_text(log, vm.log_line_subject());
    style::accent(log, dark, style::Accent::Part::Text, style::Accent::Tone::Muted);
}

bool ProgressScreen::handle_key(uint32_t key) {
    if (key == LV_KEY_ESC) {
        view_model().back();
        return true;
    }
    // Swallow everything else so stray keys do nothing mid-install.
    return true;
}

} // namespace screen
