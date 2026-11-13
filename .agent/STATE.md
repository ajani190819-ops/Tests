# Repository Current State (`STATE.md`)

**Active Working Branch:** the current `arena/<id>-tests` session branch — read it
from git (`git rev-parse --abbrev-ref HEAD`), never hardcode it. A hardcoded name
here went stale twice and sent `tools/sync_branch.sh` at the wrong branch.
**Last Updated:** 2026-11-12 (Desktop shortcuts stopped accumulating: hub workers and app Setups now only repoint pinned ones, never create. Sweep-while-held now glides at 1% per 30ms instead of 5% per 150ms, with tap and glide split into separate controls, and the brightness popup matches the Windows theme and flyout shape (round thumb, no percentage). M4/M5 hold behaviour is now reachable from the tray: all three engine modes - sweep-while-held, press-to-glide/press-to-stop, 20%-per-press - plus a read-only line reporting whether the keys are recognised at all. The last two glide inside the engine and need no key recognition, which is the likely fix for the owner's hardware if it sends no release report for those keys. `HoldPump` also gained a 10s ceiling sized against the engine's own 3s full sweep, because the pre-existing silence timeout switches itself off permanently once learning succeeds and a missed release would otherwise stream forever. CORRECTION on M4/M5: the owner's keys are already Custom actions running `KeyboardLighting.exe -BrightUp`/`-BrightDown`, so nothing in G-Helper should change - just update to 2.8.8. The earlier advice to rebind them to `Ctrl+Alt+Up`/`Down` was wrong and is retracted in the README, because the hold-to-sweep learns from the tokens that route never writes, and with codes already learned it would double-step. M4/M5 brightness no longer costs a UAC prompt per keypress. `KeyboardLighting.exe` ships `asInvoker` — the only suite app to — because G-Helper's Custom action launches it per press merely to write a token in `%LOCALAPPDATA%`, and `requireAdministrator` is enforced by the OS before our code runs. It still gets an admin token from the `/RL HIGHEST` logon task, an elevated ProjectHub, or its own `runas` self-elevation. Owner should bind M4/M5 to `Ctrl+Alt+Up`/`Down` in G-Helper's key remap instead, which launches nothing. The exclusive HID lock is now RETRIED instead of decided once at startup, `edca7f5`. The owner's log ran fourteen minutes at `lock SHARED, fallbacks 1, reopens 0` with writes at 3-6 ms against 2166 us uncontended, a 34334 us worst write and ~5% late frames against the ~1% scheduler jitter accounts for: the exclusive open was refused once and never revisited, so two writers shared the keyboard all session. `TryExclusiveUpgrade()` gives up the shared handle and asks again on a 3s->300s backoff, and the periodic line now reports `lock retries N` plus the names of running vendor lighting processes. This is why fixes 2 and 5 could not finish the job — a second writer is not a timing problem. Also noted: that log says `dither on`, meaning the panel's Smooth colour toggle is OFF on a full-range 255/255/255 panel where it can only add shimmer; left alone deliberately, owner can A/B it. Residual keyboard stepping fixed, `d4b2316`: animation POSITION now advances by a fixed nominal step (1/Fps) instead of measured frame time, so ordinary scheduler jitter — about one missed deadline a second at 60fps, per the loop's own notes — can no longer make the gradient stride unevenly. Physical decays and the palette crossfade keep the real dt so they stay frame-rate independent; `KBL_FIXED_STEP=0` reverts. Owner had reported the first three fixes made it "smoother but i can still see it if look". The remaining limit is hardware: a 4-zone deck has almost no spatial resolution, so some stepping is inherent. G-Helper containment watchdog added, `963f7a5`: the engine re-checks the six config keys Setup wrote once a minute and, on drift, rewrites them, restarts G-Helper and rewrites again - bounded to one restart per 10 minutes with a stand-down after 5 failed repairs, `KBL_GHELPER_WATCHDOG=0` to disable. Open decision: whether to extend the exclusive HID lock to the ASUS vendor collection, held until the owner's log settles whether G-Helper shares our LampArray collection. Keyboard Lighting flicker CAUSE CONFIRMED from the owner's engine.log: a stale pre-update engine was still running and holding the HID endpoint shared, so the new engine's exclusive open was refused (`lock SHARED, fallbacks 1`, two different owner pids 13172/31112, one still on the old log format). Two 60fps writers on the same LEDs is enough on its own to make the four deck zones step while the twelve bar lamps average it out. `range R255/G255/B255` rules out coarse colour quantisation. Fixed by `3e0048e` — the engine now reads `engine.pid`, stands the previous instance down via `stop.flag` and clears it before its own loop can see it; a refused lock names the likely vendor holders by process name (no WMI, which is banned here). Owner to run Restart.bat and confirm `lock exclusive, fallbacks 0`. Earlier 2026-10-30: Keyboard Lighting anti-flicker pass, engine only: the HID endpoint is now opened EXCLUSIVELY with a shared fallback (dwShareMode 0, so no other program can interleave frames; KBL_EXCLUSIVE_LOCK=0 opts out), the two batched reports of each frame are paced 1000us apart so they cannot share one 1ms USB frame (KBL_REPORT_PACE_US tunes it), and the timing line now logs lock state / fallback count / dither decision / the device's own colour range, with a shared handle escalated to WARN. Diagnosis came from the owner's detail that the four keyboard zones step while the twelve light-bar lamps stay smooth, which rules out frame tearing and points at a second writer or a coarse device colour range. NEEDS ON-DEVICE CONFIRMATION via logs/engine.log. Agent docs restructured: AGENTS.md slimmed to the always-read core - turn protocol, communication, hard rules - with the detailed protocols split verbatim into .agent/rules/{execution,repo-map,release}.md, mirroring the app modularization; the STATE.md 120-line budget is retired. Bluetooth Guard: phantom never-connected devices filtered out (HasConnectedBefore now requires a service-GUID subkey) + a per-device Forget button doing a true unpair (BluetoothRemoveDevice, not a profile disable). Source modularization COMPLETE: all six app C files are a thin main + parts/ section files (one TU; split EXEs byte-identical; tests read via read_c_inlined(); CODEMAP indexes every part). Hub buttons Update-first with a dynamic Install/Update first button; Clipboard Memory removed.)

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
   scannable — no hard length cap (the old 120-line budget is retired).

## Active Component Status

| Component | Path | Version | Status |
| :--- | :--- | :--- | :--- |
| **Project Hub** | `project-hub/src/project_hub.c` + `parts/` | 2.8.43 | **GREEN** — Desktop shortcuts are no longer created by installs or updates (`<HASH>`): every worker and all four app Setups used to re-create the per-app .lnk on every run, which is how they accumulated; creation now happens only via the explicit emblem-menu pin, while an already-pinned shortcut is still repointed after an update so it never dangles at a replaced copy (single guarded helper `RepointDesktopShortcutIfPresent`, install paths asserted to contain zero direct `CreateShortcutLink` calls). Every card leads with a dynamic Update/Install button (Install when the app is missing, Update when a newer build is known, hidden when current), then Open, Folder, and the card extra (Stop/Unfreeze); Orca + Spatial HUD match the other app cards; Orca has a real Remove and a persisted Disable that gates its installer; suite folder is the install target for every app and the hub; "Update All" refreshes every built-in; content-hash update checks; branch-aware mod card; Clipboard Memory removed (7 built-in cards). Modularized: thin `project_hub.c` `#include`s 14 `parts/` section files in order (one translation unit; `RefreshProjectStatuses` is in `parts/refresh_statuses.c`, the card button init in `parts/card_defs_verbs.c`) |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` + `parts/` | 2.8.8 | **GREEN** — rules-ghost device list; `HighestAvailable` logon task; phantom never-connected devices filtered out (`HasConnectedBefore` requires a service-GUID subkey); per-device **Forget** button does a true unpair (`BluetoothRemoveDevice`, op==3 on the worker) without disabling profiles. Modularized: thin `bluetooth_guard.c` `#include`s 9 `parts/` files in order (one TU). `HasConnectedBefore`/`IsServiceGuidSubkey` in `parts/device_history.c`; `DoForgetDevice` in `parts/bt_actions.c`; Forget button + hit in `parts/window_entry.c`; `DrawPillToggle` in `parts/ui_controls.c` |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `parts/` + `Aura-Background.ps1` | 2.8.10 | **GREEN, needs on-device check** — engine now opens the HID endpoint EXCLUSIVELY (dwShareMode 0, shared fallback, `KBL_EXCLUSIVE_LOCK=0` opts out) and paces the two reports of a frame 1000us apart (`KBL_REPORT_PACE_US`); timing line logs lock/fallbacks/dither/device colour range, shared lock logs WARN. M4/M5 hold via raw input (no global key hook, so PrintScreen works); dither keeps each lamp's phase across a blackout; strict 7-bit ASCII, C# 5.0. Modularized: thin `keyboard_lighting.c` `#include`s 9 `parts/` section files in order (one translation unit; the ASCII contract is enforced across main + parts) |
| **DNS Pilot** | `dns-pilot/src/dns_pilot.c` + `parts/` | size-tracked | **GREEN** — parallel rank test; `TestsSuite-DnsPilot` ONLOGON task. Modularized: thin `dns_pilot.c` `#include`s 11 `parts/` section files in order (one translation unit; edit the part files) |
| **Agent Notes** | `agent-notes/src/agent_notes.c` + `parts/` | size-tracked | **GREEN** — POSTs to `tools/notes_server.py`; queues in `%LOCALAPPDATA%\AgentNotes\outbox\` while offline. Modularized: thin `agent_notes.c` textually `#include`s `parts/{config_outbox,http_agent,emblem,ui,entry}.c` in order (one translation unit; edit the part files) |
| **Audio Switcher** | `audio-switcher/src/audio_switcher.c` + `parts/` | size-tracked | **GREEN** — catalog-driven hub card; surfaces+focuses its window on Open-from-hub (named-event show); lists apps on every render endpoint. Modularized: thin `audio_switcher.c` `#include`s 7 `parts/` section files in order (one translation unit; edit the part files) |
| **Spatial HUD** | `spatial-hud-template-26.3/` | mod 1.0.5, updater 4 | **GREEN** — flicker + bottom cut-off fixed; pass-splitting HUD mods (Gnetum) complete partial frames via `fillSkippedRoots()` |
| **Wave Overhangs / Unlayered Infill** | `plugins/` | 0.0.51 / 0.4.10 | **GREEN** — versions in lockstep across code, `.bat`, JSON, changelogs |
| **App catalog** | `suite-apps.json` | 7 apps | **GREEN** — one JSON entry adds a hub card, no hub rebuild. Now contract-tested. |
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

1. **Keyboard flicker — read the engine log before touching anything else.**
   Owner report: individual updates are visible; the four KEYBOARD zones step
   while the twelve LIGHT-BAR lamps stay smooth, on a built-in pattern. That
   split ruled out frame tearing across the two batched reports and pointed at a
   second writer, since vendor lighting software contests the deck and usually
   leaves the chassis bar alone. Shipped `7017694` (exclusive HID open + shared
   fallback), `398f8f0` (1000us pacing between the two reports of a frame, so
   they cannot share one 1ms USB frame), `8255949` (log lock state, fallback
   count, dither decision and the device's own colour range; README table says
   what each reading means). **To confirm:** open `logs\engine.log` via the tray
   menu and check the startup + timing lines. `lock SHARED` means Armoury Crate
   or an ASUS lighting service still owns the keyboard. `range` below 255 means
   the deck has too few colour levels for any frame rate to smooth it, and the
   trade-off moves to the panel's Smooth colour toggle. Engine only, no C
   change, so no KeyboardLighting.exe rebuild was needed — Setup.bat re-fetches
   the engine.

   **Owner's engine.log settled it (2026-11-11).** Two runs 36s apart watching
   different owner pids — 13172 logging the OLD timing format, then 31112
   logging the new one with `lock SHARED, fallbacks 1`. A pre-update engine was
   still alive and holding the endpoint shared, which is exactly what refuses an
   exclusive open, and two loops pushing 60fps at the same LEDs is by itself
   enough to make the four deck zones step while the twelve bar lamps average it
   out. `range R255/G255/B255` RULES OUT coarse colour quantisation and
   confirms `dither off` was the right call. `worst 26463 us` against a 16667 us
   budget plus `late frames 1` fits two writers contending.

   Fixed by `3e0048e`: the engine now READS `engine.pid` before opening the
   device (it only ever wrote it), asks the previous instance to stop via
   `stop.flag`, waits up to 2.5s, then clears the flag — clearing matters
   because the main loop polls that same file and would otherwise stop itself
   immediately. One-shot diagnostics (`-Restore`, `-ZoneTest`, `-ChaseTest`)
   skip the handover so a zone test can never kill the owner's engine. A refused
   exclusive open now names likely holders by process name (ArmouryCrate,
   LightingService, GHelper, OpenRGB, SignalRGB) and distinguishes "previous
   engine stopped yet still shared" from "nothing known found — run
   Restart.bat". **Names only: `Win32_Process` is banned in this file** by
   `test_keyboard_lighting_reliability_contracts` because a process-table WMI
   query can hang for seconds after sleep/wake; my first version used
   `Get-CimInstance` and the suite caught it. Findings are queued in
   `$script:StartupWarns` and flushed at WARN once `Write-Log` exists (the log is
   set up AFTER the device is opened).

   **G-Helper containment watchdog added (`963f7a5`).** Setup wrote the six
   containment keys once at install and nothing re-checked them, so one click in
   G-Helper's keyboard panel (no Apply button - it writes immediately) or an
   update silently undid it. The engine now re-reads
   `%APPDATA%\GHelper\config.json` once a minute; on drift it rewrites the
   keys, restarts G-Helper, then rewrites them AGAIN because G-Helper saves its
   in-memory config back over the file on exit. Bounded: one restart per 10
   minutes, permanent stand-down after 5 repairs that did not stick,
   `KBL_GHELPER_WATCHDOG=0` disables it. README rewritten because it told the
   owner to set `skip_aura: 0` to get G-Helper's lighting back, which the
   watchdog would now revert within a minute.

   **Owner's next step:** run `Restart.bat` (force-stops every instance, starts
   one clean), then re-read `logs\engine.log`. Expect `lock exclusive,
   fallbacks 0`. If it still says SHARED, the log now names what is holding it.
   **Owner's next log (2026-11-12 01:09-01:22) reopens this: the lock never
   took.** Fourteen consecutive minutes at `lock SHARED, fallbacks 1, reopens 0`
   — `fallbacks` never moved, so the exclusive open was refused ONCE at `Open()`
   and nothing ever revisited it. Ownership of the keyboard was decided in the
   first millisecond of the session and then never again. Every number on the
   line is the fingerprint of a second writer rather than of our own cadence:
   `device write` 2923-5765 us/frame against 2166 us measured uncontended,
   `worst` up to 34334 us (two whole frame budgets), `late frames` 114-228 per
   60 s = ~5% of frames where `Loop()`'s own comment puts ordinary scheduler
   jitter at ~1%, and `failed writes 0` throughout — so the rival opens SHARED
   and we coexist, interleaving frames on the same LEDs. **Fix 5's even step
   could not have shown up through that**, and neither could the pacing: a
   second writer is a stepping source no host-side timing can remove. Whoever
   opens the device first wins, and vendor lighting software is already running
   at logon, so we lose that race by construction.

   Fixed by `edca7f5`: `TryExclusiveUpgrade()` trades a working shared handle in
   for another attempt at the exclusive one, from the render loop between frames,
   on a gap that doubles 3 s -> 300 s. Our own handle must close first (dwShareMode
   0 refuses to open while ANY handle exists, including ours) and that window is
   the whole risk, so `OpenHandle`'s shared fallback plus a second attempt after
   60 ms guarantee the call never ends without a handle unless the device has
   genuinely gone. The periodic line now carries `lock retries N` and, when
   shared, the names of any running vendor lighting processes via
   `Get-LightingRivals` — because a shared handle was only half a diagnosis and
   the answer used to live in startup output that has scrolled away. Test
   `test_keyboard_lighting_retries_for_exclusive_ownership_of_a_shared_handle`,
   57 tests, 9 mutations all detected. **Expected in the next log:** `lock
   retries` climbing and then the line flipping to `lock exclusive`, with
   `device write` back near 2200 us and `late frames` near zero. If `retries`
   climbs forever the holder is permanent and must be closed — the line will now
   say which process it is.

   **Do not miss the second change in that log:** it says `dither on` where the
   earlier ones said `dither off`, so the panel's **Smooth colour** toggle is now
   OFF, which maps to `DitherMode = 1` (force on). Per the README that trade-off
   is for a `range` below 255; this panel reports R255/G255/B255, so forcing
   dither on can only add shimmer and no depth. **Left alone deliberately** — one
   behavioural change at a time so the next log stays unambiguous — but tell the
   owner that flipping Smooth colour back ON is a free A/B test they can run
   themselves. If a future log still shows stepping with `lock exclusive`, that
   toggle is the next thing to change, and `UseDither`'s `m == 1` branch should
   then be made to respect the device range.

