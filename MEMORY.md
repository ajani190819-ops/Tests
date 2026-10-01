# MEMORY.md — current repository handoff

Read `AGENTS.md` first. This file is the current state, not a replacement for
that rulebook. `docs/ROADMAP.md` is the plan; `docs/ORCA-PLUGIN-FACTS.md` is the
binding record of OrcaSlicer behavior.

* **Last updated:** 2026-10-01, Wave refinement and repository cleanup session.
* **Repository:** `ajani190819-ops/Tests`, public.
* **Session branch:** `arena/01a0f4f1-tests`. Never switch branches or push to
  `main`.
* **Latest commit:** `252af89 Merge internal-hole Wave history`,
  pushed on the session branch after the obstacle-aware diffraction fix.
* **Current versions:** Wave Overhangs 0.0.15, Unlayered Infill 0.3.4,
  updater 1.4.0.
* **Permanent identities:** `Wave Overhangs` and `Unlayered Infill`. Release
  numbers must remain out of package and capability names.

## One-minute orientation

This repository contains two experimental OrcaSlicer pipeline plugins, a
Windows updater, a strict branch chooser, standalone Unlayered Infill tooling,
reference documentation, and tests.

Important locations:

- `Update-Orca-Plugins.bat` — released-`main` default updater. It keeps CRLF,
  never overwrites itself while running, and installs from the selected ref.
- `Choose-Orca-Plugin-Version.bat` — branch chooser. It validates and runs the
  updater downloaded from exactly the selected branch; it never fills a test
  branch from `main`.
- `plugins.json` — catalogue and version source used by the updater.
- `plugins/wave-overhangs/` — Wave plugin and its release notes.
- `plugins/unlayered-infill/` — Unlayered plugin, standalone tool, and notes.
- `tests/fixtures/` — the supplied `Cube^2.STL` and captured
  `Cube^2_3m53s.gcode` real-export fixture.
- `docs/reference/` — supplied OrcaSlicer wiki/PDF snapshots used for the
  plugin contract and lifecycle facts.
- `tests/` — fake Orca harness, installer/runtime/audit tests, standalone
  engine test, and the real-export Wave regression.
- `keyboard-lighting/` — unrelated personal project. Do not reorganize or
  modify it.
- `orca-plugins.log` is runtime output, not source; it is ignored and must not
  be committed.

## Current implementation

### Wave Overhangs 0.0.15

The active implementation is one transactional G-code pass at
`psGCodePostProcess`:

1. Parse actual exported layers, modal XY/Z, relative-E mode, line widths, fan
   state, and `Bridge` / `Internal Bridge` sections.
2. Build the previous layer's support footprint and the current bridge footprint
   from actual extrusion paths in absolute bed coordinates.
3. Generate expanding wavefronts from supported material through unsupported
   bridge area; internal holes in that plane are treated as obstacles so fronts
   continue around both sides.
4. Simplify wave polylines, remove isolated short fronts, and merge short
   endpoint stubs so sub-nozzle edge chatter does not become blobs.
5. Order fronts using configurable smart, monotonic, or zigzag patterns plus
   deterministic endpoint policies. Travel between fronts stays non-extruding.
6. Remove only bridge centerline portions covered by generated Wave paths and
   re-emit substantial uncovered fragments with proportional extrusion.
7. Restore the expected XY, fan, and E mode/value state. Uniform absolute-E
   sections restore `M82` and `G92`; mixed E-mode sections remain untouched.
8. Write only after parsing, generation, subtraction, and assembly succeed. Any
   exception returns the original G-code unchanged.

The Wave pass uses the bridge move's actual modal nozzle Z. In the supplied
fixture, the nominal `;Z:` comments differ from the actual height because the
profile contains a 0.25 mm Z offset. The 0.0.15 fixture output uses 5.650,
9.850, and 14.650 mm for the three Wave blocks.

