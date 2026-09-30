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
| D | Unlayered Infill: make it actually work | Planned — can start; **diagnostic detail still owed** |

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

* **D1 — the symptom.** Asked 2026-09-30; the owner answered **"something
  else"**. So it *was* tried, it did *not* simply fail to appear in Orca, and
  it did *not* run quietly leaving the G-code unchanged — something more
  specific went wrong, and we still need the specifics: what was on screen,
  at which point (install / restart / slice / export), and whether
  `data_dir()/log/python_*.log` holds a traceback. Ask before planning the
  bug hunt; step 2 above does not depend on the answer.
* The form-factor question is settled: **C1 applies here too — keep both the
  plugin and the standalone script, sharing one engine.**

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
