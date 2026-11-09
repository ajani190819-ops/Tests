# Repository Current State (`STATE.md`)

**Active Working Branch:** the current `arena/<id>-tests` session branch — read it
from git (`git rev-parse --abbrev-ref HEAD`), never hardcode it. A hardcoded name
here went stale twice and sent `tools/sync_branch.sh` at the wrong branch.
**Last Updated:** 2026-10-30 (AudioSwitcher: Open-from-hub surfaces the window; apps on every render endpoint are listed, not just the default's)

## Turn protocol (do this literally)

1. **Live notes first** (before any other action): `.agent/INBOX.md` — the
   AgentNotes app / browser capture write here via `tools/notes_server.py`
   (start it at turn start if down: `python3 tools/notes_server.py 8765`).
   Re-read every 2-3 commits and before the final push; answer notes in the
   final message.
2. **Git re-align:** `bash tools/sync_branch.sh` (no argument — it now derives
   the branch from HEAD, installs the `.githooks/` guards, and bootstraps the
   remote branch on a session's first turn). Never force-push.
3. One parallel read batch: this file + `.agent/CODEMAP.md` + only the task's
   topic doc (`.agent/INDEX.md`).
4. One logical change per commit (own build+check), push immediately.
5. Update this file whenever project state changes (it survives resets). Keep it
   under 120 lines — `.agent/INDEX.md` enforces that budget.

## Active Component Status

| Component | Path | Version | Status |
| :--- | :--- | :--- | :--- |
| **Project Hub** | `project-hub/src/project_hub.c` | 2.8.39 | **GREEN** — suite folder is the install target for every app and the hub; "Update All" now refreshes every built-in (Agent Notes was the one skipped); stray copies mirrored in the same click; content-hash update checks; branch-aware mod card |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` | 2.8.7 | **GREEN** — rules-ghost device list; `HighestAvailable` logon task |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `Aura-Background.ps1` | 2.8.7 | **GREEN** — M4/M5 hold modes; dither keeps each lamp's phase across a blackout (no lockstep pulse on off/on); strict 7-bit ASCII, C# 5.0 |
| **DNS Pilot** | `dns-pilot/src/dns_pilot.c` | size-tracked | **GREEN** — parallel rank test; `TestsSuite-DnsPilot` ONLOGON task |
| **Agent Notes** | `agent-notes/src/agent_notes.c` | size-tracked | **GREEN** — POSTs to `tools/notes_server.py`; queues in `%LOCALAPPDATA%\AgentNotes\outbox\` while the agent is offline |
| **Audio Switcher / Clipboard Memory** | `audio-switcher/`, `clipboard-memory/` | size-tracked | **GREEN** — catalog-driven hub cards; Audio Switcher surfaces its window on Open-from-hub (named-event show) and lists apps on every render endpoint, not just the default |
| **Spatial HUD** | `spatial-hud-template-26.3/` | mod 1.0.5, updater 4 | **GREEN** — flicker + bottom cut-off fixed; pass-splitting HUD mods (Gnetum) complete partial frames via `fillSkippedRoots()` |
| **Wave Overhangs / Unlayered Infill** | `plugins/` | 0.0.51 / 0.4.10 | **GREEN** — versions in lockstep across code, `.bat`, JSON, changelogs |
| **App catalog** | `suite-apps.json` | 8 apps | **GREEN** — one JSON entry adds a hub card, no hub rebuild. Now contract-tested. |
| **CI** | `.github/workflows/{ci,build-spatial-hud}.yml` | — | **GREEN** — both live; `docs/ci/` mirrors them and a test fails on drift |

## Fast Commands

- Build all seven EXEs: `python3 tools/build_windows.py`
- All verification suites: `python3 tools/check_all.py`
- Code map regen: `python3 tools/agent_map.py`
- Icons + spin: `/home/user/.venv/bin/python3 tools/generate_suite_icons.py`

## Critical gotchas (invariants that broke once already)

- **Hub string pool.** `RebuildExtraCards` resets `g_strPoolUsed = 0`, which
  invalidates EVERY pool-backed pointer on the built-in cards (`title`, `blurb`,
  `exeRepo`, `exeName`, `shortcutName`, `launchArgs`, `btns[].label`,
  `btns[].verb`). `RestoreBuiltInCardText()` must run before that reset and must
  cover all 8 slots. Any card missing or malformed in `suite-apps.json` keeps
  stale pointers and draws recycled memory — that is how Audio Switcher showed
  Bluetooth Guard's blurb. `tests/test_windows_apps.py` pins both halves.
- **Install locations.** `%LOCALAPPDATA%\Programs\TestsSuite\<App>` is the ONE
  install target: `InitAllPaths` prefers it over a copy sitting next to the hub
  (a hub launched from Downloads used to make Downloads the target, so the suite
  folder kept a build from days ago while every card said "Up to Date" — it was
  measuring the stray copy it had just written). Every other existing copy is
  registered with `AddMirrorDir` and receives the same bytes through
  `MirrorFileEverywhere` / `InstallRepoFileEverywhere`; mirrors are never created
  where the user had no copy. A card's `appDir` must stay the suite path so
  Install, Open and the badge all act on one folder, and the hub's self-update
  writes `g_suiteHubExe` plus repoints the Desktop shortcut.
  `SuiteFolderInstallTests` pins all of it.
- **`suite-apps.json` needs an `id` on every entry.** `AddCatalogCard` returns
  before reading ANY other field if `id` is absent.
- **`spatial-hud-latest` must hold exactly ONE jar.** The hub reads the FIRST
  `browser_download_url`, and GitHub returns assets sorted by name, so a
  leftover `1.0.0` outranks `1.0.4` forever. The workflow now deletes stale
  assets itself; `--clobber` alone does not.
- **`Update-SpatialHUD.bat` installs the jar under the name GitHub published.**
  A hardcoded `JAR_NAME=spatial-hud-1.0.0.jar` put 1.0.4 bytes on disk under the
  1.0.0 file name, which tripped Modrinth's pack fingerprint check ("cannot be
  backed up safely"). Its `UPDATER_VERSION` is a bare INTEGER (`LEQ` compare) —
  unlike `Orca-Plugins.bat`, which uses `[version]` x.y.z.
- **Modrinth App** refuses bulk updates when a tracked mod file's hash no longer
  matches its fingerprint. Run hub Update Mod FIRST (it deletes `spatial-hud*.jar`
  and installs under a new name, making the tracked file Missing, which the
  backup accepts). Modified files are not tolerated; Missing ones are.

## CI / sandbox gotchas

- GitHub rejects `.github/workflows/*` writes from a token without the special
  `workflows` permission, and `gh workflow run` returns 403. Both workflows are
  now committed live; if a future workflow push is rejected, `docs/ci/` holds the
  identical file to paste by hand (see `docs/spatial-hud/CI-SETUP.md`).
- `/home/user/.venv` does NOT survive sandbox rebuilds. Recreate with:
  `python3 -m venv /home/user/.venv && /home/user/.venv/bin/python -m pip install -r requirements-dev.txt ziglang Pillow`
- The pre-commit guard refuses commits while `origin/<branch>` is unknown. On a
  session's first turn run `bash tools/sync_branch.sh` — it publishes the branch.

## Open Threads / Next Actions

1. **AudioSwitcher panel on the owner's screen** (as of the reachability fix):
   with the usual outputs plus several apps playing, every row and the footer
   are reachable - by fitting or by scrolling - and the app list names the apps
   that are actually playing. Check at 150% scaling, after dragging the window
   to a monitor with a different scale, scrolling with the wheel while a slider
   has focus, and holding a slider thumb while the 2-second poll fires. Clicking
   Open in the hub (app already in the tray, and elevated) should surface and
   focus the window. An app on ANY output should appear, tagged
   `app.exe - on <device>` when it is not on the default.
2. **Needs Windows / in-game validation** (cannot be tested in this sandbox):
   after hub 2.8.38, one Update click writes the suite folder copy AND every
   stray copy (owner's case: Audio Switcher stuck 9 days old in
   `Programs\TestsSuite\AudioSwitcher` while Downloads got each build);
   Audio Switcher card shows its own blurb + working buttons after a hub refresh;
   `Update-SpatialHUD.bat` installs `spatial-hud-1.0.4.jar` under its real name
   and self-updates from `main`; notes quick-capture page still posts.
3. If the owner still sees a UAC prompt on manual double-click launches, offer
   routing shortcuts through a silent task launcher (`schtasks /Run`).
4. Method 3 (`VirtualHudPlane.textureU/V`) still divides UVs by
   `guiWidth/guiHeight` — the mapping bug fixed in Method 4 (1.0.4) is unfixed
   there. Fix it if Method 3 ever reports the same symptom.

- **AudioSwitcher: Open-from-hub shows the window; apps on every output are
  listed** (owner: "when I click to open it from the hub it should actually pull
  up the app... I know it's open in the tray but I wanna actually be able to see
  it" and "I can't see what apps are using any audio even when I'm running games";
  audio-switcher/src/audio_switcher.c, `AudioSwitcher.exe` rebuilt 184,832 B).
  Two independent fixes, modelled on how professional tools do it.
  - **Open-from-hub never surfaced the window.** The hub launches the exe with no
    args (non-silent), but `wWinMain` always did `ShowWindow(hwnd, SW_HIDE)` and
    never created `SHOW_EVENT_NAME`, so a second launch's `OpenEventW`+`SetEvent`
    hit a non-existent event and exited silently while the first instance stayed
    hidden in the tray. Now an explicit (non-`--silent`) launch shows+foregrounds
    the window; the first instance creates the named event and a `ShowWatcherThread`
    waits on it and marshals `WM_ASW_SHOW` onto the UI thread (a named event, not a
    cross-process PostMessage, because the tray copy is elevated and UIPI would
    drop a message from a lower-integrity launcher); the WndProc restores/shows/
    foregrounds on `WM_ASW_SHOW`; the second instance signals the event with a
    `FindWindowW` fallback. `--silent` (the logon task) still starts hidden.
  - **Apps/games on a non-default output were invisible.** `sessions_refresh`
    swept the other endpoints only `if (g_session_count == 0)`, so one background
    app on the default hid every game rendering to another output. An app renders
    to whichever endpoint it opened its stream on and Windows does not move it when
    the default changes. Now every active render endpoint is enumerated (the
    Windows-mixer / EarTrumpet model): default first (unlabelled), then each other
    endpoint labelled `app - on <device>`; each session lives on exactly one
    endpoint so nothing is double-counted. Manager array widened 8->32 to match the
    endpoint array. `test_every_active_output_is_enumerated_not_just_the_default`
    and `test_second_launch_shows_the_existing_window` pin both (negative controls:
    unconditional SW_HIDE / re-gate the sweep both caught). No hub change - the hub
    already launched it correctly.

- **Hub 2.8.39: "Update All" was skipping Agent Notes** + **keyboard dither
  phase fix** (owner: "fix the flickering of keyboard lighting and make the
  folder actually update" -> clarified: the backlight stutters/flashes during
  effects, and the suite folders for all the apps the hub updates must really
  refresh). Two independent fixes, one per component.
  - **Update All omitted Agent Notes.** `TASK_UPDATE_ALL` called the dedicated
    workers for Bluetooth Guard, Keyboard Lighting, Orca, Spatial HUD and DNS
    Pilot, then swept slots 6+ through `WorkerInstallGeneric` (Audio Switcher,
    Clipboard Memory, catalog cards). Slots 0-5 are the dedicated-worker apps,
    but Agent Notes (slot 5) was the one whose worker was never called, and the
    generic loop starts at 6 - so its suite folder kept the previous build on
    every one-click "Update All" while every other app refreshed. Added
    `WorkerInstallNotes(FALSE);` to the block (project-hub/src/project_hub.c).
    Every built-in now updates; `WorkerCheckRemote()` still runs last so the
    badges clear. Bumped `HUB_VERSION` + `HUB_VERSION_W` to 2.8.39 together.
    `test_update_all_refreshes_every_builtin_app_suite_folder` pins all six
    dedicated calls + the generic sweep + the recheck; 4 negative controls
    (drop Notes / drop DNS / start the loop at 8 / drop the recheck) all caught.
  - **Keyboard backlight lockstep pulse on off/on cycles.** The four keyboard
    lamps almost always carry the same colour. `SeedDither()` gives each lamp and
    channel a different golden-ratio starting fraction so they round - and
    therefore step - on different frames; across the group the steps cancel
    instead of adding into a whole-keyboard pulse. But the blackout path in
    `SetZoneG` (`br <= 0.0005`) reset the carried error to `0.0` for every lamp
    at once, re-syncing them, so after any off/on cycle (master slider to 0 and
    back up, or an effect's black phase ending) all four restarted in phase and
    the shimmer came back. Split the per-lamp seed into `SeedLamp(i)` (called by
    `SeedDither()` and by `Reopen()` as before) and made the blackout path call
    `SeedLamp(i)` instead of zeroing: it still clears the accumulated error, but
    keeps each lamp's phase (keyboard-lighting/Aura-Background.ps1).
    `test_keyboard_lighting_dither_keeps_per_lamp_phase_across_blackout` pins it;
    3 negative controls (zero on blackout / remove SeedLamp / stop reseeding on
    Reopen) all caught. Strict 7-bit ASCII kept; `cslint.py` (both engine blocks
    + ui_controls.cs.txt) and `paramcheck.py` clean. The .ps1 is a script the hub
    downloads, so no KeyboardLighting.exe rebuild was needed - only
    ProjectHub.exe changed.

- **AudioSwitcher panel: nothing cut off, and the app list is real** (owner:
  "the bottom part of it is cut off and I can't see what apps are actually
  available and it'll say that no apps are available when there are apps that
  are broadcasting audio"; audio-switcher/src/audio_switcher.c, committed
  `AudioSwitcher.exe` rebuilt 184,320 B). Both halves were layout, plus one real
  gap in enumeration.
  - **The app list was squeezed to nothing**: `ui_build_controls` gave OUTPUT
    DEVICES a fixed budget and reserved only `hdr + gap + 3 rows` for APP
    VOLUME, so with several output devices `ss_max` collapsed to 0-1. Zero
    session controls were created and WM_PAINT drew "No apps are playing audio
    right now" over a list `sessions_refresh()` had already enumerated. The
    budget is now `ui_layout_budget()` with `UI_MIN_APP_ROWS 5` guaranteed to
    the app section before the device rows get anything left over.
  - **Nothing scrolled and the window could be taller than the screen**: no
    `WS_VSCROLL`, no `WM_VSCROLL`, no `SetScrollInfo` anywhere, and the height
    was clamped to a hardcoded `S(940)` with no work-area check, so the bottom
    sat under the taskbar. Now one logical-row column with `g_scroll_offset` /
    `g_scroll_range`: `WS_VSCROLL` + `WM_VSCROLL` (line/page/thumb/top/bottom) +
    `WM_MOUSEWHEEL` (3 rows per notch, also forwarded from a focused slider) +
    `WM_DPICHANGED`. Scrolling re-aims the existing controls at new logical rows
    instead of destroying them, so focus and an in-flight drag survive - and a
    held thumb blocks scrolling outright (`GetCapture()`). Scrollbar width
    (`SM_CXVSCROLL`) is claimed only when there is something to scroll.
    `ui_rebuild()` sizes to the budget and clamps into
    `MonitorFromWindow`/`GetMonitorInfoW().rcWork`, moving the window up when it
    does not fit, with an `SPI_GETWORKAREA` + `SM_CY*` fallback. The footer is
    pinned to the client bottom, never scrolled.
  - **`PANEL_X`/`PANEL_Y` were used unscaled** while every size next to them went
    through `S()`: at 150% the panel sat too high and rows overlapped the header
    band. Every panel coordinate (Refresh button included, plus all WM_PAINT
    rects) is now `S()`-scaled.
  - **Apps on a non-default output were invisible**: `sessions_refresh()` asked
    only the default render endpoint for its session enumerator, so a game or
    browser pinned to another device had a session nowhere we looked - audio
    playing, panel claiming nothing was. Collection moved into
    `sessions_collect_from_device()`; when the default endpoint yields nothing we
    sweep the other active endpoints and label those rows `app.exe - on <device>`
    (`on_other_device`), because they are inaudible until the output is switched.
    Each device's `IAudioSessionManager2` is retained in `g_session_mgrs[8]`: the
    `ISimpleAudioVolume` pointers the rows hold belong to it, so releasing it
    would leave dead sliders. All of them are released on the next poll and at
    shutdown (`sessions_cleanup`).
  - **Row identity no longer trusts control ids.** Scrolling re-aims slots, so a
    slider notification is resolved by finding the slot whose `hwnd_vol` matches.
    Ids are per-slot (`1000+i`, `2000+i`, `3000+i`, `4000+i`, `5000+i`) instead
    of `base + logical`, which walked out of its handler's range once the list
    got long enough to scroll (`4000 + 200` is not `< 4200`) and silently dropped
    the volume change. All five WM_COMMAND branches re-derive the row index from
    the slot and bounds-check it. Spare slots paint no card.
  - The empty-panel text still separates the two cases: an enumeration failure
    quotes its HRESULT, a genuinely quiet system says "No apps are playing audio
    on any output right now". `g_truncated_*` now counts only rows past the
    32/128 array caps and says "row(s) past the list limit".
  - 6 new tests in `AudioSwitcherReachabilityTests` + 3 in
    `AudioSwitcherSessionSweepTests`; the rebuild-gating test now follows the
    rebuild into `ui_rebuild()`. 37 tests pass. 17 negative controls (each
    mechanism reverted one at a time) all caught. `tools/check_all.py` green in
    both dependency states; all 7 EXEs rebuilt.

- **Project Hub 2.8.38: the suite folder is the install target** (owner: an
  Audio Switcher download never replaced the copy he actually ran;
  project-hub/src/project_hub.c, committed `ProjectHub.exe` rebuilt).
  `%LOCALAPPDATA%\Programs\TestsSuite\<App>\` is now where every Update writes,
  Downloads is a convenience copy, `DownloadRepoBytes` fetches bytes and writes
  once through a temp file + `MoveFileExW(REPLACE_EXISTING)`, and
  `MirrorFileEverywhere` gives every other stray copy the identical bytes in the
  same click (exe dir, repo checkout, shortcut target, suite folder) - so there
  is nothing stale left to launch. Same for the hub itself, including repointing
  `Project Hub.lnk`. `ID_MENU_HUB_TOSUITE` (1013) re-copies a hub that was
  launched from somewhere else. Card badges and details resolve through
  `btgDir/kblDir/asDir/cmDir` (`AppendInstallFolder` + `ShortDirForUi`), and a
  missing suite copy reads as Not Installed instead of being papered over by a
  stray copy elsewhere.

## Last session (2026-10-30) — repository audit + fixes

Audited the whole repo and fixed what the audit found: stale `.agent/CODEMAP.md`
(the gate failed on a clean checkout), missing `id` on the Audio Switcher catalog
entry, the hub string-pool bug above, `Update-SpatialHUD.bat`'s frozen `SELF_REF`
and hardcoded jar name, CI that was not running at all (`ci.yml` never installed,
`build-spatial-hud.yml` drifted from its documented copy), `sync_branch.sh`
defaulting to a stale branch, non-executable `gradlew`, the unauthenticated
write path into the agent's own context in `notes_server.py`, `JsonGet`'s
escape handling and key-position matching, and assorted lint/doc drift. Added
contract tests for the catalog, the pool restore, the hub version pairing, and
CI drift.