The captured regression reports three Wave layers, 103 covered bridge moves,
31 substantial retained fragments, 111 tiny remnants removed, 30 short Wave
fronts removed, 386 simplified Wave extrusion moves, restored fan state, exact second-pass
idempotence, and byte-for-byte unchanged output after deliberate generation failure. The owner
confirmed that the previous 0.0.11 output visibly produced perimeter-conforming
waves in real Orca. A fresh 0.0.15 export and physical print remain open.

There is no standalone Wave post-processing script in this repository. The
plugin waits for exported Bridge G-code; do not claim a Wave standalone tool
is installed or tested.

### Unlayered Infill 0.3.4

Unlayered retains the complete working control set from 0.3.0: amplitude as a
percentage of layer height, frequency, segment length, automatic nozzle-width
column grid, blending, full-strength mode, relative-E safety, and logging.
The standalone script and pipeline plugin share one engine. Edit the readable
standalone engine, run `tools/sync_engine.py`, and verify the copies match.
Preserved Orca settings can be cleared with the plugin UI's **Restore defaults**
control.

## Verified evidence

These checks have passed in the repository:

- `python3 tests/test_installer.py` — catalogue, updater fallback, selected
  branch structure, package names, version surfaces, install replay, and batch
  safety.
- `python3 tests/test_post_script.py` — standalone Unlayered transformation,
  extrusion conservation, Z restoration, relative-E refusal, input safety, and
  idempotence.
- `python3 tests/test_plugin_runtime.py` — fake-Orca registration, logging,
  configuration, G-code behavior, and recoverable Wave dependency reporting.
- `python3 tests/test_plugin_audit.py` — both plugins import under a hook that
  denies filesystem writes at import time.
- `PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py` — captured real
  export, Wave replacement, cleanup, actual Z, fail-closed behavior, fan
  restoration, and idempotence.
- `python3 tools/sync_engine.py --check` — Unlayered engine copies match.
- `python3 tools/sync_changelog.py --check` — embedded plugin notes match their
  source changelogs.
- Raw CRLF assertions — `Update-Orca-Plugins.bat` has 689 CRLF lines and
  `Choose-Orca-Plugin-Version.bat` has 136 CRLF lines.
- Python compilation and `git diff --check` pass for normal text files.

The owner's real-machine evidence is separate from those static/sandbox
checks: Unlayered previously rewrote a real export, and Wave 0.0.11 visibly
appeared in a reopened real export. The sandbox cannot run OrcaSlicer, Windows,
or a printer.

## Important decisions and safety rules

- Wave replaces covered bridge extrusion; it does not reinforce the original
  bridge. Uncovered material is retained unless it is geometrically covered by
  successfully generated Wave paths.
- Wave no longer mutates pre-export slice geometry or depends on `_PLAN`,
  object-to-bed calibration, or mutable geometry surviving between callbacks.
- A Wave parse, geometry, generation, insertion, or assembly failure retains
  the original exported G-code.
- Both plugins must be idempotent because Orca can invoke export processing more
  than once.
- Third-party dependencies are imported at module load because Orca's audit
  rules can block lazy imports during capability execution.
- Logging is lazy, bounded, best-effort, and never allowed to break a print.
- The updater's own batch file must never overwrite itself while it is running.
- A selected test branch is all-or-nothing. Missing or invalid files stop the
  install before Orca's folders change.
- Do not touch `keyboard-lighting/`.
- Keep GPL-3.0 attribution for Unlayered Infill and the predecessor tool.

## What remains to do

1. Install this branch with the chooser and confirm Wave 0.0.15 in Orca's
   separate Version column.
2. Export `Cube^2.STL` again at the owner's 0.30 mm / 0.60 mm settings.
3. Reopen the exported G-code and inspect Wave Z alignment and cleaned outer
   edges.
4. Save the fresh export and Downloads log if behavior differs from the
   fixture.
5. Perform a small physical print; no physical Wave result is claimed yet.
6. Run the Windows batch flow again whenever either batch file changes.