2. **Residual keyboard stepping after the first three fixes (`d4b2316`).** Owner:
   "its smoother but i can still see it if look". Cause was `gr.Phase += dt *
   gr.Speed` using MEASURED frame time for animation position. dt serves two
   things that want opposite properties: a physical decay needs real wall-clock
   time (that is what makes fire/sparkle frame-rate independent), a position only
   needs each frame equidistant from the last. Measured dt gives 16.2/17.1/15.8ms
   of ordinary jitter, and `Loop()` resyncs `next` after a missed deadline so the
   following dt is short — the loop's own notes put misses at ~1% of frames, i.e.
   about one hitch a second at 60fps. Four large deck zones have nothing between
   them to average it out; twelve small bar lamps do, which is again why the bar
   looked fine and the keyboard did not. Now `gr.Phase += (FixedStep ? NominalDt :
   dt) * gr.Speed` with `NominalDt = 1/Fps` — derived from Fps so the adaptive
   backoff preserves real-time speed — while `RenderGroup` and `gr.XfadeLeft` keep
   the real dt. The outer `phase` driving `Now`/overlay windows also keeps real
   dt, since those are wall-clock durations. `KBL_FIXED_STEP=0` reverts.
   **Remaining honest limit: a 4-zone deck has almost no spatial resolution, so
   some visible stepping is inherent** — G-Helper's maintainer notes the internal
   4-zone keyboard has no firmware animation mode at all, so the host must
   recalculate each zone a few times a second. Do not promise per-key smoothness
   on this hardware.
