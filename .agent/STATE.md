# Current state

- Last updated: 2026-10-20
- Session branch: `arena/e895b94f-tests`
- Read when: starting any task in this repository.

## Completed in this pass (verified)

1. **Agent workflow & repo hygiene (`AGENTS.md`, `.gitignore`, `archive/`, `docs/`):**
   - Added **Section 0 (Starter Step)** to `AGENTS.md` instructing every AI session to translate casual/dictated prompts into structured task buckets (`What I Understood`, `Do now`, `Do next`, `Boundaries`) before acting.
   - Repaired corrupted lines and outdated rules in `AGENTS.md`, removed committed crash logs/screenshots from root, scrubbed personal paths (`C:\Users\<user>`) and usernames across tracked files, archived superseded root mod folders under `archive/`, and expanded `.gitignore`.
2. **OrcaSlicer plugins (`Wave Overhangs 0.0.51`, `Unlayered Infill 0.4.10`) & `Orca-Plugins.bat` (`2.2.0`):**
   - Fixed settings auto-restore when Orca resets a plugin config to `{}`, fixed mid-session `restore_backup: true` on slice, stopped `WaveOverhangsCheck` from polluting `settings_backups` with factory defaults, and fixed `"enabled": "false"` / `"wall_snap": "false"` string parsing.
   - Hardened `check_nozzle_clearance()` (inline `;` comments, negative coordinates via `math.floor`, `M82` retractions, `1.25x` note fix), added `"auto"` flag support on the standalone CLI, and made G-code writes atomic.
   - Cached immutable GitHub branch commit dates by SHA in `Orca-Plugins.bat` (`2.2.0`), filtered `ci-reports`, rejected HTML error pages in `:download_check`, verified `TOOL_VERSION` before staging standalone tools, and prevented `:self_update` downgrades.
   - Synced all README/ROADMAP versions automatically via `tools/bump_version.py --check-docs`.
3. **Spatial HUD (`spatial-hud-template-26.3`) & Arena Link (`arena-link-windows`):**
   - Fixed 20 Hz world-space HUD stutter in `WorldSpaceHudRenderer.java` using `partialTick` interpolation, replaced closed-source `close()` assumption with verified Minecraft 26.3 `RenderSystem.getDevice().releaseGpuResource(...)` buffer cleanup in `ExperimentalHudCapture.java`, pointed `.bat` updaters to `main`, and added headless JUnit 5 tests.
   - Fixed forgetting deleted/moved folders, sidebar persistence of missing folders, 0-commit `git init` repos, Windows `.cmd` Claude CLI detection, Electron navigation locks, privacy wording, and `node:test` coverage in `arena-link-windows`.
4. **Windows Desktop Suite (`keyboard-lighting/`, `bluetooth-guard/`, `project-hub/`):**
   - **`keyboard-lighting/`:** Eliminated sleep/wake/tray deadlocks and UI freezes by replacing all `Register-ObjectEvent` subscriptions on `Microsoft.Win32.SystemEvents` with lock-free atomic C# watchers (`[KbLight.PowerWatch]` & `[AuraPowerWatch]`), replacing slow WMI `Win32_Process` queries with `engine.pid` + `running.lock` PID checks, moving process teardown and update checks off the WinForms UI thread, adding a 1.5s `show.flag` responsiveness handshake + automatic hung-instance recovery, adding `Restart.bat` + `-Stop`/`-Restart` flags, and migrating self-update URLs to `ajani190819-ops/Tests/main/keyboard-lighting`.
   - **`bluetooth-guard/`:** Built native Windows Bluetooth auto-connect blocker & quick-connect tray app using `BluetoothAPIs.dll` (`BluetoothSetServiceState` `DISABLE`/`ENABLE`) + `IOCTL_BTH_DISCONNECT_DEVICE` (`0x0041000C`) on a background runspace worker, with per-device **Allow auto-connect** toggles and session-scoped **Connect Now** that auto-relocks on disconnect/sleep/exit.
   - **`project-hub/`:** Built unified Windows desktop Control Center (`project-hub\Setup.bat`) showing live status, versions, and one-click Launch / Restart / Update actions across all 5 projects in this repository.
   - **`tests/test_windows_apps.py` + `tools/check_all.py` + `docs/ci/ci.yml`:** Added full static verification for C# 5.0 syntax, PS 5.1 syntax, strict ASCII safety, pure CRLF `.bat` invariants, URL hygiene, and app reliability contracts.

## Active notes

- Note on GitHub Actions workflow permission: Arena's GitHub App token does not hold `workflows` scope, so `.github/workflows/*` remains untouched on git pushes and updated workflow templates live in `docs/ci/`.
- Remote branch `origin/arena/b4016c28-tests` contains experimental Spatial HUD Method 4 work (mixed with raw logs and an unresolved `WorldSpaceSolidQuad.java:299` vertex crash). Reconcile selectively only when resuming Spatial HUD Method 4 work.
