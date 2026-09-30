# Roadmap — what's planned, what's in flight, what's blocked

Last updated: 2026-09-30. Keep this file current: **a plan that isn't written
down here doesn't exist.**

---

## Status at a glance

| # | Work | Status |
|---|------|--------|
| A | One-click updater (`Update-Orca-Plugins.bat`) + catalogue + contract test | **Done**, in PR #1 |
| B | Repo reorganization (plugins/ + tools/ + tests/ + docs/ + AGENTS.md) | **Done**, in PR #1 |
| C | Wave Overhangs as a standalone **post-processing script** | Planned — design below, **2 open questions** |
| D | Unlayered Infill: make it actually work | Planned — **1 open question (diagnosis)** |

PR #1 (branch `arena/01a0f42b-tests`) contains everything so far. **Until it
is merged, a .bat downloaded from the repo cannot download the plugins** —
`main` has no `plugins.json` and no plugin files. Merging the PR is what makes
the updater live.

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

### Our plan (pending the two open questions below)

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
4. Decide what happens to the slicer's own extrusions inside the overhang
   region (see open question C2).
5. Ship as `plugins/wave-overhangs/wave_overhangs_post.py`: double-click
   window mode, CLI mode, and `--inplace` for Orca's *Post-processing
   scripts* setting so it can run automatically on every export.

Dependency: shapely (+ numpy). The script will check for it and print a
plain-English `pip install shapely` instruction if missing (a standalone
script can't lean on Orca's dependency installer).

### Open questions

* **C1 — do the pipeline-plugin versions stay?**
  (a) keep both forms sharing one engine [recommended: the plugin auto-runs
  when it works, the script always works], (b) standalone only, retire the
  plugin, (c) plugin only.
* **C2 — replace or reinforce?** The fork *replaces* the slicer's infill
  inside the wave region (no double material; needs careful G-code surgery).
  The alternative is adding waves on top of whatever the slicer printed
  (safer surgery, double material in the overhang, likely blobs).
  Recommended: replace, matching the fork; offer `--reinforce` as a mode.

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

### Plan (pending diagnosis)

1. **Get the symptom.** What actually happened when the plugin was tried:
   never showed up in Orca / ran but the G-code looked unchanged / something
   else / never actually tried?
2. **Ship a standalone script** `plugins/unlayered-infill/unlayered_infill_post.py`
   wrapping the existing engine with the reference tool's proven UX:
   double-click window, CLI, `--inplace`, never overwrite input, refuse
   absolute E, report what it did. This works on any Orca version regardless
   of the plugin system, and is the fastest path to "it actually works".
3. **Fold the learnings back** into the plugin version (same engine, so any
   engine fix lands in both).

### Open question

* **D1 — the symptom** (see above), and same form-factor question as C1.

---

## Parked / non-goals

* Support Fins — superseded by an official cloud plugin; source stays in
  `ajani190819-ops/support-fins`. Do not resurrect here.
* `keyboard-lighting/` — stored as-is, not part of the Orca work.
* Anything requiring a private repo or authenticated downloads.
