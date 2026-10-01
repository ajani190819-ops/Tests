# Roadmap — what's planned, what's in flight, what's blocked

Last updated: 2026-09-30. Keep this file current: **a plan that isn't written
down here doesn't exist.**

This file is the *plan*. The *state* — what is done, what was decided, what
the last session left half-finished — lives in [`../MEMORY.md`](../MEMORY.md).
Read that one first.

---

## Status at a glance

| # | Work | Status |
|---|------|--------|
| A | One-click updater (`Update-Orca-Plugins.bat`) + catalogue + contract test | **Done and merged** (PR #1) |
| B | Repo reorganization (plugins/ + tools/ + tests/ + docs/ + AGENTS.md) | **Done and merged** (PR #1) |
| C | Wave Overhangs as a standalone **post-processing script** | Planned — **design settled 2026-09-30, unblocked** |
| D | Unlayered Infill: make it actually work | **Done 2026-09-30** — diagnosed, standalone shipped, plugin now runs under test and logs to Downloads |
| E | Updater self-updates itself + stages the standalone tools | **Done 2026-09-30** — code written, **never run on Windows** |
| F | Double-click branch chooser + strict test-build installs | **Verified on Windows 2026-09-30** after v1.2.1 fixes |
| G | Stable plugin identities + PDF contract audit | **Built 2026-09-30** — names verified in real Orca |
| H | Wave real-export repair on `Cube^2.STL` | **0.0.12 built offline 2026-10-01** — transactional replacement works visibly in real Orca; actual modal Z and edge cleanup added, awaiting print rerun |

**PR #1 is merged** (2026-09-30, from `arena/01a0f42b-tests`), so `main` now
carries `plugins.json` and both plugin files — **the updater is live**. A .bat
downloaded from the repo today fetches the catalogue and installs for real.
(Before the merge it fell back to its hardcoded list, which was complete, so
it worked either way — but the catalogue path is the live one now.)

---

## C. Wave Overhangs as a post-processing script

### What the fork actually does (researched from dennisklappe/OrcaSlicer-WaveOverhangs, 2026-09-30)

The fork is a C++ fork of OrcaSlicer (itself a port of
stmcculloch/PrusaSlicer-WaveOverhangs; algorithm by Janis A. Andersons). The
generator, per layer:

1. **Detect the overhang**: `overhang = this layer's area − layer below's
   area` (grown by a small tolerance). The anchor is the overlap with the
   layer below.
2. **Seed** a narrow band at the boundary between supported and unsupported
   material.
3. **Propagate**: repeatedly offset the accumulated covered region outward by
   `line_spacing` (default 0.35 mm) and emit a polyline along the new front.
   Fronts diffract around corners and holes "like ripples on a pond". Stop
   when a new front adds less than `min_new_area` (0.01 mm²) or nothing new.
4. **Order the fronts**: `smart` (start each line from its better-supported
   end — default), `zigzag` (connect into a back-and-forth meander), or
   `monotonic` (each line separately).
5. **Print them gently**: 2 mm/s, part-cooling fan forced to 100%, flow is an
   absolute `nozzle_diameter²` mm³/mm (0.16 for a 0.4 mm nozzle — a wave line
   hangs in air, so layer height doesn't set the bead size), optional
   end-of-line retraction (wave lines end in mid-air and dribble).
6. **Carve**: the wave-covered area is subtracted from the layer's normal
   infill/perimeters so Orca doesn't double-print it, and the N perimeters
   inside the overhang stay untouched (`wave_overhang_outer_perimeters`,
   default 1). Layers *above* the wave get solid "floor layers" (default 2).

Known limitation (their docs): **warping** of laterally supported overhangs —
thermal contraction, shape-memory effects, nozzle pressure on large spans.
Waves are for *small, self-contained* overhangs; big cantilevers still need
supports. PLA works best.

### Our plan (design settled — see the answered questions below)

Our existing plugin already ports steps 1–5 as a pure-geometry module
(`wave_core`, shapely, no Orca bindings). What it does NOT have is a way to
run without Orca's plugin system feeding it slice polygons at `posSlice` —
which is the part that has never been proven on a real build.

A **standalone post-processing script** (same shape as
`tools/nonplanar-infill-tool`) removes that dependency entirely:

1. Parse the exported G-code into layers; per layer, rebuild the printed
   footprint by buffering every extruding move by half its line width.
2. `support` = previous layer's footprint, `layer` = current footprint — feed
   both to the existing `wave_core` planner. Everything is already in bed
   coordinates, so the fragile `_bed_offset` calibration disappears.
3. Splice the wave moves into the G-code at the end of each layer, with the
   fork's speed/fan/flow/retraction treatment. Idempotent stamp, like the
   unlayered engine.
4. **Remove** the slicer's own extrusions inside the wave-covered region, so
   the waves are not printed on top of ordinary infill (decision C2 below).
5. Ship as `plugins/wave-overhangs/wave_overhangs_post.py`: double-click
   window mode, CLI mode, and `--inplace` for Orca's *Post-processing
   scripts* setting so it can run automatically on every export.

Dependency: shapely (+ numpy). The script will check for it and print a
plain-English `pip install shapely` instruction if missing (a standalone
script can't lean on Orca's dependency installer).

### Answered — these are decided, do not reopen (owner, 2026-09-30)

* **C1 — do the pipeline-plugin versions stay? → YES, keep both forms**,
  sharing one engine. The plugin auto-runs inside Orca when it works; the
  script always works on any Orca version. Consequence: the engine stays a
  separate, front-end-agnostic module so one fix lands in both, and no
  plugin gets retired.
* **C2 — replace or reinforce? → REPLACE**, matching the fork. The slicer's
  own infill/perimeter moves inside the wave-covered region come out of the
  G-code, so there is no double material. That means careful surgery: find
  the extrusions whose footprint falls inside the wave region for that layer
  and drop them, keeping the `wave_overhang_outer_perimeters` perimeters
  inside the overhang. A "waves on top" mode was offered and **declined** —
  do not build `--reinforce` unless asked.

---

## D. Unlayered Infill: make it actually work

### What we know

* The **engine** (inlined in the plugin as `nonplanar_core`) is a pure
  text-in/text-out rework of the reference tool with six test-pinned fixes
  (extrusion attached to the right move, no duplicated points, Z restored on
  exit, Orca's skin markers recognized, taper can't invert, per-XY-column
  floor/roof bracketing instead of one global list). It is believed good.
* The **plugin wrapper** (Orca capability classes, `psGCodePostProcess`) is
  the only part that depends on Orca's plugin system — which the handoff
  says has never been proven end-to-end on a real OrcaSlicer.
* The owner used `tools/nonplanar-infill-tool` (the simpler predecessor)
  **successfully, retroactively, on exported G-code** — proving the
  approach works on real files from the owner's own slicer.

### Plan

1. ~~**Get the symptom.**~~ **Answered 2026-09-30 — see D1 below.**
2. ~~**Ship a standalone script**~~ **DONE 2026-09-30.**
   `plugins/unlayered-infill/unlayered_infill_post.py` wraps the existing
   engine with the reference tool's proven UX: double-click window, CLI,
   `--inplace`, `--dry-run`, `--full-strength`, never overwrites the input,
   refuses absolute E, and reports what it did — including *why* when it did
   nothing. The engine is stored verbatim in both files;
   `tests/test_post_script.py` fails if they drift.
   **Tested for real** on synthetic sliced G-code: 54 infill moves → 1080
   segments, extrusion conserved to 1.6e-12 mm, Z restored on every section
   exit, second pass a no-op, absolute-E refused.
3. ~~**Fold the learnings back** into the plugin version~~ **DONE
   2026-09-30 (owner's follow-up).** The plugin now logs the same detail, and
   more, to `<Downloads>/orca-plugins.log`.

### Owner's follow-up, 2026-09-30 — what was asked and what was true

> *"the unlayered infill should work like my version, picking a frequency and
> amplitude (not in mm but in % of the layer height, eg 200% will give .6mm if
> given a .3mm layer height). also having a grid of nozzle size (.6mm) columns
> to blend the sin effect so that areas with local ceilings and floors do
> [not] mess everything up globally. also lets keep it as a pipeline plugin,
> just make the plugins write logs to my downloads folder."*

Three of the four already existed and simply were not the defaults — worth
knowing before rebuilding anything:

| Asked for | Status before | Done |
| --- | --- | --- |
| amplitude as % of layer height | parser already supported `%`/`x`; default was `-0.2` mm | default is now `"200%"` |
| pick frequency | already configurable | unchanged (1.5 /mm) |
| nozzle-size column grid, local skins stay local | **already built** — `SolidGrid`, per-column floor/roof, `cell_mm=0.6`, `blend_mm=2.0` | default is now `"auto"` = nozzle diameter read from the G-code |
| stay a pipeline plugin | always was | unchanged; the standalone stays alongside it (C1) |
| logs in Downloads | logged JSONL *next to the plugin file*, unfindable | plain text in `<Downloads>/orca-plugins.log`, both plugins sharing one file |

Versions: Unlayered Infill **0.2.1 → 0.3.0**, Wave Overhangs **0.0.4 → 0.0.5**.

**`tests/test_plugin_runtime.py` is new and matters**: it loads the plugin
against `tests/fake_orca.py` and drives it the way Orca would. This is the
first time anything in `plugins/` has ever been executed. It proves the
plugin loads, registers its two capabilities, rewrites G-code at
`psGCodePostProcess`, refuses absolute E, is idempotent, honours preset
config, and writes the log. It cannot prove behaviour *inside* OrcaSlicer.

**`tools/sync_engine.py`** pushes the engine from the standalone (the
readable copy) into the plugin's string literal, so "one engine, two front
ends" is mechanical rather than hopeful.

### D1 — answered 2026-09-30

The owner reported: **"they don't do anything — no change in the preview, and
none when reopening the G-code file."** Diagnosis, in order of likelihood:

1. **The preview can never show it, and that is not a bug.** Orca builds the
   preview from the slice; `psGCodePostProcess` runs afterwards, at export,
   and nothing redraws the preview
   ([OrcaSlicer#7489](https://github.com/OrcaSlicer/OrcaSlicer/issues/7489)).
   BrickLayers tells its users the same. **Half the reported symptom is
   expected behaviour.**
2. **Post-processing only runs on "Export G-code file"** — not on "Print" or
   "Send" ([#4432](https://github.com/SoftFever/OrcaSlicer/issues/4432)).
   If the owner pressed Print, the plugin never ran at all. **Prime
   suspect**, and it also explains the second half: if it never ran, the
   reopened file is genuinely unchanged.
3. **Absolute E.** Unlayered Infill refuses M82 G-code by design; the refusal
   only surfaces in the result message, never in the file.
4. **The default wave is small** — tapered, peaking at half amplitude, so
   ~0.09 mm on a 0.2 mm layer. Visible in a Z-height view, easy to miss
   otherwise. `--full-strength` doubles it.
5. **Plugin failed to load** (Wave Overhangs needs numpy + shapely) — a
   traceback would be in `data_dir()/log/python_*.log`.

Still unconfirmed: which button the owner pressed, and whether a plugin was
selected in the preset at all. The standalone script sidesteps all five.

* The form-factor question is settled: **C1 applies here too — keep both the
  plugin and the standalone script, sharing one engine.**

---

## E. Updater: self-update, and staging the standalone tools

**Done 2026-09-30. Written and statically checked, but never executed on
Windows — there is no Windows in the sandbox. Treat as unproven.**

The owner asked: *"do I need a new installer download, or can we make it
update itself?"*

* **Plugin updates never needed a new download** and still don't: the .bat
  fetches `plugins.json` and every plugin file fresh from `main` on each run.
* **Updater changes** are now handled by `:self_update`. On each run it
  downloads the latest `Update-Orca-Plugins.bat` to `%TEMP%`, and if the
  version differs it hands the run over to that copy.

**It deliberately does not overwrite itself.** `cmd.exe` streams a batch file
from disk by byte offset as it executes, so a self-overwrite can jump into
garbage mid-run — and a bad download would leave the owner with no working
updater at all. Delegating instead means the on-disk file is never at risk;
it simply always runs the newest logic. This is now hard rule 12 in
`AGENTS.md`, enforced by `tests/test_installer.py`.

Guards: `ORCA_UPDATER_CHILD` (no infinite recursion), `NO_SELF_UPDATE` /
`--no-self-update` / `ORCA_NO_SELF_UPDATE`, and `--local`. The download must
pass the existing 2000-byte floor **and** contain both `set UPDATER_VERSION=`
and `rem UPDATER_VERSION <v> end`, so a 404 page or a wifi captive portal is
never executed. Version is `1.1.0`, stored twice (a `set` line the script
uses, a `rem` line `:self_update` greps); the test pins them equal.

`:stage_tools` also drops `unlayered_infill_post.py` into
`%USERPROFILE%\Downloads\OrcaPlugins`, so the standalone tool arrives
without a separate hunt on GitHub. The test checks every staged path exists
in the repo, so the list can never 404.

**The one thing that can never auto-update** is `:self_update` itself. If
that logic needs fixing, the owner has to re-download once.

### Not done

* No Windows test. First real run is the test.
* `wave_overhangs_post.py` is not staged because it does not exist yet (§C).

---

## F. Install and test any branch without Command Prompt

**Built and verified on Windows 2026-09-30.** The first run exposed two bugs:
PowerShell 5.1 nested the API array, and the small catalogue hit the plugin
size floor. Both were fixed in updater 1.2.1 with regression tests; the owner
reran the corrected chooser and strict branch installation successfully.

The owner needs to test an Arena branch before merging it. Branch names change
every chat, so copying a name into Command Prompt was not a useful workflow.

### Settled design

* Ship a separate `Choose-Orca-Plugin-Version.bat`. Keeping the menu separate
  means a normal double-click on `Update-Orca-Plugins.bat` still installs the
  released `main` branch exactly as before. The chooser downloads and validates
  the updater from the selected branch into `%TEMP%`; it never tests a branch
  with an older adjacent updater and never overwrites either running `.bat`.
* The chooser fetches the public GitHub branch API without a login or token.
  It shows released `main`, the five newest test branches, an all-branches
  view, and manual entry. It remembers the chooser's selection under
  `%LOCALAPPDATA%`, with a conspicuous **Return to released main** option.
* If GitHub's branch list is unavailable, offer manual entry, retry, or cancel.
  Never silently select a different build.
* A test branch is all-or-nothing. Download and validate its catalogue and
  every selected plugin before changing Orca's folders. If anything is
  absent or invalid, stop and name it. Never borrow a missing file from
  `main`.
* The updater prints the selected branch in a large banner at the beginning
  and end. It prints planned versions before installation and the versions
  actually read from successfully installed plugin files afterwards.
* Do not self-update from a test branch. An old updater copy stored on that
  branch could replace the strict current updater and reintroduce fallback.
  Released-main runs retain normal self-update behavior.

### Verification boundary

`tests/test_installer.py` statically checks both batch files and replays the
all-or-nothing branch preflight. This can catch missing guards and ordering
mistakes. It cannot execute Windows `cmd.exe`, the live menu, or PowerShell's
GitHub calls. The owner's first Windows run remains the real test.

---

## G. Stable plugin identities and PDF contract audit

**Built 2026-09-30; awaiting a real Orca rerun.**

The owner's real Wave Overhangs test showed it installed, activated, and
selected, but it never appeared in the shared log. The repository's newly
added Plugin Development PDF supplied the missing identity fact: a preset's
full capability reference includes both `plugin_name` and `capability_name`.
The old `<Name> v<version>` plugin names therefore changed a saved identity on
every release.

* Owner rejected confusing compatibility names and chose one final migration.
  Both permanent package/capability identities are version-free: `Wave
  Overhangs` and `Unlayered Infill`. Releases 0.0.9 / 0.3.4 carry that
  migration; the names must never change again. Unlayered's complete 0.3.0
  control set is pinned as defaults instead of depending on an old config key.
* Wave Overhangs 0.0.7 did not solve the real problem: reopened exported
  G-code still retained the ordinary bridge. The identity diagnosis is
  disproven. Next work is same-export, geometry-bounded removal of only bridge
  segments actually replaced by successfully generated wave paths.
* Contract tests pin the PDF's required shape: root-level dependencies,
  exactly one `@orca.plugin` package subclassing `orca.base`, typed capability
  bases, correct execute signatures, and matching registration.
* Orca's bundled `uv` resolver installs PEP 723 dependencies. The plugin must
  not invoke `pip`; Wave continues to declare numpy and shapely and import them
  at module load before audited capability calls.
* Side-loaded Python metadata cannot populate Orca's dedicated Changelog tab;
  that data comes from a cloud listing. The supported visible routes remain
  the Description tab and Check setup, both generated from each plugin's
  changelog. If published to Orca Cloud, copy the same history into the cloud
  listing's Changelog field.

---

## Parked / non-goals

* Support Fins — superseded by an official cloud plugin; source stays in
  `ajani190819-ops/support-fins`. Do not resurrect here.
* `keyboard-lighting/` — stored as-is, not part of the Orca work.
* Anything requiring a private repo or authenticated downloads.
* A signed .exe installer (would remove the "Unknown Publisher" prompt
  entirely) — costs a yearly code-signing certificate and a packaging
  pipeline; the .bat + one-time "Unblock" is good enough. Revisit only if
  this ever grows up.