3. **DECISION PENDING — should the exclusive HID lock also cover the ASUS
   vendor collection?** Owner asked to hold until the log settles it, and wants
   it explained simply before deciding. The open question: our engine opens the
   *LampArray* collection (usage page 0x59, found by scoring every HID
   top-level collection for LampArray usages), while G-Helper enumerates by
   ASUS VID/PID. Windows gives each top-level collection its own device path,
   so an exclusive lock on one does NOT block the other. Evidence they may be
   the same collection: our own `PinGate` sends G-Helper's exact brightness
   packet (`0x5a 0xBA 0xC5 0xC4 <level>`) as a feature report on the LampArray
   handle and it works. G-Helper's maintainer confirms the keyboard is driven
   over USB HID, not WMI. **Test that settles it:** after `Restart.bat`, log
   should read `lock exclusive, fallbacks 0`; then change something in
   G-Helper's keyboard panel and watch. If our lighting survives, same
   collection, nothing to do. If G-Helper wins, extend the lock to the vendor
   collection - with the known risk that it may also carry Fn brightness and
   other ASUS functions.
   **Update from the 2026-11-12 log — this decision matters less than it did.**
   The contention is on the collection we ALREADY lock: our own LampArray open was
   refused, so whatever holds the keyboard is on that same device path. Extending
   the lock to the ASUS vendor collection would not have fixed any of it; actually
   ACQUIRING the lock we already ask for does (`edca7f5`). The settling test above
   still stands, and the periodic line now names the holder, so the next log should
   identify the process without a separate experiment. **Recommendation if the
   owner asks: leave the scope exactly as it is.** Do not extend it on spec.
