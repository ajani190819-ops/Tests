# Wave Overhangs (OrcaSlicer pipeline plugin)

**Experimental.** Prints steep overhangs without supports by replacing the
overhang region with wave-propagated toolpaths: each line anchors to the one
before it and the nozzle marches into thin air one fused-plastic rung at a
time, slowly (2 mm/s) with the cooling fan forced to 100%.

This is a port of the algorithm behind
[dennisklappe/OrcaSlicer-WaveOverhangs](https://github.com/dennisklappe/OrcaSlicer-WaveOverhangs)
(a C++ fork of OrcaSlicer, algorithm by Janis A. Andersons) as a Python
slicing-pipeline plugin. Earlier builds ran in real Orca but inserted no waves.
The owner confirmed that 0.0.11 produced visible, perimeter-conforming waves
in real Orca. Version 0.0.14 is regression-tested against the captured export
with corrected Z alignment and edge cleanup; it still needs a fresh Orca export
and physical-print validation.

## Install

Run `Update-Orca-Plugins.bat` (repo root). It lands here:

```
<Orca data dir>\orca_plugins\WaveOverhangs\wave_overhangs_orca.py
```

## Use

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm *Wave Overhangs* is enabled and its separate
   Version column reads **0.0.14**. The package name is permanently
   version-free.
3. Process preset → Others → **Slicing Pipeline Plugin** → *Wave Overhangs*.
4. Slice a part with a small overhang, then run the
   **Wave Overhangs - Check setup** capability (Plugins dialog) — it reports
   which pipeline steps actually fired.
5. Export the G-code, then reopen that exported file in OrcaSlicer; the normal
   slicer preview is generated before post-processing and will not show the
   Wave result.

## How it works (one transactional export pass)

At `psGCodePostProcess`, the plugin reads Orca's actual exported toolpaths. It
reconstructs the previous layer's support footprint and each `Bridge` or
`Internal Bridge` footprint, then propagates wavefronts through unsupported
bridge area. It removes only original extrusion geometrically covered by those
successful wave paths and re-emits every uncovered fragment. Planning,
insertion, and subtraction happen in memory before the file is written; any
failure returns the original G-code unchanged.

## Configuration

`enabled`, `overhang_tol`, `min_overhang_area`, `line_spacing`, `line_width`,
`perimeter_overlap`, `pattern`, `start_policy`, `min_wave_length`,
`min_wave_segment`, `simplify_tolerance`, `min_bridge_fragment`, `flow_ratio`,
`print_speed`, `travel_speed`, `fan`, and `max_iterations`.

* `pattern`: `smart` starts each front from the better-supported endpoint;
  `monotonic` keeps one endpoint direction for every front; `zigzag` alternates
  directions. All connections between separate fronts remain non-extruding
  `G0` travel moves, so monotonic never scratches across a warm perimeter.
  This is not a temperature guarantee: recently printed wave material may still
  be warm and soft when the next nearby front is deposited. Near-to-far order
  preserves the mechanical anchor; fan, speed, spacing, and optional cooling
  time control the thermal tradeoff.
* `start_policy`: `supported` is the safe default. `consistent` keeps one
  endpoint direction across smart fronts; `min-x`, `max-x`, `min-y`, and
  `max-y` make corner/endpoint selection deterministic when an arc's two ends
  are equally supported.

### Why a front can appear to start in an odd place

Shapely's `LineString` coordinate order supplies only the two endpoints; it
does not choose a midpoint. `smart` reverses the complete front when its other
endpoint is closer to the supported boundary, and uses a deterministic
min-X/min-Y tie break. `zigzag` intentionally reverses alternate complete
fronts. A real midpoint-looking start usually means a curved boundary was split
into separate `LineString` pieces at a corner, hole, or branch. Use `consistent`
or an explicit min/max axis policy when inspecting that case. No policy joins
separate pieces with extrusion.

* `min_wave_length` removes isolated short fronts. `min_wave_segment` merges
  short endpoint stubs. `simplify_tolerance` removes harmless boundary points,
  but the result is checked against the unsupported-region boundary so an arc
  cannot become a straight chord through a hole. Lower these only if a small
  overhang corner is being skipped.
* `min_bridge_fragment` is measured as a multiplier of the exported line width.
  It controls how much uncovered original bridge is retained.

For an exported bridge section, Wave uses Orca's measured bridge width for
coverage and extrusion geometry. A Wave return or bridge fragment always uses
an explicit non-extruding travel before the next original extrusion move.
Relative-E (`M83`) stays relative. A uniform absolute-E (`M82`) bridge section
is also supported: Wave emits its temporary relative block, then restores
`M82` and the prior command value with `G92` before replaying retained material.
A mixed E-mode section is left untouched rather than risking an E jump.

The plugin also keeps disconnected Wave fronts separate. It never turns the
travel between them into an extrusion, and topology-safe cleanup falls back to
the original curved boundary if simplification would cross a hole or concave
void.

The fork's reference defaults (for comparison): spacing 0.35 mm, speed
2 mm/s, fan 100%, flow = nozzle² in mm³/mm (0.16 for a 0.4 nozzle), 1 outer
perimeter kept inside the overhang, 2 solid floor layers above.

## Current boundary

The shipped Wave implementation is the Orca slicing-pipeline plugin. It waits
for exported `Bridge` and `Internal Bridge` sections and performs the complete
replacement in one pass; it does not rely on pre-export slice plans. A
standalone Wave post-processing script is not shipped yet, so do not describe
one as installed or tested.

## Dependencies

numpy>=2.0, shapely>=2.0 (declared in the PEP 723 header; Orca installs them
on first run).
