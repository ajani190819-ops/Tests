# Current state

- Last updated: 2026-10-20
- Session branch: `arena/e895b94f-tests`
- Read when: starting any task in this repository.

## Completed in this pass (verified)

1. **Agent workflow & repo hygiene (`AGENTS.md`, `.gitignore`, `archive/`, `docs/`):**
   - Added **Section 0 (Starter Step)** to `AGENTS.md` instructing every AI session to translate casual/dictated prompts into structured task buckets (`Do now`, `Do next`, `Boundaries`, `Decisions needed`) before acting.
   - Repaired corrupted lines and outdated rules in `AGENTS.md`, removed committed crash logs/screenshots from root, scrubbed personal paths (`C:\Users\<user>`) and usernames across tracked files, archived superseded root mod folders under `archive/`, and expanded `.gitignore`.
2. **OrcaSlicer plugins (`Wave Overhangs 0.0.51`, `Unlayered Infill 0.4.10`) & `Orca-Plugins.bat` (`2.2.0`):**
   - Fixed settings auto-restore when Orca resets a plugin config to `{}`, fixed mid-session `restore_backup: true` on slice, stopped `WaveOverhangsCheck` from polluting `settings_backups` with factory defaults, and fixed `"enabled": "false"` / `"wall_snap": "false"` string parsing.
   - Hardened `check_nozzle_clearance()` (inline `;` comments, negative coordinates via `math.floor`, `M82` retractions, `1.25x` note fix), added `"auto"` flag support on the standalone CLI, and made G-code writes atomic.
   - Cached immutable GitHub branch commit dates by SHA in `Orca-Plugins.bat` (`2.2.0`), filtered `ci-reports`, rejected HTML error pages in `:download_check`, verified `TOOL_VERSION` before staging standalone tools, and prevented `:self_update` downgrades.
   - Synced all README/ROADMAP versions automatically via `tools/bump_version.py --check-docs` and wired checks into `.github/workflows/ci.yml`.
3. **Spatial HUD (`spatial-hud-template-26.3`) & Arena Link (`arena-link-windows`):**
   - Fixed 20 Hz world-space HUD stutter in `WorldSpaceHudRenderer.java` using `partialTick` interpolation, made `ExperimentalHudCapture`'s warp buffer lazy with full shutdown cleanup, pointed `.bat` updaters to `main`, added JUnit 5 tests, and added `concurrency:` + safe branch release publishing in `.github/workflows/build-spatial-hud.yml`.
   - Fixed forgetting deleted/moved folders, sidebar persistence of missing folders, 0-commit `git init` repos, Windows `.cmd` Claude CLI detection, Electron navigation locks, privacy wording, and `node:test` coverage in `arena-link-windows`.

## Queued next (owner-requested follow-ups, in order)

1. **`keyboard-lighting/` crash & stuck system-tray Quit/Restart fix:**
   - Symptom: Keyboard Lighting occasionally freezes or crashes; right-clicking the tray icon does not open the menu / let the owner click **Quit**, preventing a clean restart.
   - Root causes identified in `keyboard-lighting/Tray.ps1`:
     - If `powershell.exe` exits abruptly without calling `$tray.Visible = $false; $tray.Dispose()`, Explorer leaves a dead ghost icon in the tray AND `Test-AlreadyRunning` checks only `running.lock` file age (`< 15s`) instead of checking whether the PID inside `running.lock` is alive (`Get-Process -Id $pid`), blocking restarts.
     - If the elevated WinForms thread hangs or blocks on synchronous work, right-click messages stop pumping and launching the shortcut only writes `show.flag` which the hung thread never reads.
   - Planned fix next turn: add live PID verification + automatic stuck-process kill/relaunch (`Restart-KeyboardLighting.bat` / `-Restart` / unresponsive heartbeat detection) plus `AppDomain` unhandled-exception tray disposal and moving blocking work off the WinForms UI thread.
2. **Windows Bluetooth connection manager app (prevent laptop stealing phone earbuds):**
   - Goal: let the owner pick which paired Bluetooth devices are allowed to auto-connect on Windows startup or when Bluetooth turns on, and automatically keep/disconnect unselected devices (such as earbuds currently paired to the owner's phone) so laptop Bluetooth stops hijacking phone audio.
   - Note: confirm lightweight standalone tray app vs `arena-link-windows` integration and desired mode (per-device "Allow auto-connect / Block auto-connect" toggle + quick one-click "Connect now" override in tray menu) before building.

## Active note on branch divergence

- Remote branch `origin/arena/b4016c28-tests` contains later experimental Spatial HUD Method 4 work (mixed with raw logs and an unresolved `WorldSpaceSolidQuad.java:299` vertex crash). Reconcile selectively only when resuming Spatial HUD Method 4 work.