4. **M4/M5 brightness keys cost a UAC prompt per keypress — owner must rebind.**
   Reported 2026-11-12: "anytime I want to use the M4 and M5 to change the
   master brightness it makes me go through the UAC". Cause: G-Helper's Custom
   action launches `KeyboardLighting.exe -BrightUp`, which only writes a token
   into `%LOCALAPPDATA%\KeyboardLighting` and exits — no privileges needed —
   but the manifest said `requireAdministrator`, and Windows enforces that at
   process CREATION, before `wWinMain` runs. So a non-elevated G-Helper got a
   consent dialog on every single press. Owner chose the G-Helper rebind as the
   answer and authorised the manifest change too ("do whatever you need to").
   **Shipped:** manifest -> `asInvoker` (the only suite app so treated; the
   other five still require administrator and the PE test now pins both sides).
   The panel keeps its admin token three ways — the `/RL HIGHEST` logon task, an
   elevated ProjectHub parent, and the existing `runas` self-elevation — so the
   only processes that run unelevated are the transient `--stop` / `-HoldMode` /
   `-BrightUp` / `-BrightDown` branches, which all return before the
   single-instance mutex and the elevation block. **Owner action: NOTHING in G-Helper - just
   update to 2.8.8.** The owner showed the current bindings (2026-11-12): both
   M-keys are Custom actions running `...\TestsSuite\KeyboardLighting\
   KeyboardLighting.exe` with `-BrightUp` / `-BrightDown`, which is exactly the
   path the manifest fix repairs. Leave them.
   **The earlier advice to rebind to `Ctrl+Alt+Up`/`Down` was WRONG and is
   retracted in the README.** Reason, worth not re-deriving: the hold-to-sweep
   depends on the token. `LearnFromRing` is called only from `WatchBrightTokens`
   on a fresh token, so the keystroke route never teaches the panel which HID
   code is M4 vs M5, and holding does nothing after the first step; it is also a
   10% `StepMasterBrightness` step rather than the engine's paced 5%. Worse, if
   the codes are ALREADY learned in presets.ini the keystroke route DOUBLE-steps
   - the hotkey fires and the hold streamer fires. G-Helper does accept
   space-separated keycodes in the same Custom field (`0x11 0x12 0x26` =
   Ctrl+Alt+Up, max 3 keys per its maintainer), so that stays documented as the
   no-update stopgap and the worse of the two. Update path is Project Hub's
   Branch picker -> `arena/e6c1980c-tests` (`FALLBACK_BRANCH_W`), so the fix is
   reachable without merging. Test
   `test_keyboard_lighting_brightness_taps_never_cost_a_uac_prompt`, 58 tests,
   mutations A-D detected. **Also found while mutation-testing:** the
   pre-existing `/RL HIGHEST` assertion was satisfiable by the *comments* inside
   `EnsureLogonTask` — it discussed the flag twice in prose, so stripping the
   flag from the actual schtasks command passed. Now strips comments before
   asserting. Worth remembering for any substring test over a function that
   documents the thing it is checking for.
   **Architectural note, unresolved:** M4/M5 brightness currently DEPENDS on
   G-Helper, while the watchdog actively contains G-Helper and `edca7f5` makes
   our exclusive HID lock far more likely to succeed. The panel already receives
   the ASUS key stream (VID 0x0B05, report 0x5A) through raw input, which needs
   no device handle and so cannot be blocked by our lock — but the first tap and
   the M4-vs-M5 learning are both driven by G-Helper's token. If M4/M5 stops
   working once the lock starts winning, that is why, and the durable fix is a
   Learn button in the panel so it produces the tap itself. Owner was offered
   this and chose the smaller path for now.
   **Hold behaviour now reachable (`f0ca00d`).** The engine has supported
   tap/latch/burst all along and re-reads `hold_mode.txt` every tick, but the
   only way to set it was the `-HoldMode` command line - so the two modes that
   do NOT depend on the panel recognising the M-keys were effectively
   unreachable, while default `tap` degrades SILENTLY: its hold-to-sweep needs a
   typematic repeat or a release frame, and `HasReleaseSrc` is the actual gate
   (`g_scanBound != 0 || g_hidRelSeen >= 2`). On hardware that sends one press
   report and no release, holding does nothing and every press is one 5% step
   with nothing anywhere to explain it. **That is the most likely reason the
   owner's hold does not work, and it is not fixable host-side** - hence the
   tray submenu offering all three modes plus a greyed `HoldReadyLabel()` line
   reading `Hold Ready` / `Still Learning` / `Not Recognised`. Mode is READ from
   the file on every menu open rather than cached in a global, so it stays
   correct when `-HoldMode` set it out of process. `latch` (one press glides,
   same press stops, rail stops) and `burst` (4 steps = 20% per press) both run
   entirely in the engine and need no key recognition at all, so they are the
   answer on this hardware.
   Also added a ceiling to `HoldPump`: `HasReleaseSrc` is monotonic, so the
   existing 300ms silence timeout switches itself off permanently the moment
   learning succeeds, and a release that then fails to arrive would stream
   tokens forever with brightness pinned at the rail. 10s, chosen against the
   engine's own numbers rather than by guess - `$step = 50` on a 0..1000
   `LiveLevel` at `$brightStepMs = 150` is a 3s full sweep, so a real hold
   always reaches the rail first and the ceiling can only stop a stream with
   nothing left to do. It re-arms `g_scanArmed` so the next press reads as
   fresh. Test `test_keyboard_lighting_hold_modes_are_reachable_and_bounded`
   (59 tests); the ceiling is compared against the sweep time PARSED from the
   engine, so changing `$step` or `$brightStepMs` re-checks the invariant
   instead of silently invalidating it. 8 mutations detected. **Not yet
   validated on the machine** - the tray submenu is GUI and the indicator is the
   thing that will tell us which failure mode the owner actually has.
   **Sweep smoothness + Windows-style flyout (`c5ea0c7`).** Owner sent a screenshot
   of the Windows flyout and asked for both. Sweep was 5% per 150ms = twenty
   visible treads; now tap and glide are separate controls: a tap keeps the
   coarse 5% for an instant response, the glide runs 1% per 30ms (~100 treads
   over the same 3s). Needed three supporting changes that are easy to miss:
   the main loop sleep dropped 70 -> 30ms or the ramp can only fire as fast as
   the loop wakes (test asserts rampMs >= loopMs); the panel's hold-token pace
   dropped 150 -> 60ms so `brightHoldMs` could shrink 400 -> 120ms and cut the
   post-release overshoot from ~13% to ~4%; and Set-Live's EC-gate re-assert is
   rate limited to 500ms because a 33Hz ramp would otherwise send a WMI call
   plus a HID feature report per step, competing with frame writes. The
   flyout is now theme-aware via AppsUseLightTheme, draws a round thumb with a
   separating ring, and dropped the percentage text because the Windows flyout
   has none - flag that to the owner as reversible. EXE rebuild, 2.8.9 -> 2.8.10.
