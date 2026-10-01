# Changelog — Wave Overhangs Geometry

## 0.1.4 — 2026-10-01

* Moves Wave output to `fill_surfaces` at `posPrepareInfill` so Orca keeps the
  original perimeter and the ribbons become bridge fill.
* Replaces only unsupported prepared fill with `stBottomBridge` Wave ribbons,
  preserving supported normal fill surfaces and handing Wave parts to Orca from
  the supported edge outward.
* Removed the plugin-built outer wall shell path from the active output; the
  overhang perimeter is now the original perimeter Orca already generated.

## 0.1.3 — 2026-10-01

* Split preview roles: the outer overhang edge is now one non-bridge wall shell,
  while only the interior Wave ribbons are bridge-classified.
* Clipped bridge-classified Waves inside that wall shell so they do not spill
  outside the visible overhang perimeter.
* Kept the support-outward ordering bias and added a guard that refuses to emit
  the wall shell by itself if no anchored Wave ribbon survives cleanup.

## 0.1.2 — 2026-10-01

* Cleaned preview output by dropping tiny clipped ribbon crumbs.
* Preserved a continuous outer overhang boundary shell so the visible wall does
  not disappear between Wave ribbons.
* Reclassified generated Wave replacement geometry as bridge surfaces and
  handed bridge pieces to Orca in supported-edge-to-outer-edge order.

## 0.1.1 — 2026-10-01

* Fixed real-Orca registration by constructing the slicing capability base
  with no Python arguments. The local fake Orca harness now enforces the same
  no-argument base-constructor contract so this failure is caught before
  install.

## 0.1.0 — 2026-10-01

* Added a separate `posSlice` prototype that changes Orca's live slice
  geometry before perimeter and infill generation.
* Added obstacle-aware fixed-spacing Wave fronts around holes and concave
  boundaries.
* Added fail-closed mutation handling and a setup check that reports the
  preview-visible ribbon behavior.
* Documented the current API boundary: existing `ExtrusionPath` objects are
  read-only, so the prototype supplies narrow Wave geometry ribbons rather than
  injecting raw extrusion paths.

This is an experimental geometry-stage prototype. A fresh real-Orca slice and
physical print are required before it can replace the post-processing plugin.
