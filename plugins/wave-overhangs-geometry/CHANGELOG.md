# Changelog — Wave Overhangs Geometry

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