5. **Bluetooth Guard phantom filter + Forget button — owner is testing now.**
   `a2a1e21` (label 2.8.8): never-connected devices are filtered out
   (`HasConnectedBefore` requires a real `{8-4-4-4-12}` service-GUID subkey, not
   just any subkey) and each device card has a Forget button doing a TRUE unpair
   via `BluetoothRemoveDevice` (op==3 on the worker thread), not a profile
   disable. Confirm a paired-but-never-connected device no longer appears, that
   Forget actually removes it from Windows' device list, and that manual connect
   still works afterwards.
6. **AudioSwitcher panel on the owner's screen** (as of the reachability fix):
   with the usual outputs plus several apps playing, every row and the footer
   are reachable - by fitting or by scrolling - and the app list names the apps
   that are actually playing. Check at 150% scaling, after dragging the window
   to a monitor with a different scale, scrolling with the wheel while a slider
   has focus, and holding a slider thumb while the 2-second poll fires. Clicking
   Open in the hub (app already in the tray, and elevated) should surface and
   focus the window. An app on ANY output should appear, tagged
   `app.exe - on <device>` when it is not on the default.
7. **Needs Windows / in-game validation** (cannot be tested in this sandbox):
   after hub 2.8.38, one Update click writes the suite folder copy AND every
   stray copy (owner's case: Audio Switcher stuck 9 days old in
   `Programs\TestsSuite\AudioSwitcher` while Downloads got each build);
   Audio Switcher card shows its own blurb + working buttons after a hub refresh;
   `Update-SpatialHUD.bat` installs `spatial-hud-1.0.4.jar` under its real name
   and self-updates from `main`; notes quick-capture page still posts.
