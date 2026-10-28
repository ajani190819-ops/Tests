# Repository Current State (`STATE.md`)

**Active Working Branch:** `arena/e895b94f-tests`
**Last Updated:** 2026-10-24
**PR:** #10 open — every commit+push updates it.

## Turn protocol (do this literally)

1. **Live notes first** (before any other action): `.agent/INBOX.md` - the
   AgentNotes app / browser capture write here via `tools/notes_server.py`
   (start it at turn start if down: `python3 tools/notes_server.py 8765`).
   Re-read every 2-3 commits and before the final push; answer notes in the
   final message. Transition fallback: Google Drive doc id
   `1XRzQ4yhq2slvH1E2r2t9c7uK-vGcI1EBAxQOmSZArN8` until retired.
2. **Git re-align:** `bash tools/sync_branch.sh` (safe: installs `.githooks/` commit guards, fetch + re-aligns the pointer; working files never touched; good unpushed commits left alone). The pre-commit guard refuses stale-base commits. Never force-push.
3. One parallel read batch: this file + `.agent/CODEMAP.md` + only the task's topic doc (`.agent/INDEX.md`).
4. One logical change per commit (own build+check), push immediately.
5. Update this file whenever project state changes (it survives resets).

## Active Component Status

| Component | Path | Version | Status |
| :--- | :--- | :--- | :--- |
| **Project Hub** | `project-hub/src/project_hub.c` + `ProjectHub.exe` | 2.8.21 | **GREEN** — update nag only for genuinely newer GitHub builds (numeric `VerNewer`), buttons amber only while update ready, reopens after self-update (mutex handover + retry), quiet 60s ETag checks, cached-frame spin paint, Arena Link removed (5 cards), requireAdministrator |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` + `BluetoothGuard.exe` | 2.8.7 | **GREEN** — rules-ghost device list; FastBoot task now `HighestAvailable` (elevated logon), requireAdministrator |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `KeyboardLighting.exe` + `Aura-Background.ps1` | 2.8.7 | **GREEN** — M4/M5 hold modes (`-HoldMode latch\|burst\|tap`, latch default); logon task `/RL HIGHEST`; requireAdministrator |
| **DNS Pilot** | `dns-pilot/src/dns_pilot.c` + `DnsPilot.exe` | size-tracked | **GREEN** — parallel rank test (seconds); benchmarks on EVERY start incl. tray; autostart = `TestsSuite-DnsPilot` ONLOGON task `/RL HIGHEST` (Run key migrated away); requireAdministrator |
| **Agent Notes** | `agent-notes/src/agent_notes.c` + `AgentNotes.exe` | size-tracked | **GREEN** — direct quick-notes channel (no Google middleman): POSTs to `tools/notes_server.py` (live preview port 8765), queues in `%LOCALAPPDATA%\AgentNotes\outbox\` while the agent is offline, auto-delivers on reconnect; hub card #6 with Install; requireAdministrator |
| **App catalog** | `suite-apps.json` + Hub 2.8.23+ | 2026-10-23 | **GREEN** — one JSON entry adds a hub card (title/blurb/buttons/icon + Install button); the hub fetches the catalog at refresh, so new apps need **no hub update**; icon = repo path, cached with letter-tile fallback; built-in card text/buttons also catalog-driven (2.8.24) |
| **Agent tooling** | `AGENTS.md`, `.agent/CODEMAP.md`, `tools/` | 2026-10-23 | **GREEN** — `agent_map.py` code map (`--check` in suite), one-shot build (~2s cached) / full suite (~4s cached) |

## Fast Commands

- Build all four EXEs: `python3 tools/build_windows.py`
- All verification suites: `python3 tools/check_all.py`
- Code map regen: `python3 tools/agent_map.py`
- Icons + spin: `/home/user/.venv/bin/python3 tools/generate_suite_icons.py` (only when brand files change)

## Open Threads / Next Actions

1. **User validation round (2026-10-23 evening):** update buttons stop glowing when up to date; hub reopens after self-update (2.8.21); false "vX available" nag gone; DNS Pilot tests from tray startup; all apps elevated with no UAC at login.
2. If the owner still sees a UAC prompt on manual double-click launches (UAC-on machines), offer routing shortcuts through a silent task launcher (`schtasks /Run`).

## CI / sandbox gotchas (2026-10-24)

- The Arena GitHub integration CANNOT push changes to `.github/workflows/*`
  (rejected: no `workflows` permission) and CANNOT `gh workflow run`
  (HTTP 403). Mod fixes that need a new Spatial HUD jar require the user
  to click **Actions -> Build Spatial HUD -> Run workflow** with branch
  `arena/e895b94f-tests` selected - or a one-time web edit of
  `.github/workflows/build-spatial-hud.yml` adding that branch to the
  `on.push.branches` list. Do not retry programmatic dispatch.
- `/home/user/.venv` does NOT survive sandbox rebuilds (excluded from
  persistence): recreate with `python3 -m venv /home/user/.venv &&`
  `/home/user/.venv/bin/python -m pip install -r requirements-dev.txt ziglang`.

## Spatial HUD 1.0.1 checkpoint (2026-10-24 late)

User round-2 report (screenshot `/tmp/arena-coding-uploads/image-1.png`,
747x190 crop of the panel picture, ~3.36 img px per GUI px, hotbar
182x22 GUI at x=55..666 rows 98..171): (1) selection overlay cut off at
the bottom + black hotbar outline broken at the selected slot; (2) XP/
hunger/health bars flicker.

- ROOT CAUSE (1), pixel-proven: `WorldSpaceSolidQuad.submitCapturedRect`
  fed RAW panel fractions to the texture lerp (`lerp(uLeft,uRight,a)` with
  a = 0.03..0.97). The picture is inset at 3% and can hang below the panel
  (negative picture offset), so the band was cut on all four edges and
  squeezed ~3-6%; the selection sprite's bottom frame landed ~2 GUI above
  the picture's bottom edge and the hotbar outline rows showed the sprite's
  translucent interior (lum 60) instead of the frame. Fix `cc4826e`:
  normalize UVs over the picture sub-rect (u spans s0..s1, v spans t0..t1).
- Fix `df1dde7` (1.0.1): one-shot `reinforceStripWrappers()` on first
  END_CLIENT_TICK re-applies `replaceElement` wrappers so companion
  replacements (bars drawn at vanilla screen positions = the flicker /
  snap-back) land inside the private capture. Nested capture is safe.
- Build green (run 40145988728), `spatial-hud-latest` now ONLY
  `spatial-hud-1.0.1.jar` (1,585,056 B) - deleted stale 1.0.0 asset with
  `gh release delete-asset` because the hub reads the FIRST
  `browser_download_url` of the release (`WorkerInstallSpatialHud`).
  **After every future jar publish, delete old assets from
  `spatial-hud-latest`** (workflow `--clobber` does not remove them and
  the hub would pick the oldest).
- Tests 10/10 OK. Awaiting user test round 3.

## Modrinth App fingerprint gotcha (2026-10-24 night)

Modrinth App 0.22.0 bulk updates refuse to run when a pack-tracked mod file
hash no longer matches its recorded fingerprint:
"restore or repair mods/spatial-hud-1.0.0.jar ... cannot be backed up safely"
(`modrinth/code` packages/app-lib/src/install/recovery.rs, prepare_instance_update_backup;
Missing files are tolerated, Modified ones are not). Cause: hub Update Mod
replaced the bytes of the tracked `spatial-hud-1.0.0.jar` in place (the old
release asset name). Remedy: run hub Update Mod first (it deletes
`spatial-hud*.jar` and installs the new version under a NEW name) - the
tracked file becomes Missing, which the backup accepts. After any Modrinth
pack update, check the mods folder for a restored `spatial-hud-1.0.0.jar`
and delete it (two jars break Fabric startup). Future jar bumps use new
asset names so the conflict does not recur.

## Round-4 checkpoint (2026-10-25) — Spatial HUD 1.0.4 (`132c75f`, built run 40156759689)
- **User confirmed: flicker is FIXED (1.0.3, `5c5c335`).** Remaining report: "it changes transparency and the bottom is still cutoff" — ONE root cause, finally identified:
- The GUI projection draws into `guiWidth*guiScale x guiHeight*guiScale` physical pixels; the capture texture is the WHOLE window (which can be a few pixels bigger when the window doesn't divide evenly by the GUI scale). All UVs divided by the GUI size, so every sample drifted toward the texture's transparent bottom edge: the hotbar's bottom rows faded ("still cut") and the bottom margin strip sampled that transparency ("changes transparency"). The earlier band fixes (UV normalize `cc4826e`, half-texel `6b6f6b6`, margin strip `de13a5c`) were all real bugs but all pre-dated this mapping error.
- **1.0.4 fix:** `purplePanelState` maps the band through the real span in GUI pixels (`window.getWidth()/getGuiScale()`, `getHeight()/getGuiScale()`) and keeps the half-GUI-pixel centre samples. Stale 1.0.3 asset deleted — release = `spatial-hud-1.0.4.jar` only.
- Method 3 (`VirtualHudPlane.textureU/V`) has the same `guiWidth/guiHeight` UV denominators — not fixed (user is on Method 4; fix there first if Method 3 ever reports the same).
- If anything remains after 1.0.4: ask for `latest.log` (diagnostics list every root+layer; `getGuiScale()` compile result is in the build log if this ever re-breaks).

## Icon checkpoint (2026-10-25) — Agent Notes own icon (`311ec9e`, pushed)
- Agent Notes now has its own icon: stacked amber notes on the suite's dark slate medallion, tri-spoke core faintly behind it (`docs/brand/icon-agent-notes.png` + multi-frame `agent-notes/agent-notes.ico`), built by `tools/generate_suite_icons.py` — every size rendered at its own resolution.
- Wired everywhere: `suite-apps.json` card icon → `agent-notes/agent-notes.ico`; `project-hub/src/hub.rc` RES_ICON_NOTES → `../../agent-notes/agent-notes.ico`; `agent-notes/src/notes.rc` → `../agent-notes.ico` (AgentNotes.exe carries its own resource); `tests/test_windows_apps.py` icon test covers `agent-notes.ico`.
- Hub 2.8.32: `LoadCardIcon()` caches `icons\<id>-<fnv1a(catalog path)>.ico` so changing an app's icon fetches the new art once instead of serving the old cached .ico forever; both icon blocks replaced with the shared helper. `ProjectHub.exe` rebuilt (597 KB).
- Build green (`python3 tools/build_windows.py`: all 5 suite exes OK), tests 10/10, pushed to origin/arena/e895b94f-tests.

## State summary (this turn)
1. Spatial HUD 1.0.4 (`d9c7088`, pushed, release asset spatial-hud-1.0.4.jar only — stale 1.0.3 deleted) fixes the bottom cut-off + transparency change via span-based UV mapping in `purplePanelState`. Flicker was already fixed (user confirmed 1.0.3).
2. Agent Notes icon shipped as 2.8.32 (`311ec9e`, pushed).

## Startup/tray checkpoint (2026-10-25) — all three autostart apps verified/fixed

**Keyboard Lighting** (already correct, no change):
- Setup.ps1 creates scheduled task `KeyboardLighting`, `-Silent`, `RunLevel Highest`, `-AtLogOn` trigger
- Tray.ps1 runs in tray via NotifyIcon, single instance with bring-forward
- Manifest: requireAdministrator

**Bluetooth Guard** (`65f7f25`, pushed):
- Setup.ps1 was using HKCU Run key (not elevated, would trip UAC at boot, and passed wrong flag `-Silent` vs source's `--silent`)
- Replaced with scheduled task `BluetoothGuard`, `--silent`, `RunLevel Highest`, `-AtLogOn`
- Also removes old Run value on migration
- Source already self-registers via SetAutostart(TRUE) in WorkerThread (schtasks, RunLevel HighestAvailable, --silent)
- Tray: Shell_NotifyIconW NIM_ADD, single instance via mutex + SHOW_EVENT

**DNS Pilot** (`b2709a3`, pushed):
- Had NO Setup.bat/Setup.ps1 at all
- Added Setup.bat (elevation + download Setup.ps1 from GitHub) + Setup.ps1 (shortcuts, scheduled task `TestsSuite-DnsPilot` with `-Silent` `RunLevel Highest`, launch + verify)
- Source already had EnsureAutostart() (schtasks `/RL HIGHEST`), tray, single instance, `-Silent`
- Manifest: requireAdministrator

**Hub**: does NOT autostart (per constraints). **Agent Notes**: not in autostart group per user request ("Lighting, Bluetooth, and Pilot").

Tests: 10/10 pass. Both commits pushed to origin/arena/e895b94f-tests.
