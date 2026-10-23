# Repository Current State (`STATE.md`)

**Active Working Branch:** `arena/e895b94f-tests`
**Last Updated:** 2026-10-23
**Head (remote):** `b08d933` at last sync; always `bash tools/sync_branch.sh` first.

## Turn protocol (do this literally)

1. `bash tools/sync_branch.sh` — restores history continuity after the
   cross-turn sandbox git reset (working files survive the reset, commit
   objects may not). Any uncommitted work reappears as normal diffs.
2. **Live-notes protocol** (Google Drive doc "Agent Notes - Tests repo", Drive
   connector `read_file_text`, id `1XRzQ4yhq2slvH1E2r2t9c7uK-vGcI1EBAxQOmSZArN8`;
   newest entries at the bottom):
   - Read it FIRST at the start of every turn, before any other action.
   - During long multi-commit turns, RE-READ at checkpoints: after every
     2-3 commits and once before the final push. The agent cannot be
     interrupted mid-edit; checkpoint polling is what guarantees a note
     added while work is in flight is caught before the turn ends.
   - Answer every note in the turn's final message (simple answer first).
3. Read `AGENTS.md` + this file + only the topic facts for the task.
4. Work, then ONE commit+push burst at turn end (push after every batch).
5. Update this file whenever project state changes (it survives resets; keep
   under 120 lines).

## Active Component Status

| Component | Path | Version | Status |
| :--- | :--- | :--- | :--- |
| **Project Hub** | `project-hub/src/project_hub.c` + `ProjectHub.exe` | 2.8.19 | **GREEN** — Honest `Activity · Up to Date|Update Ready` badges, updates restore apps as they were (trayed→trayed), quiet 60s ETag update checks + shared TLS session, cached-frame spin paint, HUD jar swap is write-first, Arena Link card removed (5 cards) |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` + `BluetoothGuard.exe` | 2.8.7 | **GREEN** — Device list = only devices connected before (paired/connected scan + BTHPORT service-record evidence + rules-ghost hiding) |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `KeyboardLighting.exe` + `Aura-Background.ps1` | 2.8.7 | **GREEN** — M4/M5 taps paced by engine; **hold modes** in `hold_mode.txt` (`KeyboardLighting.exe -HoldMode latch\|burst\|tap`, hot-swap): **latch = default** (press once to glide 5%/150ms, press again to stop; hardware sends ONE event per press so this is the closest real hold); seamless 52px tray emblem; G-Helper `Custom` + `ASUS services Stop` required |
| **Brand / spin assets** | `docs/brand/`, `tools/generate_suite_icons.py` | 2026-10-22 | **GREEN** — `tri-spoke-spin-1024.webp` (full alpha quality master) + `-1024.gif` (narrow-band edge dither) + `spin.html` backdrop-switcher preview. Desktop viewers flatten transparency (the "brown background"); browsers + in-app emblem are the true-alpha surfaces |
| **Agent tooling** | `AGENTS.md`, `tools/sync_branch.sh`, `tools/build_windows.py`, `tools/check_all.py` | 2026-10-22 | **GREEN** — One-shot build (~2s cached, ~130s cold), full suite (~4s cached, ~45s cold), icon regen (~80–130s) |

## Fast Verification & Build Commands

- Build all three Windows EXEs: `python3 tools/build_windows.py`
- All verification suites: `python3 tools/check_all.py`
- Icons + spin: `/home/user/.venv/bin/python3 tools/generate_suite_icons.py` (needs numpy+Pillow in `/home/user/.venv`)
- Sync: `bash tools/sync_branch.sh`

## Open Threads / Next Actions

1. **PR #10** open on `arena/e895b94f-tests` — every session commit+push updates it.
2. **User validation round (2026-10-23):** background apps auto-start to the
   tray (KBL unified task + DNS Pilot Run key; hub stays manual), DNS Pilot
   rank test is parallel (seconds, not minutes), spin repaints use the frame
   cache, Arena Link removed per notes decision.
3. Downloads are cache-busted `?v=<commit>`; new assets use NEW FILENAMES
   (`-1024`) so stale caches cannot serve old pixels.
