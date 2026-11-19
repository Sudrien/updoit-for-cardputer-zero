# Updoit for CardputerZero: Requirements

A 320x170 LVGL app that runs apt updates on the device. Keyboard-driven (46-key matrix).

## v0.1.0 (in scope)

1. **Periodic `apt update`** in the background, modelled on DietPi's
   `CONFIG_CHECK_APT_UPDATES` (daily cron job, result cached for the login banner):
   - A root systemd timer (daily, randomized delay) runs `apt-get update` and writes the
     upgradable count/list to `/etc/updoit.status`. The app only reads this file.
   - Setting `UPDOIT_CHECK_APT_UPDATES`: `0` = off, `1` = check only (default),
     `2` = check and upgrade automatically (planned).
2. **Operation picker** when the app is opened:
   - `apt upgrade`
   - `apt full-upgrade`
   - `apt autoremove`
3. **Confirm screen first**: lists the upgradable packages (count + scrollable names);
   Enter proceeds, Esc cancels.
4. **Sudo password prompt** when the app is selected from the launcher. The password is
   entered on-device, passed to `sudo -S` over stdin, kept only in memory, and never
   written to disk or logs. (Agreed.)
5. **Progress bars**: overall, per-download (from aria2) and install (from dpkg
   `--status-fd`), plus a scrolling log tail and a final success/fail state.
6. **Parallel downloads** via `aria2` (package dependency), apt-fast style: the download
   list comes from `apt-get --print-uris`, aria2c fetches into `/var/cache/apt/archives`
   and checksums are verified against the URIs' hashes.
7. **Download first, then install**: all downloads must finish and verify before
   `apt-get <op>` runs, so a dropped connection leaves the system untouched (same safety
   model as apt).

## Planned (not in v0.1.0)

- Battery / power guard (warn or refuse below a threshold unless on USB-C).
- Reboot prompt when `/var/run/reboot-required` exists.
- Network check before starting.
- Auto-upgrade (`UPDOIT_CHECK_APT_UPDATES=2`).
- **Pipelined download + install** as an opt-in setting (default stays
  download-everything-first). Trade-off: faster, but a dropped connection mid-run leaves
  a partial (consistent but mixed) upgrade until the run is finished; per-package dpkg
  calls also lose apt's batched trigger handling. Possible middle ground: start installing
  only once most downloads are done. Draft design below.

### Pipeline design (draft, future)

Two worker threads plus the UI thread:

- **Plan**: `apt-get -s <op>` gives apt's own ordered sequence of `Inst`/`Conf`/`Remv`
  actions. That order already accounts for Pre-Depends and immediate configuration, so
  the pipeline follows it exactly instead of computing its own.
- **Download thread**: runs aria2c (parallel connections), with files requested in plan
  order so the installer is blocked as little as possible. Reports per-file progress.
- **Install thread**: walks the plan in order. For `Inst` it waits until that `.deb` is
  downloaded and verified, then runs `dpkg --unpack`; `Conf` runs `dpkg --configure`;
  `Remv` runs `dpkg --remove`. It never runs ahead of the plan.
- **Locking**: the app takes `/var/lib/dpkg/lock-frontend` for the whole run and calls
  dpkg with `DPKG_FRONTEND_LOCKED=1`, so the background timer or another apt cannot slip
  in between steps.
- **Bookkeeping apt normally does**: new dependencies pulled in by `full-upgrade` are
  marked with `apt-mark auto`; `DPkg::Pre-Invoke`/`Post-Invoke` hooks are run around the
  install phase; a final `dpkg --configure -a` catches deferred triggers.
- **Failure**: on any dpkg error, the download thread is stopped, no further plan steps
  run, and the app shows the error with a "repair" action (`dpkg --configure -a` then
  `apt-get -f install`).
- **Fallback**: if the plan cannot be parsed, run aria2 downloads to completion, then a
  plain `apt-get <op>` (no pipelining).

## Open questions

- Whether to skip the background check on battery.
- Confirm `aria2` is available for the device's Debian release (arm64).
- Whether the launcher passes keyboard focus cleanly for password entry.

## Manifest implications (`app-builder.json`)

- `permissions.network = true`
- `permissions.background_service = true` (periodic update)
