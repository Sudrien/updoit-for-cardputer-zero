/*
 * SPDX-License-Identifier: MIT
 */

#include "i18n.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace i18n {
namespace {

// English, Simplified Chinese, Japanese. Format strings must keep the same
// conversions in the same order in every column (checked by i18n_tests).
// Keep translations short: the screen is 320x170 and some subjects hold only
// 24 bytes (menu hints), so a few CJK characters (3 bytes each) is the budget.
constexpr Entry kEntries[] = {
    // Age of the last check (status line, menu hints)
    {"just now", "刚刚", "たった今"},
    {"%lld min ago", "%lld 分钟前", "%lld 分前"},
    {"%lld h ago", "%lld 小时前", "%lld 時間前"},
    {"%lld d ago", "%lld 天前", "%lld 日前"},

    // Password page
    {"Enter your sudo password", "请输入 sudo 密码", "sudo のパスワードを入力"},
    {"Wrong password, try again", "密码错误，请重试", "パスワードが違います"},
    {"Checking...", "正在验证...", "確認中..."},
    {"Kept in memory only, never saved.", "仅保存在内存中，不会写入磁盘。", "メモリ上にのみ保持し、保存しません。"},

    // No sudo
    {"No root access", "没有 root 权限", "root 権限がありません"},
    {"Updoit needs sudo (or to run as root) to install packages, and sudo was not found.",
     "Updoit 需要 sudo（或以 root 运行）才能安装软件包，但未找到 sudo。",
     "パッケージのインストールには sudo（または root での実行）が必要ですが、sudo が見つかりません。"},

    // Menu
    {"Turn daily check on", "开启每日检查", "毎日のチェックをオン"},
    {"Turn daily check off", "关闭每日检查", "毎日のチェックをオフ"},
    {"is on", "已开启", "オン"},
    {"is off", "已关闭", "オフ"},
    {"Could not change the daily check.", "无法更改每日检查设置。", "毎日のチェック設定を変更できませんでした。"},
    {"Not checked yet - try Refresh lists", "尚未检查 - 请运行 apt update", "未確認 - apt update を実行してください"},
    {"Up to date", "已是最新", "最新です"},
    {"%d update available", "有 %d 个可用更新", "%d 件の更新があります"},
    {"%d updates available", "有 %d 个可用更新", "%d 件の更新があります"},
    {"Last check failed - %s", "上次检查失败 - %s", "前回のチェックに失敗 - %s"},
    {"none", "无", "なし"},
    {"%d pkg", "%d 个包", "%d 個"},
    {"%d pkgs", "%d 个包", "%d 個"},
    {"never updated", "从未更新", "未更新"},
    {"empty", "空", "空"},

    // Progress page
    {"Refreshing lists", "正在刷新列表", "リストを更新中"},
    {"Cleaning caches", "正在清理缓存", "キャッシュを削除中"},
    {"Checking", "正在检查", "確認中"},
    {"Installing", "正在安装", "インストール中"},
    {"Downloading %d/%d", "正在下载 %d/%d", "ダウンロード中 %d/%d"},
    {"%s of %s", "%s / %s", "%s / %s"},
    {"Starting apt...", "正在启动 apt...", "apt を起動中..."},

    // Confirm page
    {"Download only: ", "仅下载：", "ダウンロードのみ："},
    {"%d upgrade", "升级 %d", "更新 %d"},
    {", %d new", "，新装 %d", "、新規 %d"},
    {", %d remove", "，删除 %d", "、削除 %d"},
    {", %s", "，%s", "、%s"},
    {"\n... and %d more", "\n……还有 %d 个", "\n…他 %d 件"},

    // Result page
    {"Failed", "失败", "失敗"},
    {"Lists refreshed", "列表已刷新", "リストを更新しました"},
    {"Caches cleaned", "缓存已清理", "キャッシュ削除完了"},
    {"Done", "完成", "完了"},
    {"Downloaded", "已下载", "ダウンロード完了"},
    {"Nothing to download.", "没有需要下载的内容。", "ダウンロードするものはありません。"},
    {"%d packages cached. Run apt upgrade to install them.",
     "已缓存 %d 个软件包。运行 apt upgrade 进行安装。",
     "%d 個のパッケージをキャッシュしました。apt upgrade でインストールできます。"},
    {"Freed %s.", "已释放 %s。", "%s を解放しました。"},
    {"Nothing to clean.", "没有需要清理的内容。", "削除するものはありません。"},
    {"Nothing to do.", "无需操作。", "何もすることはありません。"},
    {"%d upgraded", "已升级 %d", "%d 個を更新"},
    {", %d installed", "，已安装 %d", "、%d 個をインストール"},
    {", %d removed", "，已删除 %d", "、%d 個を削除"},
    {".", "。", "。"},
    {"Cancelled", "已取消", "キャンセルしました"},
    {"Nothing was installed.", "未安装任何内容。", "何もインストールされていません。"},

    // Bottom bar
    {"Quit", "退出", "終了"},
    {"Unlock", "解锁", "解除"},
    {"Choose", "选择", "選択"},
    {"Run", "运行", "実行"},
    {"Back", "返回", "戻る"},
    {"Scroll", "滚动", "スクロール"},
    {"Start", "开始", "開始"},
    {"Cancel", "取消", "キャンセル"},
    {"Please wait...", "请稍候...", "お待ちください..."},
    {"Menu", "菜单", "メニュー"},

    // Keyboard help popup
    {"Keyboard help", "按键帮助", "キー操作"},
    {"PASSWORD", "密码", "パスワード"},
    {"MENU", "菜单", "メニュー"},
    {"CONFIRM", "确认", "確認"},
    {"PROGRESS", "进度", "進行状況"},
    {"RESULT", "结果", "結果"},
    {"type", "输入", "入力"},
    {"sudo password", "sudo 密码", "sudo パスワード"},
    {"Delete", "删除", "削除"},
    {"Run directly", "直接运行", "直接実行"},
    {"Scroll list", "滚动列表", "リストをスクロール"},
    {"Download + install", "下载并安装", "ダウンロードしてインストール"},
    {"Cancel (not while", "取消", "キャンセル"},
    {"installing)", "（安装时不可取消）", "（インストール中は不可）"},
    {"Back to menu", "返回菜单", "メニューに戻る"},
    {"HELP / ESC  Close   PRSC  Screenshot", "HELP / ESC  关闭   PRSC  截图",
     "HELP / ESC  閉じる   PRSC  スクリーンショット"},

    // Toasts
    {"Saved to ", "已保存到 ", "保存先："},
    {"Screenshot failed: %s", "截图失败：%s", "スクリーンショット失敗：%s"},
    {"Installing - please wait", "正在安装 - 请稍候", "インストール中 - お待ちください"},
};

constexpr int kUnset = -1;
std::atomic<int> g_language{kUnset};

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

const char* env(const char* name) {
    const char* value = std::getenv(name);
    return value && *value ? value : nullptr;
}

const std::unordered_map<std::string_view, const Entry*>& index() {
    static const auto map = [] {
        std::unordered_map<std::string_view, const Entry*> m;
        for (const auto& entry : kEntries) {
            m.emplace(entry.english, &entry);
        }
        return m;
    }();
    return map;
}

} // namespace