8. If the owner still sees a UAC prompt on manual double-click launches, offer
   routing shortcuts through a silent task launcher (`schtasks /Run`).
9. Method 3 (`VirtualHudPlane.textureU/V`) still divides UVs by
   `guiWidth/guiHeight` — the mapping bug fixed in Method 4 (1.0.4) is unfixed
   there. Fix it if Method 3 ever reports the same symptom.

- **Clipboard Memory removed from the suite entirely** (owner: "get rid of
  clipboard memory entirely" -> hard-delete, not archive; git history keeps it).
  It was the LAST hub slot (7), so slots 0-6 did not renumber. Deleted
  `clipboard-memory/` + `docs/brand/icon-clipboard-memory.png` and every
  reference: hub source (built-in count 8->7 across the slot-indexed arrays,
  `known[]`, the `BuiltInBtnId` map/off[] window, `MIRROR_CLIP`, the dir-detect
  block, the card init, the refresh block, the remote size/SHA probe, and the
  three WM_COMMAND handlers), `suite-apps.json` (8->7 apps), `build_windows.py`
  (7->6 EXEs), `hub.rc` (icon 11), `generate_suite_icons.py`, and the tests
  (dropped the clipboard show-event test + its APPS entry; retuned the
  built-in-count and AppendInstallFolder-floor assertions). Bumped
  `HUB_VERSION`+`HUB_VERSION_W` to 2.8.41. 48 tests OK; check_all green.

- **Orca + Spatial HUD hub cards trimmed to match the others** (owner: "i want
  the cards for orca and hud to match the others moreso, but they only need a
  folder, remove, and update buttons (updates only show when needed)" ->
  clarified: both cards get Update, Folder, Remove, Disable; Update is first and
  shows ONLY when an update is available; drop the HUD "Mods Dir" picker and the
  Orca "Branch" picker; Orca needs a real Remove that deletes the installed
  plugin files, and keeps Disable). Hub 2.8.40.
  - **Both cards render Update / Folder / Remove / Disable** in that order. The
    `suite-apps.json` `buttons` string drives the runtime card; the C `g_cards[]`
    initializers are the offline fallback - both updated. Update is `btns[0]`, so
    the existing dynamic style logic targets it: the HUD hides `btns[0]` when the
    mod is up to date or disabled and reveals it when the jar is missing or a
    newer build exists; Orca mirrors that (`hidden = !upd`, plus a new
    not-installed branch that reveals it).
  - **Orca's Remove is new** (`ID_ORCA_REMOVE`): it deletes exactly what
    `WorkerInstallOrcaPlugins` writes - across every `%APPDATA%\OrcaSlicer*`
    profile, the two `orca_plugins` packages (files + their folders) and the two
    legacy top-level plugin files - then refreshes the card. Added
    `{ "remove", ID_ORCA_REMOVE }` inside Orca's `BuiltInBtnId` window (retuned
    `off[]`, map 31->30 entries) or the verb resolves to 0 and the button is
    dropped silently.
  - **Orca's Disable was a no-op** (`g_orcaDisabled` was toggled but never
    loaded, saved, or checked). It is now persisted (`LoadAppsCfg`/`SaveAppsCfg`
    `OrcaDisabled`) and gates `WorkerInstallOrcaPlugins`, and the card shows a
    Disabled badge + Enable label like the HUD card.
  - Bumped `HUB_VERSION` + `HUB_VERSION_W` to 2.8.40 together (new
    ProjectHub.exe). 5 new tests pin the button sets, a Python reimplementation
    of `BuiltInBtnId` that proves every catalog verb resolves inside its OWN
    slot's `off[]` window (the check the catalog test misses), the Remove
    handler's targets, the persisted+gating Disable, and the hidden-unless-needed
    Update; 5 negative controls (drop the map entry / revert the JSON buttons /
    remove the handler / drop the Disable persistence+gate / remove a HUD
    hidden=FALSE) all caught. 49 tests OK; check_all green.

