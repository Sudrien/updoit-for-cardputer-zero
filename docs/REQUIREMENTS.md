# Updoit for CardputerZero: Requirements

A 320x170 LVGL app that runs apt updates on the device. Keyboard-driven (46-key matrix).

## v1 (in scope)

1. **Periodic `apt update`** in the background (DietPi-style), so the app can show
   "N packages upgradable" without waiting. Interval: TBD (suggest daily + on network up).
2. **Operation picker** when the app is opened:
   - `apt upgrade`
   - `apt full-upgrade`
   - `apt autoremove`
3. **Confirm screen first**: lists the upgradable packages (count + scrollable names);
   Enter proceeds, Esc cancels.
4. **Sudo password prompt** when the app is selected from the launcher. The password is
   entered on-device, passed to `sudo -S` over stdin, kept only in memory, and never
   written to disk or logs.
5. **Progress bars**: overall and per-download, driven by apt's machine-readable status
   (`-o APT::Status-Fd=<fd>`), plus a scrolling log tail and a final success/fail state.
6. **Parallel downloads** enabled for the upgrade run via apt configuration.

## Planned (not in v1)

- Battery / power guard (warn or refuse below a threshold unless on USB-C).
- Reboot prompt when `/var/run/reboot-required` exists.
- Network check before starting.

## Open questions

- Background `apt update` mechanism: systemd timer running as root (no password needed)
  vs. in-app scheduling. A timer is preferred; the app then only reads the cached result.
- Update interval and whether to skip on battery.
- Whether the launcher passes keyboard focus cleanly for password entry.

## Manifest implications (`app-builder.json`)

- `permissions.network = true`
- `permissions.background_service = true` (periodic update)