Language language_from_locale(const char* value) {
    if (!value) {
        return Language::English;
    }
    std::string_view v(value);
    if (starts_with(v, "zh")) {
        return Language::ChineseSimplified;
    }
    if (starts_with(v, "ja")) {
        return Language::Japanese;
    }
    return Language::English;
}

Language detect_language() {
    if (const char* forced = env("UPDOIT_LANG")) {
        return language_from_locale(forced);
    }
    // As in gettext, LANGUAGE is a colon-separated preference list, and it is
    // ignored when the locale is C/POSIX.
    const char* locale = env("LC_ALL");
    if (!locale) {
        locale = env("LC_MESSAGES");
    }
    if (!locale) {
        locale = env("LANG");
    }
    const bool c_locale = locale && (std::strcmp(locale, "C") == 0 ||
                                     std::strcmp(locale, "POSIX") == 0 ||
                                     starts_with(locale, "C."));
    if (const char* list = env("LANGUAGE"); list && !c_locale) {
        std::string_view rest(list);
        while (!rest.empty()) {
            const auto colon = rest.find(':');
            const std::string item(rest.substr(0, colon));
            const auto lang = language_from_locale(item.c_str());
            if (lang != Language::English || starts_with(item, "en")) {
                return lang;
            }
            if (colon == std::string_view::npos) {
                break;
            }
            rest.remove_prefix(colon + 1);
        }
    }
    return language_from_locale(locale);
}

Language current() {
    int value = g_language.load(std::memory_order_relaxed);
    if (value == kUnset) {
        value = static_cast<int>(detect_language());
        g_language.store(value, std::memory_order_relaxed);
    }
    return static_cast<Language>(value);
}

void set_language(Language language) {
    g_language.store(static_cast<int>(language), std::memory_order_relaxed);
}

const char* code(Language language) {
    switch (language) {
        case Language::ChineseSimplified:
            return "zh-CN";
        case Language::Japanese:
            return "ja";
        case Language::English:
            break;
    }
    return "en";
}

bool needs_cjk_font() {
    return current() != Language::English;
}

const char* tr(const char* english) {
    if (!english) {
        return "";
    }
    const auto lang = current();
    if (lang == Language::English) {
        return english;
    }
    const auto& map = index();
    const auto it = map.find(english);
    if (it == map.end()) {
        return english;
    }
    const char* text = lang == Language::Japanese ? it->second->ja : it->second->zh_cn;
    return text && *text ? text : english;
}

std::string format(const char* english_format, ...) {
    const char* fmt = tr(english_format);
    va_list args;
    va_start(args, english_format);
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    std::string out;
    if (n > 0) {
        std::vector<char> buf(static_cast<size_t>(n) + 1);
        std::vsnprintf(buf.data(), buf.size(), fmt, args);
        out.assign(buf.data(), static_cast<size_t>(n));
    }
    va_end(args);
    return out;
}

const Entry* entries(std::size_t& count) {
    count = sizeof(kEntries) / sizeof(kEntries[0]);
    return kEntries;
}

} // namespace i18n
