# Repository Current State (`STATE.md`)

**Active Working Branch:** the current `arena/<id>-tests` session branch — read it
from git (`git rev-parse --abbrev-ref HEAD`), never hardcode it. A hardcoded name
here went stale twice and sent `tools/sync_branch.sh` at the wrong branch.
**Last Updated:** 2026-10-30

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
| **Project Hub** | `project-hub/src/project_hub.c` | 2.8.34 | **GREEN** — catalog card text now fully restored before every string-pool reset (see gotcha below); update nag only for genuinely newer builds; requireAdministrator |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` | 2.8.7 | **GREEN** — rules-ghost device list; `HighestAvailable` logon task |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `Aura-Background.ps1` | 2.8.7 | **GREEN** — M4/M5 hold modes; strict 7-bit ASCII, C# 5.0 |
| **DNS Pilot** | `dns-pilot/src/dns_pilot.c` | size-tracked | **GREEN** — parallel rank test; `TestsSuite-DnsPilot` ONLOGON task |
| **Agent Notes** | `agent-notes/src/agent_notes.c` | size-tracked | **GREEN** — POSTs to `tools/notes_server.py`; queues in `%LOCALAPPDATA%\AgentNotes\outbox\` while the agent is offline |
| **Audio Switcher / Clipboard Memory** | `audio-switcher/`, `clipboard-memory/` | size-tracked | **GREEN** — catalog-driven hub cards |
| **Spatial HUD** | `spatial-hud-template-26.3/` | mod 1.0.4, updater 4 | **GREEN** — flicker + bottom cut-off fixed (1.0.4, span-based UV mapping in `purplePanelState`) |
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

1. **Needs Windows / in-game validation** (cannot be tested in this sandbox):
   Audio Switcher card shows its own blurb + working buttons after a hub refresh;
   `Update-SpatialHUD.bat` installs `spatial-hud-1.0.4.jar` under its real name
   and self-updates from `main`; notes quick-capture page still posts.
2. If the owner still sees a UAC prompt on manual double-click launches, offer
   routing shortcuts through a silent task launcher (`schtasks /Run`).
3. Method 3 (`VirtualHudPlane.textureU/V`) still divides UVs by
   `guiWidth/guiHeight` — the mapping bug fixed in Method 4 (1.0.4) is unfixed
   there. Fix it if Method 3 ever reports the same symptom.

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
