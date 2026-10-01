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
in real Orca. Version 0.0.19 is regression-tested against the captured export
with corrected Z alignment, edge cleanup, straight snap-to-boundary endpoints,
and tapered endpoint flow; it still
needs a fresh Orca export and physical-print validation.

## Install

Run `Update-Orca-Plugins.bat` (repo root). It lands here:

```
<Orca data dir>\orca_plugins\WaveOverhangs\wave_overhangs_orca.py
```

## Use

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm *Wave Overhangs* is enabled and its separate
   Version column reads **0.0.19**. The package name is permanently
   version-free.
3. Process preset → Others → **Slicing Pipeline Plugin** → *Wave Overhangs*.
4. Slice a part with a small overhang, then run the
   **Wave Overhangs - Check setup** capability (Plugins dialog) — it reports
   which pipeline steps actually fired.
5. Export the G-code, then reopen that exported file in OrcaSlicer; the normal
   slicer preview is generated before post-processing and will not show the
   Wave result.

## Log and approval prompts

Routine logs and state now default to Orca's plugin storage folder, the same
safe location used by the other plugins in this repo. That avoids approval
prompts during normal slicing/export. Run **Wave Overhangs - Check setup** to
print the exact `orca-plugins.log` path. `ORCA_PLUGIN_LOG_DIR` remains only as a
debug override; pointing it outside plugin storage can bring approval prompts
back.

## How it works (one transactional export pass)

At `psGCodePostProcess`, the plugin reads Orca's actual exported toolpaths. It
reconstructs the previous layer's support footprint and each `Bridge` or
`Internal Bridge` footprint, then propagates wavefronts through unsupported
bridge area. It removes only original extrusion geometrically covered by those
successful wave paths and re-emits every uncovered fragment. Planning,
insertion, and subtraction happen in memory before the file is written; any
failure returns the original G-code unchanged.

## Configuration

The complete configuration is exposed through `get_default_config()`:

### Detection and geometry

* `enabled`: `true`/`false`. Disable the rewrite without removing the plugin.
* `overhang_tol`: support forgiveness in millimetres. A larger value treats
  nearby material as support and creates fewer Wave areas.
* `min_overhang_area`: ignore unsupported regions smaller than this area in mm².
* `line_spacing`: distance between Wave centre lines. Smaller values make a
  denser, smoother result but add heat and print time.
* `line_width`: fallback Wave width in millimetres. The exported Bridge width
  normally overrides it.
* `perimeter_overlap`: how far the first front reaches into supported material.
  More overlap improves anchoring but can put Wave lines onto the perimeter.
* `propagation_mode`: `auto`, `obstacle`, or `legacy`.
  `auto` uses obstacle-aware propagation only when an internal hole exists.
  `obstacle` forces it for every region. `legacy` uses the older support-only
  expansion and is mainly useful for comparison; it can miss internal holes.

### Order and direction

* `pattern`: `smart`, `monotonic`, or `zigzag`.
  `smart` starts from the endpoint closer to support. `monotonic` keeps one
  global endpoint direction while still progressing near-to-far. `zigzag`
  reverses alternate fronts. None of these extrudes between disconnected
  fronts; those connections are `G0` travel.
* `start_policy`: `supported`, `consistent`, `min-x`, `max-x`, `min-y`, or
  `max-y`. `supported` is the safe default. The axis options make endpoint
  selection deterministic when both ends are similarly supported.
* `component_order`: `support` or `nearest`. `support` preserves the normal
  support-first ordering. `nearest` reorders disconnected components at the
  same Wave distance to reduce long visible travel moves around holes. It does
  not allow a farther Wave distance to print before a nearer one. For
  `monotonic`, the global direction takes priority and `support` is safest.

### Cleanup and extrusion

* `min_wave_length`: remove complete Wave fronts shorter than this length in
  millimetres. Increase it to remove dots; decrease it to preserve small tips.
