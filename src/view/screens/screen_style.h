/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include "lvgl.h"

namespace screen::style {

inline const lv_font_t* font(app::AssetManager& assets, uint32_t size, bool bold = false) {
    const lv_font_t* f = assets.load_standard_font(
        size, bold ? app::StandardFontWeight::Bold : app::StandardFontWeight::Regular);
    if (f) {
        return f;
    }
    if (size <= 12) {
        return &lv_font_montserrat_12;
    }
    if (size <= 14) {
        return &lv_font_montserrat_14;
    }
    return &lv_font_montserrat_16;
}

// Plain transparent container with no padding or scrolling.
inline lv_obj_t* box(lv_obj_t* parent) {
    auto* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

inline lv_obj_t* label(lv_obj_t* parent, lv_subject_t* dark_mode, const lv_font_t* f,
                       const char* text = "") {
    auto* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, f, 0);
    reactive::bind_theme(l, dark_mode, reactive::ThemeRole::Text);
    return l;
}

// Colors that follow the theme but are not covered by ThemeRole.
struct Accent {
    lv_obj_t* obj;
    enum class Part { Text, Bg, Indicator, Border } part;
    enum class Tone { Primary, Muted, Border, Error, Warning } tone;
};

inline lv_color_t tone_color(const view::ThemePalette& p, Accent::Tone tone) {
    switch (tone) {
        case Accent::Tone::Primary:
            return p.primary;
        case Accent::Tone::Muted:
            return p.text_disabled;
        case Accent::Tone::Border:
            return p.border;
        case Accent::Tone::Error:
            return p.error;
        case Accent::Tone::Warning:
            return p.warning;
    }
    return p.text;
}

inline void accent_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* obj = lv_observer_get_target_obj(observer);
    const auto part = static_cast<Accent::Part>(reinterpret_cast<intptr_t>(lv_observer_get_user_data(observer)) >> 8);
    const auto tone = static_cast<Accent::Tone>(reinterpret_cast<intptr_t>(lv_observer_get_user_data(observer)) & 0xff);
    const auto colors = view::palette(lv_subject_get_int(subject) != 0);
    const auto c = tone_color(colors, tone);
    switch (part) {
        case Accent::Part::Text:
            lv_obj_set_style_text_color(obj, c, 0);
            break;
        case Accent::Part::Bg:
            lv_obj_set_style_bg_color(obj, c, 0);
            break;
        case Accent::Part::Indicator:
            lv_obj_set_style_bg_color(obj, c, LV_PART_INDICATOR);
            break;
        case Accent::Part::Border:
            lv_obj_set_style_border_color(obj, c, 0);
            break;
    }
}

inline void accent(lv_obj_t* obj, lv_subject_t* dark_mode, Accent::Part part, Accent::Tone tone) {
    const auto packed = (static_cast<intptr_t>(part) << 8) | static_cast<intptr_t>(tone);
    reactive::observe_obj(obj, dark_mode, accent_observer, reinterpret_cast<void*>(packed));
}

// Thin themed progress bar, value range 0..1000.
inline lv_obj_t* bar(lv_obj_t* parent, lv_subject_t* dark_mode, int32_t height) {
    auto* b = lv_bar_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_PCT(100), height);
    lv_bar_set_range(b, 0, 1000);
    lv_obj_set_style_radius(b, height / 2, 0);
    lv_obj_set_style_radius(b, height / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    accent(b, dark_mode, Accent::Part::Bg, Accent::Tone::Border);
    accent(b, dark_mode, Accent::Part::Indicator, Accent::Tone::Primary);
    return b;
}

} // namespace screen::style
