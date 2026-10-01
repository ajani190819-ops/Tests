# MEMORY.md — current repository handoff

Read `AGENTS.md` first. This file is the current state, not a replacement for
that rulebook. `docs/ROADMAP.md` is the plan; `docs/ORCA-PLUGIN-FACTS.md` is the
binding record of OrcaSlicer behavior.

* **Last updated:** 2026-10-01, Wave perimeter-conformance and arc-move session (0.0.21).
* **Repository:** `ajani190819-ops/Tests`, public.
* **Session branch:** `arena/01a0f908-tests`. Never switch branches or push to
  `main`.
* **Latest code state:** Wave Overhangs 0.0.21 measures the overhang against
  the layer's real wall moves, so Wave ends land on the wall and hole
  perimeters instead of the castellated bridge-line edge, and it both reads
  and writes G2/G3 arc moves; Geometry 0.1.4 remains an experimental
  alternate.
* **Current versions:** Wave Overhangs 0.0.21, Wave Overhangs Geometry 0.1.4,
  Unlayered Infill 0.3.4, updater 1.4.0.
* **Permanent identities:** `Wave Overhangs`, `Wave Overhangs Geometry`, and
  `Unlayered Infill`. Release numbers must remain out of package and capability
  names.

## One-minute orientation

This repository contains three experimental OrcaSlicer pipeline plugins, a
Windows updater, a strict branch chooser, standalone Unlayered Infill tooling,
reference documentation, and tests.

Important locations:

- `Update-Orca-Plugins.bat` — released-`main` default updater. It keeps CRLF,
  never overwrites itself while running, and installs from the selected ref.
- `Choose-Orca-Plugin-Version.bat` — branch chooser. It validates and runs the
  updater downloaded from exactly the selected branch; it never fills a test
  branch from `main`.
- `plugins.json` — catalogue and version source used by the updater.
- `plugins/wave-overhangs/` — exported-G-code Wave plugin and its release notes.
- `plugins/wave-overhangs-geometry/` — separate preview-visible
  `posPrepareInfill` Wave fill-surface prototype and notes.
- `plugins/unlayered-infill/` — Unlayered plugin, standalone tool, and notes.
- `tests/fixtures/` — the supplied `Cube^2.STL` and captured
  `Cube^2_3m53s.gcode` real-export fixture.
- `docs/reference/` — supplied OrcaSlicer wiki/PDF snapshots used for the
  plugin contract and lifecycle facts.
- `tests/` — fake Orca harness, installer/runtime/audit tests, standalone
  engine test, the real-export Wave regression, and the geometry-stage Wave
  regression.
- `keyboard-lighting/` — unrelated personal project. Do not reorganize or
  modify it.
- `orca-plugins.log` is runtime output, not source; it is ignored and must not
  be committed.

## Current implementation

### Wave Overhangs 0.0.21

The active implementation is one transactional G-code pass at
`psGCodePostProcess`:

1. Parse actual exported layers, modal XY/Z, relative-E mode, line widths, fan
   state, and `Bridge` / `Internal Bridge` sections.
2. Build the previous layer's support footprint, the current layer's wall
   material (wall moves joined into loops first, then given their width), and
   the current bridge footprint, all from actual extrusion paths in absolute
   bed coordinates.
2b. Square the bridge footprint up against that wall material: a closing
   operation fills the narrow channel Orca leaves between its last bridge line
   and the wall, ends finish 25% of a line width inside the wall bead
   (`wall_overlap`), and nothing may be placed outside the part. The decision
   about *where* a Wave belongs is unchanged -- the area still has to come from
   exported bridge extrusion over unsupported space. `wall_snap=false`
   restores the 0.0.19 edges for comparison.
3. Generate expanding wavefronts from supported material through unsupported
   bridge area; internal holes in that plane are treated as obstacles so fronts
   continue around both sides.
4. Simplify wave polylines with a topology guard, merge connected obstacle
   fronts before cleanup, retain interior curve points, and remove only short
   endpoint stubs. Drop isolated short fronts so edge chatter does not become
   blobs. Surviving front endpoints remain on the valid overhang wall or hole
   boundary.
5. Order fronts using configurable smart, monotonic, or zigzag patterns plus
   deterministic endpoint policies. Travel between fronts stays non-extruding.
6. Remove only bridge centerline portions covered by generated Wave paths and
   re-emit substantial uncovered fragments with proportional extrusion.
7. Restore the expected XY, fan, and E mode/value state. Uniform absolute-E
   sections restore `M82` and `G92`; mixed E-mode sections remain untouched.
8. Snap Wave endpoints back onto non-support detail boundaries by extending in
   the Wave endpoint direction, then taper E on existing moves by default.
   Optional centerline clearance and taper micro-segmentation exist for
   comparison but are off by default; the support-side anchor boundary is
   excluded from snap, clearance, and taper.
