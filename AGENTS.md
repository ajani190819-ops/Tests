# AGENTS.md — read this before doing anything in this repo

This is the rulebook for any AI assistant (or human contributor) working here.
If you are an AI, everything in this file is an instruction from the repository
owner and it is **high priority**: it outranks your own defaults wherever the
two disagree.

**Start every session by reading `.agent/STATE.md`, then `.agent/INDEX.md`.**
They identify the active work and the smallest set of facts needed for it. Do
not load large historical or unrelated documents by default. A new chat has no
memory of the last one, so update `.agent/STATE.md` before ending a session.

Companion documents, mandatory only when the current task needs them:

* `.agent/INDEX.md` — topic map, reading order, and agent-document rules.
* `.agent/facts/` — short, confirmed technical facts by topic.
* `.agent/decisions/` — accepted design constraints and their reasons.
* `.agent/runbooks/` — repeatable test and release procedures.
* `docs/ORCA-PLUGIN-FACTS.md` — binding OrcaSlicer facts. Read before changing
  an Orca plugin; do not re-derive or contradict it.
* `docs/ROADMAP.md` — Orca plugin plans and open questions. Read before
  planning Orca work; update it when that plan changes.
* `MEMORY.md` — a redirect to preserved history. Do not use as an active
  handoff.

The division of labour: `.agent/STATE.md` is **where we are**,
`.agent/facts/` is **what is known**, `.agent/decisions/` is **why a constraint
exists**, and `docs/ROADMAP.md` is **where Orca work is going**. Keep them from
contradicting each other.

---

## 0. Starter step — translate the owner's message first (mandatory every turn)

The owner often dictates by voice or types quickly without worrying about
spelling, punctuation, run-on sentences, or technical terminology. Treat messy
phrasing as normal input, never as ambiguity by itself.

**Before touching code or asking questions on ANY message, perform this starter
step:**

1. **Translate the raw message into a crisp action list.** Separate every part
   of what the owner said into four buckets:
   * **Do now** — concrete changes requested for this turn.
   * **Do next (queued)** — items the owner mentioned for *after* the current
     pass ("once we're done with this...", "after that..."). Record these in
     `.agent/STATE.md` right away so they are never forgotten, and do **not**
     mix half-finished work on later items into the current pass unless asked.
   * **Boundaries / constraints** — anything the owner asked not to change yet.
   * **Decisions needed from the owner** — only genuine forks in behaviour
     where guessing wrong wastes time.