- **PrintScreen (and other keys) dropped while Keyboard Lighting ran** (owner:
  "i cant use printscreen while in any of our apps"). Keyboard Lighting installed
  a system-wide `WH_KEYBOARD_LL` hook (`KbHoldProc`) on its UI thread to watch the
  M4/M5 brightness keys. A low-level hook must be serviced promptly by its thread,
  but that same thread blocks for seconds in the engine (re)start waits
  (`WaitForSingleObject(pi.hProcess, 2500..8000)`); Windows times a stalled hook
  out and DROPS the keystroke machine-wide, and PrintScreen is the most visibly
  broken. It was also the ONLY global key interception in the whole suite (no app
  has a WM_KEYDOWN handler), which is why it hit "any of our apps".
  - Fix: removed the hook entirely and repaired the raw-input path that was meant
    to replace it. `InitRawInput(g_hwnd)` was being called BEFORE `CreateWindowExW`,
    i.e. with a NULL hwnd, so `RegisterRawInputDevices` had no target and no
    `WM_INPUT` ever arrived - the hook was doing all the work. Moved the call to
    just after the window is created. Raw input (`RIDEV_INPUTSINK`, keyboard usage
    1/6 plus the ASUS 0x0B05 HID devices) is queued to our own message loop via
    `OnRawInputMsg`, which already fed `OnScanEvent` (keyboard MakeCode) and
    `OnRawHid` (vendor HID) - so M4/M5 keep working, now also covering the HID path
    that was dead, with no system-wide interception and no keystroke drops. All
    detection stays on the UI thread (WM_INPUT), so no threading change.
  - `test_keyboard_lighting_watches_keys_via_raw_input_not_a_global_hook` pins it
    (no SetWindowsHookEx / KbHoldProc; WM_INPUT + OnScanEvent(MakeCode) present;
    InitRawInput after CreateWindowExW); 2 negative controls (re-add the hook /
    move InitRawInput before the window) both caught. NEEDS Windows re-validation
    of M4/M5 brightness since the input mechanism changed. KeyboardLighting.exe
    rebuilt; strict 7-bit ASCII kept.

