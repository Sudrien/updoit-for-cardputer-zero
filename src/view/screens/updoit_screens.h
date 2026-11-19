/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

class PasswordScreen : public BaseScreen {
public:
    PasswordScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    bool handle_key(uint32_t key) override;
};

class MenuScreen : public BaseScreen {
public:
    MenuScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    bool handle_key(uint32_t key) override;
};

class ConfirmScreen : public BaseScreen {
public:
    ConfirmScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    bool handle_key(uint32_t key) override;
    lv_obj_t* list_{nullptr};
};

class ProgressScreen : public BaseScreen {
public:
    ProgressScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    bool handle_key(uint32_t key) override;
};

class ResultScreen : public BaseScreen {
public:
    ResultScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    bool handle_key(uint32_t key) override;
    lv_obj_t* scroller_{nullptr};
};

} // namespace screen
