# Changelog — Wave Overhangs Geometry

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