2. **Map user symptoms to engineering root causes internally.** The owner will
   describe what they see on screen ("it crashes and I can't right-click the
   tray icon to quit", "my laptop steals my earbuds from my phone and cuts my
   audio"). Translate those symptoms into the underlying Windows / Python / JS /
   Java mechanics yourself rather than asking the owner technical questions.
3. **Show a short "What I understood" check at the very top of your reply**
    whenever a message has multiple parts or casual phrasing — 2 to 5 short,
   plain-English bullets with zero filler — then immediately deliver the work
   or ask one compact batch of options if a real choice is needed.

---

## 1. Who you are working for

The owner of this repo is **a beginner** at software internals. That is a
design constraint, not an apology:

* **Explain plainly.** Short sentences. Define every piece of jargon the first
  time it appears (there is a glossary at the bottom of this file — use and
  extend it). "The .bat reads plugins.json" beats "the installer consumes the
  manifest".
* **Never hide a failure.** If something did not work, or was not tested on
  real hardware, say so in plain words. An honest "I ran the Linux checks, not
  Windows desktop" beats pretending it ran on Windows.
* **Beginner-safe defaults.** Tools must never destroy the user's input files,
  must say what they did, and must stop cleanly with a plain-English fix rather
  than produce broken output.

## 2. How to work — the standards

1. **Write out what is about to happen, before it happens.** For multi-step
   work, keep a clear plan and update `.agent/STATE.md` when active work shifts.
   Update `docs/ROADMAP.md` only for Orca plugin planning.
2. **Ask instead of guessing on real product choices.** When a user-facing
   behaviour could go multiple ways (for example, how a new desktop app should
   behave at startup), present 2–4 plain options with a recommended default.
3. **Work in reviewable steps and verify every edit.** Run the repo's checks
   (`python3 tools/check_all.py`) and project tests after changes. Confirm
   every edit landed cleanly. Separate `Tested` from `Not tested on Windows /
   in-game`.
4. **Report in three parts:** what changed / what it means for you / what is
   next. Keep it concise and scannable.
5. **Leave clean state for the next session.** Update `.agent/STATE.md` before
   finishing so the next chat knows exactly what is done and what is queued.

### 2a. How to communicate with the owner

Use direct, plain language. The owner wants useful answers, not an AI-style
performance.

1. **Answer first.** Open directly with the translated checklist and result —
   no praise, recap fluff, or filler phrases.
2. **Use only the detail needed to support understanding or decisions.**
   Explain a technical term briefly when it matters, then use ordinary words.
3. **Use clear status labels** (`Done`, `Tested`, `Not tested`, `Queued next`,
   `Need from you`) when helpful. Never imply a build check proves live hardware
   or game runtime behavior.
4. **Do not manufacture certainty or enthusiasm.** Avoid phrases such as
   "great progress", "rest assured", or unverified promises.
5. **Keep formatting functional.** Use short headings and bullets; skip
   motivational closes or generic offers of help.

## 3. Repo map

```
Orca-Plugins.bat              THE ONE FILE AN ORCA USER DOWNLOADS, and the only
                              top-level .bat in the repo: menu, live branch
                              picker, OrcaSlicer folder picker, remembered
                              choices, plugin installer, tool stager, and safe
                              temp-copy self-update.
plugins.json                  catalogue the updater reads (id, name, version, paths)
AGENTS.md                     permanent agent rulebook — read first
MEMORY.md                     redirect to preserved legacy history
.agent/
  STATE.md                    active status, verified work, and queued next tasks
  INDEX.md                    topic map and document maintenance rules
  facts/                      confirmed technical facts by topic
  decisions/                  accepted design constraints and their reasons
  runbooks/                   repeatable test and release procedures
  history/                    dated context; do not load by default
.github/workflows/
  ci.yml                      runs tools/check_all.py + Arena Link tests on push/PR
  build-spatial-hud.yml       JDK 25 Gradle build + JUnit tests + release upload
plugins/
  wave-overhangs/             OrcaSlicer pipeline plugin: support-free steep overhangs
  unlayered-infill/           OrcaSlicer pipeline plugin: non-planar interlocking infill
spatial-hud-template-26.3/    canonical Minecraft 26.3 Fabric client mod + updaters
arena-link-windows/           Windows Electron companion app + node:test suite
keyboard-lighting/            Windows keyboard RGB tray & background engine utility
                              (do NOT modify unless the owner asks to work on it)
tools/
  bump_version.py             bumps plugin versions across code, .bat, JSON, & docs
  check_all.py                runs all Python checks (both dep states) + Arena Link tests
  dump_default_config.py      refreshes docs/config-reference/*.json
  sync_changelog.py           embeds recent changelog bullets into plugin Check setup
  sync_engine.py              copies shared nonplanar_core engine into plugin file
  nonplanar-infill-tool/      standalone predecessor GUI/CLI script (reference, GPL-3.0)
archive/                      retired prototypes and reference artifacts (not shipped)
tests/                        automated regression & contract tests for plugins/updater
```

The `plugins.json` entry `path` is a URL path relative to the repo root using
forward slashes. `Orca-Plugins.bat` downloads from
`https://raw.githubusercontent.com/ajani190819-ops/Tests/<ref>/<path>`, so
`plugins.json` and the fallback row in `Orca-Plugins.bat` must always stay in
lockstep (`tests/test_installer.py` enforces this).

## 4. Hard rules — breaking these breaks real users

1. **The repo must stay public.** `Orca-Plugins.bat` makes unauthenticated
   `raw.githubusercontent.com` downloads; a private repo 404s every file.
2. **`Orca-Plugins.bat` must stay CRLF (`\r\n`) on every line.** `.gitattributes`
   pins `*.bat -text`. Never write it with Python text mode (`Path.write_text`
   converts CRLF to LF — always use `read_bytes`/`write_bytes`). Keep it as the
   only top-level `.bat` in the repo (`tests/test_installer.py` enforces both).
3. **Version bumps move in lockstep via `tools/bump_version.py`.** Never put a
   version number inside a plugin `name` or capability name — Orca saves those
   strings inside process presets, so changing them orphans user presets. Keep
   package names permanently `Wave Overhangs` and `Unlayered Infill`. Always
   prepend `## <version> — <date>` with `*` bullets first in
   `plugins/<id>/CHANGELOG.md`, add a dated entry in root `CHANGELOG.md`, and
   run `sync_engine.py`, `sync_changelog.py`, `dump_default_config.py`, and
   `check_all.py`.
4. **No `|`, `^`, `%`, `!` in any `plugins.json` field** — they break `cmd.exe`
   and pipe-delimited plan parsing.
5. **Exactly two capabilities per plugin** in `plugins.json`.
6. **Do not modify or reorganize `keyboard-lighting/` unless the owner asks to
   work on `keyboard-lighting`.** When the owner *does* ask to fix or improve
   `keyboard-lighting/`, edit it cleanly in place while preserving existing user
   configs under `%LOCALAPPDATA%\KeyboardLighting`.
7. **GPL-3.0 attribution must survive** in `plugins/unlayered-infill/` and
   `tools/nonplanar-infill-tool/` (derived from Roman Tenger's NonPlanarInfill).
8. **Git discipline:** work only on the active session branch handed to you
   (`arena/<id>-tests`). Commit and push only to that branch, never straight to
   `main`, never force-push, and never commit personal logs, crash dumps, or
   credentials.
9. **Orca plugin facts in `docs/ORCA-PLUGIN-FACTS.md` are binding:** import all
   standard-library and third-party modules at module top level (never lazily
   inside a capability), keep module import free of disk I/O, and keep G-code
   rewrites idempotent because `psGCodePostProcess` can fire twice.
10. **Keep the shared `nonplanar_core` engine synchronized.** Edit
    `plugins/unlayered-infill/unlayered_infill_post.py` between the
    `BEGIN/END nonplanar_core` markers, then run `python3 tools/sync_engine.py`.
11. **Logging defaults to Orca plugin storage and must never break a print.**
    `log_path()` writes `orca-plugins.log` inside `orca.host.plugin.storage()`
    so slicing never triggers permission popups (`ORCA_PLUGIN_LOG_DIR` is a
    debug-only override). Every `_log()` call must swallow `BaseException`.
12. **Never make `Orca-Plugins.bat` overwrite itself while running.** Download
    the newer copy to `%TEMP%`, verify its markers and semantic version, and
    delegate to it.
13. **One entry `.py` file per Orca plugin folder.** Stage standalone tools into
    `%USERPROFILE%\Downloads\OrcaPlugins\tools\` (`%TOOLDIR%`), never alongside
    plugin entry files.

## 5. How to verify your work

```bash
pip install --break-system-packages -q -r requirements-dev.txt
python3 tools/check_all.py             # runs all Orca checks (with & without deps),
                                       # sync checks, doc-version checks, and Arena Link tests
```

Individual commands when debugging one component:

```bash
python3 tests/test_installer.py        # catalogue, .bat, headers, and install replay
python3 tests/test_post_script.py      # standalone engine & CLI behaviour
python3 tests/test_plugin_runtime.py   # plugin config, backups/restore, & runtime logs
python3 tests/test_plugin_audit.py     # Orca import/capability audit hook safety
python3 tests/test_unlayered_waves.py  # Unlayered Infill wave shapes & backwards compat
python3 tests/test_wave_gcode.py       # Wave Overhangs real G-code fixture (needs numpy+shapely)
cd arena-link-windows && npm test      # Electron main/helper unit tests + syntax check
```

For `spatial-hud-template-26.3/`, pushing to any `arena/**` branch triggers
`.github/workflows/build-spatial-hud.yml` on GitHub Actions with JDK 25 to
compile the Fabric mod and execute its JUnit 5 test suite.

## 5a. The release playbook — bump a plugin version cleanly

1. Prepend `## <version> — YYYY-MM-DD` (leading with `*` summary bullets) to
   `plugins/<id>/CHANGELOG.md` and `CHANGELOG.md`.
2. Run:
   ```bash
   python3 tools/bump_version.py <plugin-id> <version>
   python3 tools/sync_engine.py
   python3 tools/sync_changelog.py
   python3 tools/dump_default_config.py
   python3 tools/check_all.py
   ```

## 6. Glossary

* **catalogue** — `plugins.json`: the list of plugins the updater installs.
* **updater** — `Orca-Plugins.bat`: single self-updating Windows batch file that
  manages build selection, OrcaSlicer folder selection, and installation.
* **PEP 723 header** — the `# /// script` comment block at the top of each
  plugin file carrying its name, version, description, and dependencies.
* **sidecar** — `.install_state.json` written next to an installed plugin so
  OrcaSlicer recognizes and enables both capabilities immediately.
* **pipeline plugin** — an OrcaSlicer Python plugin that hooks into slicing and
  G-code export steps (`psGCodePostProcess`). Requires Orca > 2.4.2 or nightly.
* **post-processing script** — a standalone script run directly on an exported
  `.gcode` file (CLI, GUI, or Orca's *Post-processing scripts* setting).
* **CRLF** — Windows line endings (`\r\n`), required on every line of `.bat`
  files.
* **partialTick (`tickDelta`)** — the fraction (`0.0` to `1.0`) between two 20 Hz
  Minecraft game ticks used so rendered elements move smoothly at full FPS.
