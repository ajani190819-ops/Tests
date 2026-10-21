# Windows Suite Architecture & Invariants

- Status: confirmed
- Last verified: 2026-10-21
- Read when: touching `project-hub/`, `bluetooth-guard/`, `keyboard-lighting/`, `arena-link-windows/`, or `docs/brand/`

## Fast One-Shot Commands

- **Compile `ProjectHub.exe` + `BluetoothGuard.exe` in parallel (~1.5s):** `python3 tools/build_windows.py`
- **Regenerate all `.ico` / `.png` suite icons (~1.5s):** `/home/user/.venv/bin/python tools/generate_suite_icons.py`
- **Run all 13 verification suites concurrently (~4s):** `python3 tools/check_all.py`
- **External caches only:** Always keep Python packages in `/home/user/.venv` and Zig caches in `/home/user/.cache/zig` — never inside `/home/user/Tests`.

## Critical Technical Facts (Never Break These)

1. **`BluetoothGuard.exe` (`bluetooth-guard/src/bluetooth_guard.c`):**
   - **Never call `BluetoothSetServiceState(..., BLUETOOTH_SERVICE_DISABLE)`** — disabling profiles unloads Windows audio endpoints and permanently breaks manual `"Connect"` in Windows Quick Settings & Settings.
   - Always keep `EnsureProfilesEnabled()` (`BLUETOOTH_SERVICE_ENABLE`) + CoreAudio `IKsControl::KsProperty` `KSPROPERTY_BT_AUDIO` (`{7FA06C40-B8F6-4C7E-8556-E8C33A12E54D}`, `KSPROPERTY_ONESHOT_RECONNECT = 0`, `KSPROPERTY_ONESHOT_DISCONNECT = 1`) + `IOCTL_BTH_DISCONNECT_DEVICE` (`0x0041000C`).
   - **Zero-delay boot & real-time intercept:** `wWinMain` immediately sets `HIGH_PRIORITY_CLASS`, disables Windows 11 EcoQoS (`PROCESS_POWER_THROTTLING_EXECUTION_SPEED`), runs `PollPairedDevices()` synchronously at millisecond 0 before window creation, listens to kernel `WM_DEVICECHANGE` (`GUID_BTHPORT_DEVICE_INTERFACE` + radio `DEV_BROADCAST_HANDLE`), and configures both `HKCU\...\Explorer\Serialize\StartupDelayInMSec = 0` and the `Tests_BluetoothGuard_FastBoot` `ONLOGON` (`PT0S`, `Priority = 4`, battery-allowed) Scheduled Task.
2. **`ProjectHub.exe` (`project-hub/src/project_hub.c`):**
   - Keep prominent top-bar **`Update Hub`** self-update button (`WorkerSelfUpdateHub` swaps `ProjectHub.exe` -> `ProjectHub.exe.old` atomically via `MoveFileExW` and relaunches). Bump `HUB_VERSION` whenever shipping a new `ProjectHub.exe` build so existing users see `Update Hub ▲` immediately.
   - `tests/test_windows_apps.py` asserts exact project names (`Keyboard Lighting`, `Bluetooth Guard`, `Spatial HUD`, `OrcaSlicer Plugins`, `Arena Link`), `SmoothRoundRect`, `RES_ICON_EMBLEM`, and valid multi-size `.ico` headers.
   - Load `dwmapi.dll` dynamically via `LoadLibraryW(L"dwmapi.dll")` so static PE imports remain clean.
3. **`Keyboard Lighting` (`keyboard-lighting/`):**
   - **Strict 7-bit ASCII only** in all `.cs`, `.ps1`, and `.bat` files (`tests/test_windows_apps.py` enforces pure ASCII + C# 5.0 syntax + no `$Var::Member` + flat `goto` labels in `.bat` files).
   - Delete stale `Ui.dll` during updates so `Tray.ps1` recompiles updated `ui_controls.cs.txt`.
4. **UI & Branding (`docs/UI_STYLE_GUIDE.md`):**
   - Soft Slate palette (`#181B24` canvas, `#222734` cards, `#2B3142` controls), rounded geometry (`DWMWCP_ROUND = 2` windows, `14px` cards, `9-11px` buttons, capsule pills), **Tri-Spoke Core Emblem** everywhere (**never** the old 8-point compass), and unified squircle `.ico` app icons across all apps.
