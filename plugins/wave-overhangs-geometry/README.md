# Wave Overhangs Geometry

**Experimental alternate plugin.** This is a separate preview-visible geometry
version of Wave Overhangs. It does not edit exported G-code.

## What it does

At OrcaSlicer's `posSlice` stage it reads the previous layer's support footprint
and the current layer's slice surfaces. Reachable unsupported regions are filled
with fixed-spacing Wave fronts. Each front is written back as a narrow polygon
ribbon, and Orca continues with its normal perimeter and infill generation.
Because the change happens before those stages, the result is intended to be
visible in Orca's normal preview.

The original `Wave Overhangs` plugin remains separate and unchanged. It waits
for exported Bridge G-code and is the safer fallback while this prototype is
being tested.

## Important limitation

The current Python host bindings expose existing `ExtrusionPath` objects as
read-only. They do not provide a writable collection for injecting a raw
polyline directly into Orca's toolpath graph. This plugin therefore creates
preview-visible Wave **geometry ribbons**; Orca generates ordinary toolpaths
from those ribbons. It is not yet a direct `ExtrusionPath` injector.

Do not use this as a physical-print release until a real Orca build confirms
that `LayerRegion.slices.set(...)` and `Layer.make_slices()` behave as expected
for the installed version.

## Install and select

Install with `Update-Orca-Plugins.bat`, restart OrcaSlicer, and select:

`Process preset -> Others -> Slicing Pipeline Plugin -> Wave Overhangs Geometry`

Run **Wave Overhangs Geometry - Check setup** from the Plugins dialog first. Do
not select both the post-processing `Wave Overhangs` capability and this
geometry capability for the same preset; they are alternatives and would apply
two different Wave strategies.

## Controls

The visible configuration defaults are conservative:

* `enabled`: master switch.
* `line_spacing`: distance between Wave fronts, in millimetres.
* `line_width`: width used to make preview-visible ribbons.
* `overhang_tol`: support forgiveness, in millimetres.
* `min_overhang_area`: ignores tiny unsupported regions.
* `perimeter_overlap`: moves the first front toward its supported anchor.
* `min_wave_length`: ignores very short front fragments.
* `min_preview_island_area`: removes tiny clipped preview dots left at corners
  and holes.
* `outer_boundary_band`: keeps the visible overhang edge as a continuous
  bridge-classified shell. `auto` follows `line_width`.
* `simplify_tolerance`: removes harmless boundary noise.
* `max_iterations`: safety limit for propagation.

Generated replacement surfaces are tagged as `stBottomBridge` when the host
exposes that type. Bridge pieces are handed to Orca from the supported edge
outward; Orca still owns final path planning until Python can write
`ExtrusionPath` objects directly.

The plugin processes each object once per slicing execution. Invalid geometry,
missing host bindings, or a failed mutation return a recoverable error and try
to restore every layer already edited for that object before returning.
