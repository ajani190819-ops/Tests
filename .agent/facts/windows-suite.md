# Windows Suite Architecture & Invariants

- Status: confirmed
- Last verified: 2026-11-16
- Read when: touching `project-hub/`, `bluetooth-guard/`, `keyboard-lighting/`, `dns-pilot/`, or `docs/brand/`

## Fast One-Shot Commands

- **Cross-build seven apps + standalone App Review:** `python3 tools/build_windows.py` (cached
  Zig; `/home/user/.venv`, caches under `/home/user/.cache`).
- **Run all verification suites:** `python3 tools/check_all.py` (~4s warm, ~45s cold)
- **Regenerate icons + spin:** `/home/user/.venv/bin/python3 tools/generate_suite_icons.py` (~80–130s; only when brand assets change)
- **Git sync (start of every turn):** `bash tools/sync_branch.sh`
- **External caches only:** Python packages in `/home/user/.venv`, Zig caches in `/home/user/.cache/zig` — never inside `/home/user/Tests`.

## Hot File Landmarks (stop grepping; these are the anchors)

Line numbers drift — search for the symbol names.

| File | Symbols / what lives where |
|---|---|
| `keyboard-lighting/src/keyboard_lighting.c` | `wWinMain` (CLI: `-Silent`, `-Stop`, `-BrightUp/-BrightDown` ms-token writes), `InitKblPaths` (all `%LOCALAPPDATA%\KeyboardLighting\` paths), `brightness_controller.h`/`brightness_master.c`/`panel_config.c` (exact master, input arbitration, scoped three-way preference saves); `RegisterBrightnessHotkeys`/`WM_HOTKEY` (Ctrl+Alt+Up/Down, `[Keys]` in presets.ini), `ParseLayoutTxt`/`PollLayoutTxt` (ring preview zone order from layout.txt) |
| `keyboard-lighting/Aura-Background.ps1` | `Set-Live` (smooth master 0..1000), native-owned engine mirrors exact live.txt (no token/rounded echo); standalone legacy token path retained, stop.flag, repair scheduler, `PinGate(3)` every tick, WMI `AuraAtk` handler (Fn events 0xC4/0xC5/0xC7 + fnstamp.txt 200ms dedupe), `PublishLayout` (layout.txt ground truth) |
| `project-hub/src/project_hub.c` | `HUB_VERSION`/`HUB_VERSION_W` (bump both when shipping), `BranchCfgPath`/`LoadBranchCfg`/`SaveBranchCfg`/`EnsureActiveBranch` (branch-picker updater, `config.ini`), `ShowBranchMenu`/`PromptBranchName`/`SetActiveBranch`, `HandleActionCommand` (all button IDs), `g_topBtns[5]` (Refresh / Update Hub / Update All / Branch / Settings) |
| `project-hub/src/parts/settings_page.c` | Soft Slate Settings window, hold mode, keyboard Balance/Steady Colours, DNS interval, branch-follow preference; Keyboard Lighting updates persisted `panel.json` and live `theme.json` atomically and rolls back the saved file if live apply fails |
| `project-hub/src/parts/app_activation.c` | `HubOpenApp` is the Hub UI-thread card/tray show-or-launch route. Native classes/mutexes and catalogue image-PID lookup avoid duplicate starts; async restore/show preserves maximized placement and targets active modal popups. Timer 3 retries for <=10s after shell return; newer user activity/request cancels focus. No synthetic input, topmost, input-queue attachment, or elevation change. Windows checks: `.agent/runbooks/test-hub-open-to-front.md`. |
| `keyboard-lighting/Tray.ps1` | `Load-Cfg`/`Save-Cfg` preserve false values with recursive baseline merging and atomic writes; adopted external preferences refresh existing controls before later UI saves |
| `shared/win32/suite_emblem.c` | Canonical native Tri-Spoke geometry, transparent 32-bit DIB alpha compositing, cache lifecycle, 30-second angle helper, and dark/light palette selection. Project Hub, Agent Notes, Audio Switcher, and Weather Radar include it once from their unity roots; adapters may position/color it but never copy its renderer. `SuiteEmblemDibPixel` is mandatory for a raw emblem DIB background, and every consuming target must be listed in `tools/build_windows.py` `TARGET_SHARED_SOURCES`. |
| `bluetooth-guard/src/bluetooth_guard.c` | `DeviceEntry` trusted/review/denied/forgotten state; `device_policy.c` holds testable approval/drop rules. `LoadRules`/`SaveRules` use K/D/F records. Paired candidates stay reviewable without automatic trust; UI actions use stable device addresses |
| `tools/generate_suite_icons.py` | `trispoke_sdf_alpha` (the emblem math), `build_icon_*` (per-size analytic ICO frames), `export_spin_loops` (1024² spin: `.webp` full alpha + `.gif` narrow-band dither + `spin.html`) |

## Critical Technical Facts (Never Break These)

1. **`BluetoothGuard.exe`:**
   - **Never call `BluetoothSetServiceState(..., BLUETOOTH_SERVICE_DISABLE)`** — always `EnsureProfilesEnabled()` + CoreAudio `KSPROPERTY_ONESHOT_RECONNECT/DISCONNECT` + `IOCTL_BTH_DISCONNECT_DEVICE` (`0x0041000C`).
   - Zero-delay boot: `HIGH_PRIORITY_CLASS`, EcoQoS off, sync device sweep before window creation, `WM_DEVICECHANGE` listening, `Tests_BluetoothGuard_FastBoot` ONLOGON task (`PT0S`, `Priority=4`).
   - Native paired discovery creates visible **Needs approval** rows, even when disconnected; it never grants trust. Undecided pairs are not dropped before a decision. **Accept** allows one session; **Auto-Connect** explicitly approves future links. **Deny** persists D, blocks connections, and keeps Windows pairing; **Forget** persists F and unpairs. Existing K and F records remain valid. Unknown unpaired links stay blocked; queued policy drops recheck approval by stable address. The legacy PowerShell setup path is unchanged.
2. **`ProjectHub.exe`:**
   - Keep the **`Update Hub`** self-update; bump `HUB_VERSION` **and** `HUB_VERSION_W` with every shipped binary.
   - Updater follows the user-selected branch (`Branch` button); `tests/test_windows_apps.py` asserts project names, `SmoothRoundRect`, `RES_ICON_EMBLEM`, multi-size `.ico` headers.
   - Longevity: owned catalogue icons swap/discard through `ReplaceCardIcon` under the draw lock; failed loads retain the last good handle. The two-second tray poll probes the shell, restores missing icons, and releases the host marker on unconfirmed registration. Quit/removal cannot re-register. Mocked lifetime tests are not a native soak or proof of the owner's original expiry symptom; see `.agent/runbooks/test-hub-longevity.md`.
   - Grouped cards keep stable slots: 0 BTG, 1 KBL, 2 HUD, 3 Undahang (`orca`), 4 DNS, 5 Notes, 6 Audio, 7 Infill; extras start at `BUILTIN_CARD_COUNT`. `orca_plugins.c` owns independent routes/opt-out migration. Icon cache slots differ: 3 Infill, 7 Weather, 8 Undahang; `HubCardIcon` selects resource identity before positional fallback. Native checklist: `.agent/runbooks/test-hub-grouped-entries.md`.
   - Load `dwmapi.dll` dynamically so static PE imports stay clean. PE whitelist: no hid.dll/setupapi.dll/WIC imports.
3. **Keyboard Lighting:**
   - **Strict 7-bit ASCII** in all `keyboard-lighting/` text files (enforced by tests). The app is C11 (`src/keyboard_lighting.c`) + PowerShell engine (`Aura-Background.ps1`); `project-hub/` + `bluetooth-guard/` I/O is latin-1 safe.
   - M4/M5: `Custom` runs `KeyboardLighting.exe -BrightDown/-BrightUp` (0 down, 1 up). Native master consumes each press once; raw reports supply release/hold evidence. Keep 5% tap, 1% down/1.5% up per 30ms, 120ms freshness, 10s ceiling and distinct bindings. `-NativeBrightness` engines only mirror master; real Fn/standalone legacy actions can publish. Preserve the pinned EC gate (`PinGate(3)`).
   - Saved Balance (`Equalise`) is per-zone; Steady (`Smooth`) is global and restored before first render. Preserve all eight combinations, legacy files/unknown fields and three-way external edits. `MasterLevel` retains exact half-percent steps with compatible legacy `Brightness` projection. Native EXE RCDATA3 bundles its matching Aura script; never fetch an arbitrary branch-tip engine. PowerShell 5.1 `File.Replace` needs explicit `NullString.Value` for its null backup path, not plain `$null`. Automated/mock tests are not physical acceptance; `.agent/runbooks/test-lighting-input.md`.
   - Crash-isolation discipline: one app-C change per commit; `0edce0a` is the pure baseline.
4. **UI & Branding (`docs/UI_STYLE_GUIDE.md`):** Soft Slate (`#181B24`), Tri-Spoke Core (never the old compass). Native header emblems float on transparency. Per-app icons retain approved slate-medallion family art with transparent outer margins. Undahang's approved `icon-undahang-wave-v1.png` master is read-only during regeneration; Infill art stays unchanged.

## Git / Sandbox Gotchas

- Run `bash tools/sync_branch.sh` each turn; sandbox Git metadata may reset.
- Commit/push only the active Arena branch; never main or force-push.
- Download links need cache-busting `?v=<commit>`; when pixels change, prefer NEW FILENAMES (stale GitHub/Windows caches otherwise serve old art).
