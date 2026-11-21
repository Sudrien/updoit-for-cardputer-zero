/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "asset_manager.h"

#include "i18n.h"
#include "logger.h"

#if LV_USE_FREETYPE
#include "src/libs/freetype/lv_freetype.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <utility>

#ifndef APP_NAME
#define APP_NAME "updoit-for-cardputer-zero"
#endif

#ifndef APP_CMAKE_ASSETS_ROOT
#define APP_CMAKE_ASSETS_ROOT ""
#endif

#ifndef APP_SOURCE_ASSETS_ROOT
#define APP_SOURCE_ASSETS_ROOT "assets"
#endif

namespace app {
namespace {

#if !USE_DESKTOP
constexpr const char* kNotoSansCjkRegular =
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
constexpr const char* kNotoSansCjkBold =
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc";
#endif

bool path_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::filesystem::path weakly_canonical_path(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    return ec ? path.lexically_normal() : canonical;
}

// Fonts with Chinese and Japanese glyphs, tried in order when the UI language
// needs them and Noto Sans CJK is not installed. The first one ships with the
// CardputerZero launcher (applaunch) and covers all kana and ~21k CJK ideographs;
// the others are common Debian/Ubuntu packages, which also helps the simulator.
constexpr const char* kCjkFallbackFonts[] = {
    "/usr/share/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
};

const lv_font_t* fallback_font(uint32_t size) {
    if (size <= 10) {
        return &lv_font_montserrat_10;
    }
    if (size <= 12) {
        return &lv_font_montserrat_12;
    }
    if (size <= 14) {
        return &lv_font_montserrat_14;
    }
    if (size <= 16) {
        return &lv_font_montserrat_16;
    }
    if (size <= 18) {
        return &lv_font_montserrat_18;
    }
    if (size <= 20) {
        return &lv_font_montserrat_20;
    }
    if (size <= 24) {
        return &lv_font_montserrat_24;
    }
    return &lv_font_montserrat_28;
}

} // namespace

struct AssetManager::LoadedFont {
    std::filesystem::path path;
    uint32_t size;
    lv_font_t* font;
};

struct AssetManager::CjkFont {
    uint32_t size;
    lv_font_t font; // a copy of the built-in Montserrat face, with a CJK fallback
};

AssetManager::AssetManager(std::string app_name, std::filesystem::path cmake_assets_root) {
    add_root_if_valid(std::move(cmake_assets_root));
    add_root_if_valid(default_cmake_assets_root());
    add_root_if_valid(APP_SOURCE_ASSETS_ROOT);
    add_root_if_valid(std::filesystem::path{"/usr/share"} / app_name);
}

AssetManager::~AssetManager() {
#if LV_USE_FREETYPE
    for (auto& loaded_font : loaded_fonts_) {
        if (loaded_font && loaded_font->font) {
            lv_freetype_font_delete(loaded_font->font);
        }
    }

#endif
}

std::string AssetManager::default_app_name() {
    return APP_NAME;
}

std::filesystem::path AssetManager::default_cmake_assets_root() {
    return APP_CMAKE_ASSETS_ROOT;
}

const std::vector<std::filesystem::path>& AssetManager::roots() const {
    return roots_;
}

std::filesystem::path AssetManager::resolve(const std::filesystem::path& relative_path) const {
    if (relative_path.is_absolute() && path_exists(relative_path)) {
        return weakly_canonical_path(relative_path);
    }

    for (const auto& root : roots_) {
        auto candidate = root / relative_path;
        if (path_exists(candidate)) {
            return weakly_canonical_path(candidate);
        }
    }

    return {};
}

std::filesystem::path AssetManager::resolve_font(const std::filesystem::path& file_name) const {
    return resolve(std::filesystem::path{"fonts"} / file_name);
}

lv_font_t* AssetManager::load_font(const std::filesystem::path& file_name, uint32_t size) {
#if LV_USE_FREETYPE
    auto path = resolve_font(file_name);
    if (path.empty()) {
        LOG_WARN("font asset not found: {}", file_name.string());
        return nullptr;
    }

    auto existing = std::find_if(loaded_fonts_.begin(), loaded_fonts_.end(), [&](const auto& loaded_font) {
        return loaded_font && loaded_font->path == path && loaded_font->size == size;
    });
    if (existing != loaded_fonts_.end()) {
        return (*existing)->font;
    }

    lv_font_t* font = lv_freetype_font_create(path.string().c_str(),
                                              LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                              size,
                                              LV_FREETYPE_FONT_STYLE_NORMAL);
    if (!font) {
        LOG_WARN("failed to load freetype font: {}", path.string());
        return nullptr;
    }

    loaded_fonts_.push_back(std::make_unique<LoadedFont>(LoadedFont{path, size, font}));
    LOG_INFO("loaded freetype font: {} ({})", path.string(), size);
    return font;
#else
    LV_UNUSED(file_name);
    LV_UNUSED(size);
    LOG_WARN("FreeType font loading requested but LV_USE_FREETYPE is disabled");
    return nullptr;
#endif
}

lv_font_t* AssetManager::load_standard_font(uint32_t size, StandardFontWeight weight) {
#if !USE_DESKTOP
    const char* path = weight == StandardFontWeight::Regular
                           ? kNotoSansCjkRegular
                           : kNotoSansCjkBold;
    if (path_exists(path)) {
        auto* font = load_font(path, size);
        if (font) {
            font->fallback = fallback_font(size);
            return font;
        }
    }
#else
    LV_UNUSED(weight);
#endif
    // Without Noto the UI uses the built-in Montserrat faces (callers fall back
    // to them on nullptr). Chinese and Japanese text also needs CJK glyphs.
    if (!i18n::needs_cjk_font()) {
        return nullptr;
    }
    return load_cjk_standard_font(size);
}

lv_font_t* AssetManager::load_cjk_standard_font(uint32_t size) {
    for (auto& cached : cjk_fonts_) {
        if (cached->size == size) {
            return &cached->font;
        }
    }
    lv_font_t* cjk = nullptr;
    if (const char* forced = std::getenv("UPDOIT_CJK_FONT"); forced && *forced) {
        cjk = load_font(forced, size);
    }
    for (const char* candidate : kCjkFallbackFonts) {
        if (cjk) {
            break;
        }
        if (path_exists(candidate)) {
            cjk = load_font(candidate, size);
        }
    }
    if (!cjk) {
        if (!cjk_missing_logged_) {
            LOG_WARN("no CJK font found; Chinese/Japanese text will show as boxes");
            cjk_missing_logged_ = true;
        }
        return nullptr;
    }
    // Latin keeps the built-in face (same look as English); CJK comes from the fallback.
    auto entry = std::make_unique<CjkFont>(CjkFont{size, *fallback_font(size)});
    entry->font.fallback = cjk;
    cjk_fonts_.push_back(std::move(entry));
    return &cjk_fonts_.back()->font;
}

void AssetManager::add_root_if_valid(std::filesystem::path path) {
    if (path.empty()) {
        return;
    }

    path = weakly_canonical_path(path);
    if (!path_exists(path)) {
        return;
    }

    auto it = std::find(roots_.begin(), roots_.end(), path);
    if (it == roots_.end()) {
        roots_.push_back(std::move(path));
    }
}

} // namespace app
