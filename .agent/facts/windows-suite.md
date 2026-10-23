# Windows Suite Architecture & Invariants

- Status: confirmed
- Last verified: 2026-10-22
- Read when: touching `project-hub/`, `bluetooth-guard/`, `keyboard-lighting/`, `dns-pilot/`, or `docs/brand/`

## Fast One-Shot Commands

- **Build all three EXEs** (`ProjectHub.exe`, `BluetoothGuard.exe`, `KeyboardLighting.exe`): `python3 tools/build_windows.py` (~2s warm zig cache, ~130s cold; compiler is `python -m ziglang cc -target x86_64-windows-gnu` from `/home/user/.venv`)
- **Run all verification suites:** `python3 tools/check_all.py` (~4s warm, ~45s cold)
- **Regenerate icons + spin:** `/home/user/.venv/bin/python3 tools/generate_suite_icons.py` (~80–130s; only when brand assets change)
- **Git sync (start of every turn):** `bash tools/sync_branch.sh`
- **External caches only:** Python packages in `/home/user/.venv`, Zig caches in `/home/user/.cache/zig` — never inside `/home/user/Tests`.

## Hot File Landmarks (stop grepping; these are the anchors)

Line numbers drift — search for the symbol names.

| File | Symbols / what lives where |
|---|---|
| `keyboard-lighting/src/keyboard_lighting.c` | `wWinMain` (CLI: `-Silent`, `-Stop`, `-BrightUp/-BrightDown` ms-token writes), `InitKblPaths` (all `%LOCALAPPDATA%\KeyboardLighting\` paths), `StepMasterBrightness`/`RegisterBrightnessHotkeys`/`WM_HOTKEY` (global Ctrl+Alt+Up/Down hotkeys, `[Keys]` in presets.ini), `ParseLayoutTxt`/`PollLayoutTxt` (ring preview zone order from layout.txt) |
| `keyboard-lighting/Aura-Background.ps1` | `Set-Live` (smooth master 0..1000), main loop ~60fps: stop.flag, M4/M5 paced taps (150ms), repair scheduler, `PinGate(3)` every tick, WMI `AuraAtk` handler (Fn events 0xC4/0xC5/0xC7 + fnstamp.txt 200ms dedupe), `PublishLayout` (layout.txt ground truth) |
| `project-hub/src/project_hub.c` | `HUB_VERSION`/`HUB_VERSION_W` (bump both when shipping), `BranchCfgPath`/`LoadBranchCfg`/`SaveBranchCfg`/`EnsureActiveBranch` (branch-picker updater, `config.ini`), `ShowBranchMenu`/`PromptBranchName`/`SetActiveBranch`, `HandleActionCommand` (all button IDs), `EmbCacheEnsure`/`EmbSpinDraw` (in-app spinning emblem, 30s/rev), `g_topBtns[4]` (Refresh / Update Hub / Update All / Branch) |
| `bluetooth-guard/src/bluetooth_guard.c` | `DeviceEntry`, `HasConnectedBefore` (BTHPORT service-record evidence), `LoadRules`/`SaveRules` (`rules.txt` policy only), OS scan (`BluetoothFindFirstDevice`, authenticated/connected only), paint snapshot `visCount` filter (hidden rows keep raw hit indices) |
| `tools/generate_suite_icons.py` | `trispoke_sdf_alpha` (the emblem math), `build_icon_*` (per-size analytic ICO frames), `export_spin_loops` (1024² spin: `.webp` full alpha + `.gif` narrow-band dither + `spin.html`) |

## Critical Technical Facts (Never Break These)

1. **`BluetoothGuard.exe`:**
   - **Never call `BluetoothSetServiceState(..., BLUETOOTH_SERVICE_DISABLE)`** — always `EnsureProfilesEnabled()` + CoreAudio `KSPROPERTY_ONESHOT_RECONNECT/DISCONNECT` + `IOCTL_BTH_DISCONNECT_DEVICE` (`0x0041000C`).
   - Zero-delay boot: `HIGH_PRIORITY_CLASS`, EcoQoS off, sync device sweep before window creation, `WM_DEVICECHANGE` listening, `Tests_BluetoothGuard_FastBoot` ONLOGON task (`PT0S`, `Priority=4`).
   - Device list shows ONLY devices connected before (scan filter + `HasConnectedBefore` + rules-ghost hiding) — user requirement.
2. **`ProjectHub.exe`:**
   - Keep the **`Update Hub`** self-update; bump `HUB_VERSION` **and** `HUB_VERSION_W` with every shipped binary.
   - Updater follows the user-selected branch (`Branch` button); `tests/test_windows_apps.py` asserts project names, `SmoothRoundRect`, `RES_ICON_EMBLEM`, multi-size `.ico` headers.
   - Load `dwmapi.dll` dynamically so static PE imports stay clean. PE whitelist: no hid.dll/setupapi.dll/WIC imports.
3. **Keyboard Lighting:**
   - **Strict 7-bit ASCII** in all `keyboard-lighting/` text files (enforced by tests). The app is C11 (`src/keyboard_lighting.c`) + PowerShell engine (`Aura-Background.ps1`); `project-hub/` + `bluetooth-guard/` I/O is latin-1 safe.
   - M4/M5 brightness: G-Helper `Custom` rows run `KeyboardLighting.exe -BrightDown/-BrightUp`; engine paces taps (1 step/150ms, hold = smooth sweep). G-Helper's native 4-step gate must stay pinned (`PinGate(3)`).
   - Crash-isolation discipline: one app-C change per commit; `0edce0a` is the pure baseline.
4. **UI & Branding (`docs/UI_STYLE_GUIDE.md`):** Soft Slate palette (`#181B24` canvas), Tri-Spoke Core Emblem everywhere (never the old 8-point compass), icons float on full transparency (the old opaque squircle plate read as a "muddy warm-brown blob" — never bring it back).

## Git / Sandbox Gotchas

- The Arena sandbox resets `.git` between turns (files survive, commit objects may not): run `bash tools/sync_branch.sh` first thing every turn.
- All commits/pushes only on `arena/<id>-tests`; never `main`, never force-push.
- Download links need cache-busting `?v=<commit>`; when pixels change, prefer NEW FILENAMES (stale GitHub/Windows caches otherwise serve old art).
