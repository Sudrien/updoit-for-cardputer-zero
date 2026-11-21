/*
 * SPDX-License-Identifier: MIT
 */

// Tests for the UI translation table and language detection (no LVGL, no framework).

#include "i18n.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace {
int failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

using i18n::Language;

// The printf conversions of a format string, in order ("%lld", "%s", ...).
std::string conversions(const char* fmt) {
    std::string out;
    for (const char* p = fmt; *p; ++p) {
        if (*p != '%') {
            continue;
        }
        ++p;
        if (*p == '%') {
            continue;
        }
        out += '%';
        while (*p && !std::strchr("diouxXeEfFgGaAcspn", *p)) {
            out += *p++;
        }
        if (!*p) {
            break;
        }
        out += *p;
        out += ' ';
    }
    return out;
}

// Rough UTF-8 check: lead bytes followed by the right number of continuation bytes.
bool valid_utf8(const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) {
        int extra = *p < 0x80 ? 0 : (*p >> 5) == 6 ? 1 : (*p >> 4) == 14 ? 2 : (*p >> 3) == 30 ? 3 : -1;
        if (extra < 0) {
            return false;
        }
        ++p;
        for (; extra > 0; --extra, ++p) {
            if ((*p & 0xC0) != 0x80) {
                return false;
            }
        }
    }
    return true;
}

void test_table() {
    std::size_t count = 0;
    const auto* entries = i18n::entries(count);
    CHECK(count > 50);
    std::set<std::string> seen;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& e = entries[i];
        CHECK(seen.insert(e.english).second); // no duplicate keys
        CHECK(e.zh_cn && *e.zh_cn);
        CHECK(e.ja && *e.ja);
        CHECK(valid_utf8(e.zh_cn));
        CHECK(valid_utf8(e.ja));
        const auto want = conversions(e.english);
        if (conversions(e.zh_cn) != want || conversions(e.ja) != want) {
            std::fprintf(stderr, "format mismatch for \"%s\"\n", e.english);
            ++failures;
        }
    }
}

// Strings that end up in fixed-size LVGL string subjects (see base_viewmodel.h).
void test_lengths() {
    const struct {
        const char* english;
        std::size_t room; // bytes, without the terminator
    } limited[] = {
        {"never updated", 23}, {"none", 23}, {"empty", 23}, {"is on", 23}, {"is off", 23},
        {"%d pkgs", 23 - 4}, {"%lld min ago", 23 - 4}, {"%lld h ago", 23 - 4}, {"%lld d ago", 23 - 4},
        {"Turn daily check on", 31}, {"Turn daily check off", 31},
        {"Enter your sudo password", 63}, {"Wrong password, try again", 63},
        {"Not checked yet - try Refresh lists", 63}, {"Failed", 31}, {"Cancelled", 31},
        {"Lists refreshed", 31}, {"Caches cleaned", 31}, {"Downloaded", 31}, {"No root access", 31},
        {"Refreshing lists", 47}, {"Cleaning caches", 47}, {"Downloading %d/%d", 47 - 8},
    };
    for (const auto lang : {Language::ChineseSimplified, Language::Japanese}) {
        i18n::set_language(lang);
        for (const auto& item : limited) {
            if (std::strlen(i18n::tr(item.english)) > item.room) {
                std::fprintf(stderr, "%s: \"%s\" is too long\n", i18n::code(lang), i18n::tr(item.english));
                ++failures;
            }
        }
    }
}

void test_locale_parsing() {
    CHECK(i18n::language_from_locale("ja_JP.UTF-8") == Language::Japanese);
    CHECK(i18n::language_from_locale("ja") == Language::Japanese);
    CHECK(i18n::language_from_locale("zh_CN.UTF-8") == Language::ChineseSimplified);
    CHECK(i18n::language_from_locale("zh-TW") == Language::ChineseSimplified);
    CHECK(i18n::language_from_locale("en_US.UTF-8") == Language::English);
    CHECK(i18n::language_from_locale("C.UTF-8") == Language::English);
    CHECK(i18n::language_from_locale("") == Language::English);
    CHECK(i18n::language_from_locale(nullptr) == Language::English);
}

void set_env(const char* name, const char* value) {
    if (value) {
        setenv(name, value, 1);
    }
    else {
        unsetenv(name);
    }
}

Language detect_with(const char* updoit, const char* language, const char* lc_all,
                     const char* lc_messages, const char* lang) {
    set_env("UPDOIT_LANG", updoit);
    set_env("LANGUAGE", language);
    set_env("LC_ALL", lc_all);
    set_env("LC_MESSAGES", lc_messages);
    set_env("LANG", lang);
    return i18n::detect_language();
}

void test_detection() {
    CHECK(detect_with(nullptr, nullptr, nullptr, nullptr, "ja_JP.UTF-8") == Language::Japanese);
    CHECK(detect_with(nullptr, nullptr, nullptr, nullptr, "en_US.UTF-8") == Language::English);
    CHECK(detect_with(nullptr, nullptr, nullptr, nullptr, nullptr) == Language::English);
    CHECK(detect_with(nullptr, nullptr, nullptr, "zh_CN.UTF-8", "en_GB.UTF-8") == Language::ChineseSimplified);
    CHECK(detect_with(nullptr, nullptr, "en_GB.UTF-8", "ja_JP.UTF-8", "ja_JP.UTF-8") == Language::English);
    CHECK(detect_with(nullptr, "ja:en", nullptr, nullptr, "en_GB.UTF-8") == Language::Japanese);
    CHECK(detect_with(nullptr, "de:zh_CN", nullptr, nullptr, "en_GB.UTF-8") == Language::ChineseSimplified);
    CHECK(detect_with(nullptr, "en:ja", nullptr, nullptr, "ja_JP.UTF-8") == Language::English);
    CHECK(detect_with(nullptr, "ja", "C", nullptr, "ja_JP.UTF-8") == Language::English); // C ignores LANGUAGE
    CHECK(detect_with("zh", nullptr, "ja_JP.UTF-8", nullptr, nullptr) == Language::ChineseSimplified);
    CHECK(detect_with("en", nullptr, nullptr, nullptr, "ja_JP.UTF-8") == Language::English);
}

void test_lookup() {
    i18n::set_language(Language::English);
    CHECK(std::strcmp(i18n::tr("Quit"), "Quit") == 0);
    CHECK(!i18n::needs_cjk_font());
    i18n::set_language(Language::Japanese);
    CHECK(std::strcmp(i18n::tr("Quit"), "終了") == 0);
    CHECK(std::strcmp(i18n::tr("not in the table"), "not in the table") == 0);
    CHECK(std::strcmp(i18n::tr(nullptr), "") == 0);
    CHECK(i18n::needs_cjk_font());
    CHECK(i18n::format("Downloading %d/%d", 3, 12) == "ダウンロード中 3/12");
    CHECK(std::strcmp(i18n::tr(" (kept back)"), "（保留）") == 0); // must match upgrade_job.cpp
    i18n::set_language(Language::ChineseSimplified);
    CHECK(std::strcmp(i18n::tr("Quit"), "退出") == 0);
    CHECK(i18n::format("%lld min ago", 5LL) == "5 分钟前");
    i18n::set_language(Language::English);
    CHECK(i18n::format("%d updates available", 2) == "2 updates available");
}

} // namespace

int main() {
    test_table();
    test_lengths();
    test_locale_parsing();
    test_detection();
    test_lookup();
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all i18n tests passed\n");
    return 0;
}
