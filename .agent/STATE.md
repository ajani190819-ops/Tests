# Repository Current State (`STATE.md`)

**Active Working Branch:** `arena/e895b94f-tests`
**Last Updated:** 2026-10-21

## Active Component Status

| Component | Path | Version | Build / Test Status |
| :--- | :--- | :--- | :--- |
| **Project Hub (`ProjectHub.exe`)** | `project-hub/` | `2.4.0` | **GREEN** — Soft rounded slate UI (`SmoothRoundRect`), Tri-Spoke Core Emblem, embedded suite `.ico` icons, `Update Hub` self-update, and automatic background activation of `BluetoothGuard.exe --silent` after updates |
| **Bluetooth Guard (`BluetoothGuard.exe`)** | `bluetooth-guard/` | `2.4.0` | **GREEN** — Instant high-priority boot & real-time intercept: `HIGH_PRIORITY_CLASS`, EcoQoS disabled, millisecond-0 pre-UI sweep, kernel `RegisterDeviceNotificationW` (`WM_DEVICECHANGE` <1ms wake), `80ms` boot/wake burst guard, `Tests_BluetoothGuard_FastBoot` `PT0S` `ONLOGON` Scheduled Task (battery-allowed, `Priority=4`), `Explorer\Serialize\StartupDelayInMSec=0`, pre-shutdown/Fast-Startup cleanup, and reliable manual connect |
| **Keyboard Lighting (`Tray.ps1` + `ui_controls.cs.txt`)** | `keyboard-lighting/` | `1.3.0` | **GREEN** — Soft Slate palette, `14px` rounded cards, `11px` rounded controls, anti-aliased `Theme.DrawSuiteEmblem` title bar, and suite `app.ico` |
| **Agent Fast Execution & Build Tooling** | `AGENTS.md`, `CLAUDE.md`, `tools/build_windows.py`, `tools/check_all.py` | `2.0.0` | **GREEN** — Parallel tool protocol, parallel ~1.5s Win32 builder (`build_windows.py`), and concurrent ~4s verification runner (`check_all.py`) |
| **OrcaSlicer Plugins & Installer** | `Orca-Plugins.bat`, `plugins/` | `2.2.0` / `0.0.51` / `0.4.10` | **GREEN** — Pure CRLF, 100% unit & audit tests passing |
| **Arena Link & Spatial HUD** | `arena-link-windows/`, `spatial-hud-template-26.3/` | `0.1.0` / `1.0.2+mc26.3` | **GREEN** — All Node & Gradle checks passing |

## Fast Verification & Build Commands

- Build both Windows `.exe` apps in parallel (~1.5s): `python3 tools/build_windows.py`
- Run all repository tests concurrently (~4s): `python3 tools/check_all.py`