## Session log

### 2026-10-01 — repository cleanup

Asked for a repository-wide organization pass. The prior branch state was
fetched and verified byte-for-byte before local alignment. The supplied PDFs
were moved to `docs/reference/`; the model and captured G-code were moved to
`tests/fixtures/`; runtime `orca-plugins.log` was removed from source control
and added to `.gitignore`. README, plugin guides, roadmap, Orca facts, AGENTS,
and changelog content were reconciled with the current implementation. The
obsolete Wave slice-object planning helpers and their stale documentation were
removed from the active plugin module; the one-pass G-code design remains.
Tests and synchronization checks are the release gate before commit.

### 2026-10-01 — Wave 0.0.15

The owner asked for smoother, cleaner fronts and more control. The current
configuration now documents every setting, including obstacle propagation,
pattern, endpoint, component ordering, cleanup, extrusion, speed, fan, and
iteration safety. `component_order="nearest"` can reduce same-distance travel
moves while preserving near-to-far anchoring; the safe default remains
`component_order="support"`. The Cube regression remains unchanged.

A fresh 0.0.15 Orca export and physical print remain unverified.

### 2026-10-01 — Wave 0.0.14

The owner supplied a comparison showing a stray diagonal through a circular
opening. The likely cause was safe boundary geometry being simplified into a
straight chord, not legitimate diffraction. Wave cleanup now checks the
simplified front against the allowed unsupported region and its interior holes;
if a shortcut would cross a void, it keeps the original curved boundary. A
circular-hole regression protects this case, and the existing Cube measurements
remain unchanged.

A fresh 0.0.14 Orca export and physical print remain unverified.

### 2026-10-01 — Wave 0.0.13

The owner reported three refinements: a sharp extruding handoff when the nozzle
returned to the original bridge, remaining small edge blobs, and a desire for
pattern/start controls. Wave now emits a non-extruding `G0` to every replaced
segment endpoint before any retained original `G1` move. Cleanup has configurable
minimum front and segment lengths, simplification tolerance, and retained
fragment threshold. `smart`, `monotonic`, and `zigzag` patterns plus deterministic
endpoint policies are available. The embedded geometry diffracts around support
boundaries and now treats internal holes in the unsupported plane as obstacles;
synthetic concave, internal-hole, and circular-hole regression checks accompany
the Cube fixture. The fixture passes with 386 Wave extrusion moves and no
Wave-block-to-retained-extrusion handoff without a G0. Monotonic is a mechanical
near-to-far ordering, not a thermal guarantee: recently printed plastic may
still be warm and soft.

A fresh 0.0.14 Orca export and physical print remain unverified.

### 2026-10-01 — Wave 0.0.12

The owner reported that Wave was finally visible but one layer low and had
small edge dots. The captured export showed nominal `;Z:` comments and actual
moves separated by the profile's 0.25 mm Z offset. Wave now uses actual modal
Z, derives coverage width from the exported bridge, simplifies wavefronts, and
drops sub-nozzle retained fragments. The captured fixture passes with three
layers and 436 cleaned Wave extrusion moves.

### 2026-10-01 — Wave 0.0.11

Replaced the ineffective slice-object/cross-callback architecture with one
transactional G-code bridge replacement pass. Real captured-export evidence
showed three generated layers, covered bridge removal, retained uncovered
fragments, fan restoration, fail-closed generation, and idempotence. The owner
then confirmed visible waves in real Orca.

### Earlier sessions

- The chooser was made strict: selected branch only, all-file preflight, safe
  manual/API failure behavior, and selected-branch updater execution.
- Plugin identities were made permanently version-free after Orca preset
  identity research.
- Unlayered Infill received its standalone tool, complete controls, shared
  engine synchronization, relative-E refusal, and Downloads logging.
- Import-time filesystem writes were removed from both plugins and guarded by
  the audit test.