9. Write only after parsing, generation, subtraction, and assembly succeed. Any
   exception returns the original G-code unchanged.

The Wave pass uses the bridge move's actual modal nozzle Z. In the supplied
fixture, the nominal `;Z:` comments differ from the actual height because the
profile contains a 0.25 mm Z offset. The 0.0.20 fixture output uses 5.650,
9.850, and 14.650 mm for the three Wave blocks.

The captured regression reports three Wave layers, 104 covered bridge moves,
28 substantial retained fragments, 115 tiny remnants removed, 16 short Wave
fronts removed, three wall-bounded sections, 488 default Wave moves (713 with
`wall_snap=false`, which still shows the 0.0.19 castellated edges), 44 arcs
replacing 180 straight moves when the profile asks for arc fitting, restored fan
state, exact second-pass idempotence, and byte-for-byte unchanged output after
deliberate generation failure. It also measures edge quality directly: Wave
ends along each wall of the Cube's overhang now lie on one line within
0.02 mm (0.0.19 varied by 0.29 mm), no end stops in the 0.001-0.30 mm "just
short of the wall" band, and a new synthetic overhang-with-hole export puts
every hole end on one radius within 0.001 mm with nothing inside the hole or
outside the part. The owner confirmed that the previous 0.0.11 output visibly
produced perimeter-conforming waves in real Orca. A fresh 0.0.20 export and
physical print remain open.

There is no standalone Wave post-processing script in this repository. The
plugin waits for exported Bridge G-code; do not claim a Wave standalone tool
is installed or tested.

### Wave Overhangs Geometry 0.1.4

A separate experimental geometry-stage plugin now runs at `posPrepareInfill`.
It reads live `LayerRegion.fill_surfaces` and the previous layer's `lslices`,
generates obstacle-aware fixed-spacing Wave fronts, converts them to narrow
preview ribbons, removes tiny clipped dot islands, and replaces only reachable
unsupported prepared fill with `stBottomBridge` Wave ribbons. It deliberately
leaves `LayerRegion.slices` alone, so Orca keeps the original overhang perimeter
instead of generating dark-blue overhang-wall loops around every Wave ribbon.
Bridge fill surfaces are handed to Orca from the supported side outward. It
keeps the post-processing Wave plugin unchanged as the fallback.

Current Orca bindings expose existing `ExtrusionPath` objects read-only, so
this is preview-visible Wave fill-surface geometry rather than direct raw path
injection or path reordering. The plugin documents that limitation, snapshots
fill surfaces, rolls back an object on mutation failure, and fails closed when
dependencies or host fill-surface bindings are unavailable. Routine logs/state
still default to plugin storage to avoid normal approval prompts. Synthetic
geometry, installer, runtime, audit, G-code, sync, compilation, whitespace, and
CRLF checks passed for 0.1.4 in the sandbox; a real Orca preview and physical
print remain unverified.

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
  configuration, G-code behavior, and recoverable Wave dependency reporting
  for both Wave capabilities.
- `python3 tests/test_plugin_audit.py` — all shipped plugins import under a
  hook that denies filesystem writes at import time.
- `PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py` — captured real
  export, Wave replacement, cleanup, actual Z, fail-closed behavior, fan
  restoration, and idempotence.
- `python3 tools/sync_engine.py --check` — Unlayered engine copies match.
- `python3 tools/sync_changelog.py --check` — embedded plugin notes match their
  source changelogs.
- Raw CRLF assertions — `Update-Orca-Plugins.bat` has 690 CRLF lines and
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
- Export processing must be idempotent because Orca can invoke it more than
  once. The geometry prototype also processes each PrintObject once per slice
  execution and rolls back failed object edits.
- Third-party dependencies are imported at module load because Orca's audit
  rules can block lazy imports during capability execution.
- Logging is lazy, bounded, best-effort, and never allowed to break a print.
- The updater's own batch file must never overwrite itself while it is running.
- A selected test branch is all-or-nothing. Missing or invalid files stop the
  install before Orca's folders change.
- Do not touch `keyboard-lighting/`.
- Keep GPL-3.0 attribution for Unlayered Infill and the predecessor tool.

## What remains to do

1. Install this branch with the chooser and confirm all three plugin versions
   in Orca's separate Version column.
2. Run a fresh current-Orca slice with `Wave Overhangs Geometry` selected and
   confirm the edited ribbons appear as bridge fill in the normal preview while
   the original overhang perimeter remains intact.
3. Export `Cube^2.STL` again at the owner's 0.30 mm / 0.60 mm settings with the
   original post-processing Wave plugin and inspect Z alignment and the new
   wall-conforming Wave edges.
