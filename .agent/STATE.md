# Repository Current State (`STATE.md`)

**Active Working Branch:** `arena/e895b94f-tests`
**Last Updated:** 2026-10-22
**Head (remote):** `b08d933` at last sync; always `bash tools/sync_branch.sh` first.

## Turn protocol (do this literally)

1. `bash tools/sync_branch.sh` — restores history continuity after the
   cross-turn sandbox git reset (working files survive the reset, commit
   objects may not). Any uncommitted work reappears as normal diffs.
2. Read `AGENTS.md` + this file + only the topic facts for the task.
3. Work, then ONE commit+push burst at turn end (push after every batch).
4. Update this file whenever project state changes (it survives resets; keep
   under 120 lines).

## Active Component Status

| Component | Path | Version | Status |
| :--- | :--- | :--- | :--- |
| **Project Hub** | `project-hub/src/project_hub.c` + `ProjectHub.exe` | 2.8.6 | **GREEN** — Branch-picker updater (`Branch` button: live GitHub branch list + `Other branch...`, persisted in `%LOCALAPPDATA%\ProjectHub\config.ini`, default `main`), full version + active branch in header, in-app spinning Tri-Spoke emblem (30s/rev, 50ms timer), update/install workers for all apps |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` + `BluetoothGuard.exe` | 2.8.6 | **GREEN** — Device list = only devices connected before (paired/connected scan + BTHPORT service-record evidence + rules-ghost hiding) |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `KeyboardLighting.exe` + `Aura-Background.ps1` | 2.8.6 | **GREEN** — Ring preview follows layout.txt zone order; M4/M5 taps via `KeyboardLighting.exe -BrightUp/-BrightDown` (ms-token file) paced by engine at 1 step/150ms (screen-brightness hold feel); G-Helper `Custom` binding + `ASUS services Stop` required |
| **Brand / spin assets** | `docs/brand/`, `tools/generate_suite_icons.py` | 2026-10-22 | **GREEN** — `tri-spoke-spin-1024.webp` (full alpha quality master) + `-1024.gif` (narrow-band edge dither) + `spin.html` backdrop-switcher preview. Desktop viewers flatten transparency (the "brown background"); browsers + in-app emblem are the true-alpha surfaces |
| **Agent tooling** | `AGENTS.md`, `tools/sync_branch.sh`, `tools/build_windows.py`, `tools/check_all.py` | 2026-10-22 | **GREEN** — One-shot build (~2s cached, ~130s cold), full suite (~4s cached, ~45s cold), icon regen (~80–130s) |

## Fast Verification & Build Commands

- Build all three Windows EXEs: `python3 tools/build_windows.py`
- All verification suites: `python3 tools/check_all.py`
- Icons + spin: `/home/user/.venv/bin/python3 tools/generate_suite_icons.py` (needs numpy+Pillow in `/home/user/.venv`)
- Sync: `bash tools/sync_branch.sh`

## Open Threads / Next Actions

1. **User testing (in flight):** hold-to-repeat feel of M4/M5 brightness; device-list filtering on their machine; G-Helper `Custom` strings (`"...\KeyboardLighting.exe" -BrightDown/-BrightUp` on rows `M4:` / `M5/ROG:`) + `Stop` on "Asus Services Running".
2. **Awaiting user direction:** the icon/spin conversation continues after this refactor (per-size visuals, where the spin should appear next).
3. **G-Helper gate-lock confirmation** still outstanding from earlier Setup.bat re-run.
4. Downloads are cache-busted `?v=<commit>`; new assets use NEW FILENAMES (`-1024`) so stale caches cannot serve old pixels.