- **Every hub app focuses its window when Open is clicked** (owner: "all apps
  should focus when the open button is clicked"; the hub launches each app's exe
  with no args, so this is each app's own show/focus path). Audited all six apps
  with an Open button and fixed the gaps; no hub change was needed.
  - **Clipboard Memory was fully broken** - the exact bug Audio Switcher had. It
    always started hidden (`ShowWindow(hwnd, SW_HIDE)`) and a second launch did
    `OpenEventW`+`SetEvent` on `SHOW_EVENT_NAME`, but the first instance never
    created that event and nothing watched it, so Open-from-hub did nothing while
    it sat in the tray. Now the first instance creates the event, its 50 ms emblem
    timer (which runs even while hidden) polls it and restores+shows+foregrounds,
    an explicit launch shows+focuses instead of hiding, and the second instance
    keeps a `FindWindowW` fallback.
  - **Bluetooth Guard, Keyboard Lighting, DNS Pilot, Agent Notes** already
    resurfaced+focused a running instance (timer poll / FindWindow /
    `MsgWaitForMultipleObjects` on the show event) but only *showed*, not
    foregrounded, on a fresh explicit launch, so the window could come up behind
    the hub. Added `SetForegroundWindow` to each first-launch show (the `!silent`
    path; the silent tray/logon start still stays hidden).
  - Audio Switcher was already done last commit. `AppOpenFocusTests` pins both
    halves for all six apps (an explicit launch foregrounds + a running instance
    is resurfaced) plus the Clipboard Memory event lifecycle; negative controls
    (drop CM's CreateEvent / revert CM to unconditional hide / drop BTG's
    foreground) all caught. All five changed EXEs rebuilt.

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
