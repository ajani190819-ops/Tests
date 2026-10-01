# Wave Overhangs Geometry

**Experimental alternate plugin.** This is a separate preview-visible geometry
version of Wave Overhangs. It does not edit exported G-code.

## What it does

At OrcaSlicer's `posPrepareInfill` stage it reads the previous layer's support
footprint and the current layer's prepared fill surfaces. Reachable unsupported
fill is replaced with fixed-spacing Wave ribbons tagged as bridge fill.

The important change in v0.1.4 is that the plugin no longer rebuilds the
unsupported area as slice islands. Orca keeps the original slice and perimeter,
then the plugin changes only `LayerRegion.fill_surfaces`. The goal is for Orca's
preview to show:

* the normal/original overhang perimeter from Orca, not a plugin-built shell;
* supported fill and supported features left alone;
* unsupported interior Wave ribbons classified as bridge surfaces.

The original `Wave Overhangs` plugin remains separate and unchanged. It waits
for exported Bridge G-code and is the safer fallback while this prototype is
being tested.

## Important limitation

The current Python host bindings expose existing `ExtrusionPath` objects as
read-only. They do not provide a writable collection for injecting or reordering
raw polylines directly in Orca's toolpath graph. This plugin therefore creates
preview-visible Wave **fill surfaces**; Orca generates ordinary bridge toolpaths
from those surfaces. It is not yet a direct `ExtrusionPath` injector.

This method is intended to fix the preview-role problem where Wave ribbons were
seen as small overhang-wall islands. It still does not prove the final physical
G-code order; Orca owns final bridge/perimeter scheduling unless the exported
G-code is post-processed or Orca exposes writable toolpath ordering.

Do not use this as a physical-print release until a real Orca build confirms
that `LayerRegion.fill_surfaces.set(...)` behaves as expected for the installed
version.

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
* `line_width`: width used to make preview-visible fill ribbons.
* `overhang_tol`: support forgiveness, in millimetres.
* `min_overhang_area`: ignores tiny unsupported regions.
* `perimeter_overlap`: moves the first front toward its supported anchor.
* `min_wave_length`: ignores very short front fragments.
* `min_preview_island_area`: removes tiny clipped preview dots left at corners
  and holes.
* `simplify_tolerance`: removes harmless boundary noise.
* `max_iterations`: safety limit for propagation.

Interior Wave replacement surfaces are tagged as `stBottomBridge` when the host
exposes that type. Bridge pieces are handed to Orca from the supported edge
outward; Orca still owns final path planning until Python can write or reorder
`ExtrusionPath` objects directly.

The plugin processes each object once per slicing execution. Invalid geometry,
missing host bindings, or a failed mutation return a recoverable error and try
to restore every layer's fill surfaces already edited for that object before
returning.