* `min_wave_segment`: merge short endpoint segments in millimetres.
* `simplify_tolerance`: remove harmless boundary points. The result is checked
  against the unsupported region and its holes, so a curve cannot become a
  chord through empty space.
* `min_bridge_fragment`: minimum retained original bridge length as a multiple
  of the exported line width. This controls how much uncovered original bridge
  material remains.
* `flow_ratio`: multiplies calculated Wave extrusion volume. Keep at `1.0`
  unless you deliberately want more or less plastic.
* `edge_snap_distance`: how far an endpoint may reach to land exactly on an
  outer wall, hole, or concave detail boundary. `auto` follows the exported
  bridge width. Set to `0` to compare with older unsnapped endpoints.
* `edge_clearance`: optional centerline inset from outer walls, holes, and
  concave detail boundaries. It is `0` by default because inset endpoints can
  create visible gaps. Use it only as a comparison/debug control.
* `edge_taper_distance`: Arachne-like cleanup distance in millimetres after the
  snap-to-boundary step. Wave endpoints that touch outer walls, holes, or
  concave detail boundaries are extruded with less E near the boundary. Set to
  `0` to disable variable endpoint flow.
* `edge_taper_min_flow`: lowest endpoint flow as a fraction of normal Wave
  flow. The default keeps more than half flow at the boundary so the line still
  bonds, but it avoids full-width blobs at curved walls and holes.
* `edge_taper_segment`: optional maximum G-code move length inside the taper
  zone. It is `0` by default, meaning taper changes E on the existing straight
  Wave moves without adding tiny preview-visible endpoint segments. Set it above
  zero only if you deliberately want a finer multi-segment flow gradient.

### Speed, cooling, and safety

* `print_speed`: Wave extrusion speed in mm/s. Slow is mechanically gentle but
  gives recently printed plastic more time to remain warm.
* `travel_speed`: non-extruding reposition speed in mm/s.
* `fan`: Wave fan setting from `0.0` to `1.0`; the default `1.0` means 100%.
* `max_iterations`: safety limit on fronts in one region. It prevents geometry
  errors from creating an unbounded toolpath.

### Why a front can appear to start in an odd place

Shapely's `LineString` coordinate order supplies only the two endpoints; it
does not choose a midpoint. `smart` reverses the complete front when its other
endpoint is closer to the supported boundary, and uses a deterministic
min-X/min-Y tie break. `zigzag` intentionally reverses alternate complete
fronts. A real midpoint-looking start usually means a curved boundary was split
into separate `LineString` pieces at a corner, hole, or branch. Use `consistent`
or an explicit axis policy when inspecting that case. No policy joins separate
pieces with extrusion.

For an exported bridge section, Wave uses Orca's measured bridge width for
coverage and extrusion geometry. A Wave return or bridge fragment always uses
an explicit non-extruding travel before the next original extrusion move.
Relative-E (`M83`) stays relative. A uniform absolute-E (`M82`) bridge section
is also supported: Wave emits its temporary relative block, then restores
`M82` and the prior command value with `G92` before replaying retained material.
A mixed E-mode section is left untouched rather than risking an E jump.

The plugin also keeps disconnected Wave fronts separate. It never turns the
travel between them into an extrusion. An internal hole in the unsupported
plane is treated as an obstacle, so fronts continue around it instead of
stopping at only the supported boundary. Topology-safe cleanup falls back to
the original curved boundary if simplification would cross a hole or concave
void.

For cleaner surfaces, v0.0.19 snaps Wave endpoints back onto non-support detail
boundaries by extending the endpoint along its own Wave direction, then tapers
extrusion at the endpoint without adding extra tiny G-code moves by default.
This is intended to conform to the same visible wall and hole perimeters that
Orca's normal bridge infill uses, without leaving clearance gaps or rectangular
endpoint textures. Optional `edge_clearance` remains available for comparison,
but it is off by default. The support-side anchor boundary is excluded from
snapping, clearance, and taper, so the first Wave rung keeps full flow where it
needs to grab supported material.

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
