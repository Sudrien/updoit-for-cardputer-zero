/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

// UI translations. Strings are looked up by their English text, gettext style, so
// call sites stay readable and anything missing from the table shows in English.
//
// The language comes from the environment the launcher passes on, read the same
// way as gettext: LANGUAGE, LC_ALL, LC_MESSAGES, then LANG. UPDOIT_LANG overrides
// all of them. zh* selects Simplified Chinese, ja* Japanese, anything else English.
// No setlocale() is involved, so the matching system locale need not be installed.
namespace i18n {

enum class Language {
    English,
    ChineseSimplified,
    Japanese,
};

// The language a single locale value names ("ja_JP.UTF-8", "zh-CN", "C", ...).
Language language_from_locale(const char* value);

// The language the environment asks for (see above).
Language detect_language();

// Detected once on first use; set_language() replaces it (tests, previews).
Language current();
void set_language(Language language);

// BCP 47 code of a language: "en", "zh-CN", "ja".
const char* code(Language language);

// True when the UI text needs a CJK font.
bool needs_cjk_font();

// The translation of an English UI string, or the string itself.
const char* tr(const char* english);

// printf-style formatting of the translated format string.
std::string format(const char* english_format, ...);

// The translation table, for tests.
struct Entry {
    const char* english;
    const char* zh_cn;
    const char* ja;
};
const Entry* entries(std::size_t& count);

} // namespace i18n