3a. Turn **Arc fitting** on in the print profile for that export, so the
   arc paths get exercised in real Orca and real firmware. Check the printer
   accepts the G2/G3 Wave blocks and that curved walls still read back
   correctly (they now arrive as arcs).
3b. The owner offered a fresh export plus screenshots of the jagged 0.0.19
   result from their own model (one with holes would be the most useful). Ask
   for it, drop it in `tests/fixtures/`, and re-measure edge straightness
   against it before claiming 0.0.20 is correct on real geometry.
4. Save the fresh export and the plugin-storage log path reported by Check
   setup if behavior differs from the fixtures.
5. Perform a small physical print; no physical Wave result is claimed yet.
6. Run the Windows batch flow again whenever either batch file changes.

## Session log

### 2026-10-01 — Wave speaks arcs; the hole file-size bug (0.0.21)

The owner asked whether Wave could emit arc moves, since they can switch Arc
fitting on in their print profile. Three things came out of it.

1. Wave runs at `psGCodePostProcess`, after Orca has written the file, so
   Orca's arc fitter never sees Wave's moves. Wave now fits its own arcs.
   `arc_fitting` defaults to `auto`, which follows the export's own
   `enable_arc_fitting` line, so a printer whose firmware cannot read G2/G3
   never receives one. An arc has to pass through the kept points, must not
   bow more than a quarter of the rung spacing, and is re-checked against the
   overhang region afterwards so a bow cannot push into a wall or a hole.
2. The reverse direction mattered more. Wave's parser ignored G2/G3 entirely.
   With arc fitting on, a round hole's wall is exported as arcs, so Wave would
   have gone blind to that wall and quietly lost the 0.0.20 perimeter fix on
   the parts that need it most. The parser now expands `I J` and `R` arcs;
   a hole wall written as four arcs gives the same wall material as the same
   hole written as 72 straight moves (10.208 vs 10.207 mm^2).
3. A file-size bug, found while measuring the arcs. Cleanup refused to
   simplify any front that *touched* a hole, and 0.0.20 had just made ends
   finish on hole walls, so 46 of 58 fronts fell back to raw rasterised form:
   a 9.9 mm front written as 980 moves instead of 9. Touching is no longer
   treated as crossing (the void is shrunk by the guard margin before the
   test), and a shortcut that really would cut a corner now retries at a
   tighter tolerance before giving up. The synthetic hole part's Wave G-code
   went from 316 KB to 9 KB.

Sandbox measurements only. The owner will re-export with Arc fitting on.

### 2026-10-01 — Wave ends snap to the real perimeter (0.0.20)

The owner reported that Wave ends would not snap to the overhang perimeter:
instead of progressing from the supported perimeter all the way out to the
overhang perimeter and around holes, fronts finished on a jagged edge that made
it hard to lay the following outer perimeters down.

Diagnosis (reproduced in the sandbox against the captured Cube export): Wave
measured the overhang from the footprint of Orca's exported bridge *lines*. The
union of those line footprints is castellated — alternating in and out by about
half a line width — and stops short of the wall, and the fronts were clipped to
it. The 0.0.19 "snap to boundary" step could not help because it snapped to
that same castellated boundary and its guard refused to move an end more than
0.08 mm.

Fix: parse `Outer wall` / `Inner wall` / `Overhang wall` moves, join them into
loops before giving them width, close the bridge footprint against that wall
bead, and let ends finish 25% of a line width inside the bead. Two safety
rules keep it honest: the growth may reshape a Wave area but never create one
(the area must still contain bridge extrusion over unsupported space), and
nothing may be placed deeper than the overlap into the outer wall, because a
wall loop is not always a sealed band.

Two further bugs were found and fixed along the way: a wall buffered one G-code
move at a time leaves a hairline slit at every vertex of a curved wall (a Wave
end slipped through one and finished on the visible surface of a hole), and
reaching for a wall could put fronts in the gaps between the sparse-infill
lines of the layer below.

Sandbox evidence is in the regression; a real Orca export and a physical print
remain open. The owner will supply a fresh export and screenshots on request.

### 2026-10-01 — Wave endpoint taper without default micro-moves

After the owner marked rectangular/grid-like endpoint texture in preview, Wave
Overhangs moved to 0.0.19. `edge_taper_segment` now defaults to `0`, so taper
changes E on existing straight moves instead of adding tiny endpoint subdivision
moves. Endpoint snapping now extends along the Wave endpoint direction to reach
the wall/hole boundary, avoiding sideways doglegs from nearest-point snapping.
Sandbox regressions pass; a real Orca export/print remains open.

### 2026-10-01 — Wave endpoints snap to perimeter by default

After the owner showed that v0.0.17's default centerline clearance left visible
gaps around walls and holes, Wave Overhangs moved to 0.0.18. `edge_clearance`
now defaults to `0` and is only a comparison/debug control. The active default
snaps Wave endpoints back onto nearby non-support detail boundaries before
applying endpoint flow taper, so the paths should conform to wall and hole
perimeters more like Orca's normal bridge infill while still reducing blobs.
Sandbox regressions pass; a real Orca export/print remains open.

