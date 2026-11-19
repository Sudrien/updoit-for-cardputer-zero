/*
 * SPDX-License-Identifier: MIT
 */

// Minimal self-contained tests for the apt parsers (no LVGL, no framework).

#include "apt_parse.h"
#include "status_cache.h"

#include <cstdio>
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

using namespace updater;

void test_simulation() {
    const std::string out =
        "Reading package lists...\n"
        "The following packages have been kept back:\n"
        "  fonts-crosextra-caladea linux-image-rpi-v8\n"
        "The following packages will be upgraded:\n"
        "  apt libapt-pkg6.0t64\n"
        "2 upgraded, 1 newly installed, 1 to remove and 2 not upgraded.\n"
        "Remv oldpkg [1.0-1]\n"
        "Inst libapt-pkg6.0t64 [2.8.3] (2.8.4ubuntu0.1 Ubuntu:24.04/noble-updates [amd64])\n"
        "Conf libapt-pkg6.0t64 (2.8.4ubuntu0.1 Ubuntu:24.04/noble-updates [amd64])\n"
        "Inst apt [2.8.3] (2.8.4ubuntu0.1 Ubuntu:24.04/noble-updates [amd64]) []\n"
        "Inst newdep (3.1-2 Debian:13/stable [arm64])\n";
    const Plan plan = parse_simulation(out);
    CHECK(plan.actions.size() == 5);
    CHECK(plan.upgrades() == 2);
    CHECK(plan.new_installs() == 1);
    CHECK(plan.removals() == 1);
    CHECK(plan.kept_back.size() == 2);
    CHECK(plan.kept_back[1] == "linux-image-rpi-v8");
    CHECK(plan.actions[0].kind == PlanAction::Kind::Remove);
    CHECK(plan.actions[0].old_version == "1.0-1");
    CHECK(plan.actions[1].package == "libapt-pkg6.0t64");
    CHECK(plan.actions[1].old_version == "2.8.3");
    CHECK(plan.actions[1].new_version == "2.8.4ubuntu0.1");
    CHECK(plan.actions[4].old_version.empty());
    CHECK(plan.actions[4].new_version == "3.1-2");
    CHECK(!plan.empty());
    CHECK(parse_simulation("0 upgraded, 0 newly installed\n").empty());
}

void test_print_uris() {
    const std::string out =
        "'http://deb.debian.org/debian/pool/main/a/apt/apt_2.6.1_arm64.deb' apt_2.6.1_arm64.deb 1396574 SHA256:abcdef\n"
        "'http://archive.ubuntu.com/x/libapt.deb' libapt.deb 985236 MD5Sum:63138e\n"
        "garbage line\n";
    const auto items = parse_print_uris(out);
    CHECK(items.size() == 2);
    CHECK(items[0].uri == "http://deb.debian.org/debian/pool/main/a/apt/apt_2.6.1_arm64.deb");
    CHECK(items[0].filename == "apt_2.6.1_arm64.deb");
    CHECK(items[0].size == 1396574);
    CHECK(aria2_checksum(items[0]) == "sha-256=abcdef");
    CHECK(aria2_checksum(items[1]) == "md5=63138e");
}

void test_status_lines() {
    StatusLine s;
    CHECK(parse_status_line("pmstatus:libgl1-mesa-dri:4.7619:Unpacking libgl1-mesa-dri (amd64)", s));
    CHECK(s.kind == StatusLine::Kind::PmStatus);
    CHECK(s.item == "libgl1-mesa-dri");
    CHECK(s.percent > 4.76 && s.percent < 4.77);
    CHECK(s.message == "Unpacking libgl1-mesa-dri (amd64)");

    CHECK(parse_status_line("pmstatus:libc6:arm64:50.0:Configuring libc6:arm64", s));
    CHECK(s.item == "libc6:arm64");
    CHECK(s.percent == 50.0);
    CHECK(s.message == "Configuring libc6:arm64");

    CHECK(parse_status_line("dlstatus:5:100.0000:Retrieving file 5 of 5", s));
    CHECK(s.kind == StatusLine::Kind::DlStatus);
    CHECK(s.percent == 100.0);

    CHECK(parse_status_line("pmerror:/var/cache/apt/archives/x.deb:30:trying to overwrite", s));
    CHECK(s.kind == StatusLine::Kind::PmError);

    CHECK(!parse_status_line("Get:1 http://archive.ubuntu.com noble/main", s));
    CHECK(!parse_status_line("Setting up apt (2.8.4) ...", s));
}

void test_status_cache() {
    const auto c = parse_status_cache("checked=1763560000\nupgradable=3\npackages=a b c\n");
    CHECK(c.valid);
    CHECK(c.checked == 1763560000);
    CHECK(c.upgradable == 3);
    CHECK(c.packages.size() == 3);
    const auto d = parse_status_cache("checked=5\nupdated=4\nupgradable=1\nfull_upgradable=7\nautoremovable=2\n");
    CHECK(d.updated == 4 && d.full_upgradable == 7 && d.autoremovable == 2);
    CHECK(parse_status_cache("upgradable=1\n").updated == 0);
    const auto e = parse_status_cache("upgradable=0\ncache_bytes=1048576\n");
    CHECK(e.has_cache_bytes && e.cache_bytes == 1048576);
    CHECK(!parse_status_cache("upgradable=0\n").has_cache_bytes);
    CHECK(!parse_status_cache("").valid);
}

void test_format_bytes() {
    CHECK(format_bytes(512) == "512 B");
    CHECK(format_bytes(2048) == "2 kB");
    CHECK(format_bytes(11 * 1024 * 1024 + 100000) == "11.1 MB");
}

} // namespace

int main() {
    test_simulation();
    test_print_uris();
    test_status_lines();
    test_status_cache();
    test_format_bytes();
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all updater tests passed\n");
    return 0;
}
