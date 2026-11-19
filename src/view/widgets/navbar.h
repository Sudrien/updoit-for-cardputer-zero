/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_widget.h"
#include "base_viewmodel.h"

#include <array>

namespace app {
class AssetManager;
}

namespace view::widgets {

// Bottom bar with the key hints for the current page, e.g. "ESC Back   ENTER Install".
class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;

    void build() override;

private:
    static constexpr std::size_t kMaxHints = 3;

    void update_hints();
    static void update_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<lv_obj_t*, kMaxHints> items_{};
    std::array<lv_obj_t*, kMaxHints> key_labels_{};
    std::array<lv_obj_t*, kMaxHints> action_labels_{};
};

} // namespace view::widgets