### 2026-10-01 — Wave edge clearance before taper

Wave Overhangs moved to 0.0.17. The post-processing plugin now clips emitted
Wave centerlines back from non-support detail boundaries before applying endpoint
flow taper. The old cleaned Wave paths still drive bridge-removal coverage, so
straight Bridge fragments do not reappear just because the visible Wave bead was
inset. `edge_clearance="auto"` follows the exported bridge width; set it to `0`
to compare against the previous full-length endpoint behavior. Sandbox
regressions pass; a real Orca export/print remains open.

### 2026-10-01 — Wave endpoint flow taper

After the owner decided to stick with the original post-processing Wave plugin,
Wave Overhangs moved to 0.0.16. The geometry/path planner stays the same, but
the emitted Wave G-code now tapers E near endpoints that touch outer walls,
holes, or concave detail boundaries. The support-side anchor boundary is
excluded from tapering so starts keep full flow. `edge_taper_distance=0`
restores the old no-taper 399-move Cube output for comparison. The post-
processing plugin already used Orca plugin storage for log/state writes, so the
no-approval-prompt behavior remains in place.

### 2026-10-01 — Wave Geometry fill-surface method

After the owner clarified that the preview showed Wave shapes but they were
still behaving as overhang-wall islands, the geometry prototype moved from
`LayerRegion.slices` at `posSlice` to `LayerRegion.fill_surfaces` at
`posPrepareInfill`. This should preserve Orca's original perimeter and make the
unsupported Wave ribbons bridge-classified fill surfaces instead of separate
slice islands. It still cannot guarantee final bridge/perimeter G-code order
because generated `ExtrusionPath` collections remain read-only in Python.
Sandbox verification passed for the implementation in this branch.

### 2026-10-01 — Wave Overhangs Geometry prototype

The owner chose a separate alternate plugin that runs before G-code so Wave can
interact with Orca's geometry and appear in the preview. Current Orca source
bindings expose editable `LayerRegion.slices` and `Layer.make_slices()`, but
existing `ExtrusionPath` collections are read-only. Added
`plugins/wave-overhangs-geometry/` v0.1.3: it generates obstacle-aware Wave
fronts and writes a non-bridge outer wall shell plus bridge-classified interior
Wave ribbons at `posSlice`, while keeping the post-processing Wave plugin as
fallback. It reports the raw-path limitation, uses object-level rollback on
mutation failure, and fails closed when the host API or dependencies are
unavailable.

The owner confirmed the geometry version registers in Orca and is much cleaner
than the previous version, then reported remaining preview issues: tiny dots,
missing/broken outer overhang wall, overhang-wall classification instead of
bridge, and a thin-air start order. Version 0.1.3 addresses those with dot
filtering, a continuous non-bridge outer wall shell, `stBottomBridge` interior
Wave ribbons clipped inside that shell, and support-outward bridge surface
ordering. It also moves routine logs/state to plugin storage to avoid normal
approval prompts. Synthetic Wave geometry, circular-hole safety, installer,
runtime, audit, changelog, compilation, and CRLF checks pass. A follow-up
real-Orca preview of 0.1.3 and physical printing remain unverified.

### 2026-10-01 — Wave endpoint cleanup

The supplied screenshots showed that the topology was correct but individual Wave
starts and ends were frayed around the outer wall and circular opening. The
cleanup now keeps the simplified interior curve points, removes only short
stubs at each endpoint, and merges connected obstacle-front pieces before
cleanup. Geometry tests assert that surviving front endpoints stay on the valid
wall or hole boundary and that cleaned curves remain inside the allowed domain.
The captured Cube result changed from 386 to 399 Wave extrusion moves because
more valid curve points are retained; covered-move, bridge-fragment, idempotence,
and fail-closed behavior remain covered by the regression.

A fresh Orca export and physical print are still required. The screenshots are
visual evidence of the old behavior, not proof of the new output.

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

The owner asked for smoother, cleaner fronts and more control. The configuration
then documented every setting, including obstacle propagation, pattern,
endpoint, component ordering, cleanup, extrusion, speed, fan, and iteration
safety. `component_order="nearest"` can reduce same-distance travel moves while
preserving near-to-far anchoring; the safe default remains
`component_order="support"`. The Cube regression remained unchanged.

A fresh 0.0.15 Orca export and physical print were still unverified at that
point.

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
the Cube fixture. The fixture passed with 386 Wave extrusion moves and no
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
  engine synchronization, relative-E refusal, and later no-prompt plugin-storage
  logging.
- Import-time filesystem writes were removed from all shipped plugins and
  guarded by the audit test.
