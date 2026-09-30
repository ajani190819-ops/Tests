# Non-Planar Infill Tool (standalone reference)

A double-click tool: pick sliced G-code, it writes `<name>_nonplanar.gcode`
next to it with wavy, interlocking infill. Works with OrcaSlicer / Bambu
Studio / PrusaSlicer G-code.

This is the tool the `plugins/unlayered-infill/` plugin was derived from.
It is kept here **verbatim** as:

1. a working, beginner-friendly fallback that runs on any slicer version,
   and
2. the reference implementation the plugin's engine fixes are measured
   against (the plugin fixes six real bugs in this code — see the plugin
   README).

Engine adapted from nonPlanarInfill.py, Copyright (c) 2025 Roman Tenger
(TenTech), GPL-3.0 — https://github.com/TengerTechnologies/NonPlanarInfill.
This file is likewise GPL-3.0.

## Use

* **Double-click** it (with Python installed) → a small window opens →
  choose your G-code → "MAKE IT WAVY".
* Or from a terminal: `python nonplanar_infill_tool.py file.gcode`
  (options: `--amplitude`, `--frequency`, `--inplace`).
* `--inplace` rewrites the file itself, which is what Orca's
  *Post-processing scripts* setting needs for automatic runs.

Your input file is never overwritten (unless you ask for `--inplace`).
G-code using absolute extrusion (M82) is refused with instructions.
