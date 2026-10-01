# Changelog — Wave Overhangs

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog in its separate Version column, and running
**Wave Overhangs - Check setup** prints it with a short version history.

**This plugin is still experimental and has not completed a verified physical
print.** The owner confirmed that 0.0.11 produced visible, perimeter-conforming
waves in a reopened real Orca export; 0.0.22 still needs a fresh export and
physical print. Treat every version here as a work in progress.

Dates are the day the change was made, not a release date.

## 0.0.22 — 2026-10-01

* Fixed the unfilled corner the owner photographed. A wavefront is a contour
  of equal distance from the supported edge, and the contours step outward one
  line spacing at a time, so where the far boundary runs at an angle to that
  march -- the tip of a corner -- the last contour stops short and leaves a
  small sliver with nothing in it. Measured in the owner's own export: a
  0.22 mm^2 void, 0.53 x 0.75 mm, in the corner of the plate. Wave now fills
  such a sliver with one short path down its middle. `gap_fill` (on) and
  `gap_fill_min_area` (0.05 mm^2) control it. It only ever adds material where
  there is none: a part with no sliver comes out byte for byte identical.
* A short wavefront that touches a full-length rung is now kept instead of
  discarded. Short fronts were dropped to avoid specks printed into thin air,
  but one that touches a rung already on the plate is anchored, and dropping
  it was leaving holes in exactly the places this release is about. Whether a
  front survives is measured against every rung, not the ones kept so far, so
  print order cannot change which bridge extrusion ends up covered.
* Investigated the second thing the owner circled -- a rounded wall whose Wave
  edge looks like a staircase. Measured in their export, every Wave end along
  that curve sits 0.456 to 0.457 mm from the wall: a spread of 0.001 mm, so the
  ends are already on the wall. What the preview shows is the flat end of each
  rung meeting a curve at 0.35 mm intervals. Smoothing that needs a rung laid
  *along* the wall with the others trimmed back to make room; the first attempt
  made the edge worse and was not shipped.

## 0.0.21 — 2026-10-01

* Wave can now emit **G2/G3 arc moves**. Wave runs after Orca has written the
  file, so Orca's own arc fitter never sees these toolpaths; Wave fits its own
  arcs instead. `arc_fitting` is `auto` by default, which follows the export's
  own `enable_arc_fitting` setting: switch Arc fitting on in the print profile
  and the Waves become arcs too, leave it off and nothing changes. An arc is
  only used where it is measurably closer to the real wavefront than the
  straight moves it replaces, and it is re-checked against the overhang region
  so a bowed arc cannot push into a wall or a hole.
* Wave now **reads** G2/G3 moves out of the export. With arc fitting switched
  on, a round hole's wall is exported as arcs; before this, Wave could not see
  that wall at all, which would have silently undone the 0.0.20 perimeter fix
  on exactly the parts that need it most. Both the `I J` and `R` forms are
  understood.
* Fixed a large file-size problem on parts with holes. Cleanup refused to
  simplify any front that *touched* a hole, and since 0.0.20 ends deliberately
  finish on hole walls, nearly every front fell back to its raw form: a 9.9 mm
  front was being written as 980 moves instead of 9. Touching a hole is no
  longer treated as cutting across one, and when a shortcut really would cut a
  corner, Wave retries with a tighter tolerance before giving up. On the test
  part with a hole this cut the Wave G-code from 316 KB to 9 KB.
* Added `arc_tolerance` (`auto` follows the profile's own `resolution`, capped
  at 0.05 mm).

## 0.0.20 — 2026-10-01

* Fixed the frayed Wave edges. Orca exports bridge infill as separate lines, so
  the area those lines cover has a castellated edge that also stops short of
  the perimeter. Wave was clipping its fronts to that edge, which is what made
  the ends look jagged. Wave now reads the wall moves the layer actually
  printed and squares the overhang area up against them, so fronts run from the
  supported perimeter all the way to the overhang perimeter and to any hole.
* Wave ends now finish inside the wall bead, overlapping it by 25% of the Wave
  line width by default (`wall_overlap`), so the following perimeter has a
  straight, fully bonded edge to print against instead of a sawtooth.
* Added `wall_snap` (on by default; set it to `false` to get the 0.0.19 edges
  back for comparison), `wall_reach` (how far the area may be stretched to
  reach a wall, `auto` = 1.5 line widths) and `wall_overlap`.
* Fixed wall material being measured one G-code move at a time, which left a
  hairline slit at every vertex of a curved wall. A Wave end could slip through
  one of those slits and finish on the visible surface of a hole.
* Stretching the area to the wall can never create a Wave where there was not
  one: a region still has to come from bridge extrusion Orca exported over
  unsupported space, and nothing may be placed outside the part.

## 0.0.19 — 2026-10-01

* Stopped creating default tiny endpoint subdivision moves. Endpoint taper now
  changes E on the existing straight Wave moves unless `edge_taper_segment` is
  explicitly set above zero, avoiding the rectangular/grid texture seen near
  some walls.
* Changed endpoint snapping to extend along the Wave's own endpoint direction
  until it reaches the wall or hole boundary, rather than jumping sideways to
  the nearest boundary point.
* Kept snap-to-boundary and endpoint taper as the default clean-edge behavior,
  with optional `edge_clearance` still off by default.

## 0.0.18 — 2026-10-01

* Changed the default edge cleanup from clearance/inset to snap-to-boundary.
  Wave endpoints are projected back onto nearby outer walls, holes, and concave
  detail boundaries so they conform to the perimeter instead of leaving gaps.
* Set `edge_clearance` off by default. It remains available as an explicit
  comparison/debug option, but the normal output now keeps Wave endpoints on the
  visible perimeter and uses taper to reduce endpoint blobs.
* Added `edge_snap_distance` (`auto` by default) and regressions proving that
  wall and hole endpoints snap to their perimeter while support-side anchors are
  not moved away from support.

## 0.0.17 — 2026-10-01

* Added `edge_clearance` for cleaner Wave terminations near overhang walls,
  holes, and concave detail boundaries. The emitted Wave centerline is clipped
  back from those non-support boundaries so the bead should not bleed into the
  overhang perimeter.
* Kept bridge replacement coverage based on the untrimmed cleaned Wave paths, so
  old straight Bridge fragments are still removed at the boundary instead of
  reappearing where the visible Wave bead was inset.
* `edge_clearance="auto"` follows the exported bridge width; set
  `edge_clearance=0` to compare against the previous full-length endpoint
  behavior. Endpoint flow taper remains active after the inset unless disabled
  separately.

## 0.0.16 — 2026-10-01

* Added Arachne-like endpoint flow taper for the post-processing Wave emitter.
  Wave endpoints that touch outer walls, holes, or concave detail boundaries are
  split into short moves and extruded with less E near the boundary, so they can
  finish cleaner instead of leaving full-width jagged blobs.
* Excludes the support-side anchor boundary from tapering so the first Wave rung
  keeps full flow where it needs to bite into supported material.
* Keeps the original one-pass G-code replacement, plugin-storage logging, fan
  restoration, idempotence, retained-fragment behavior, and fail-closed safety.
  Setting `edge_taper_distance=0` restores the previous 399-move no-taper Cube
  output for comparison.

## 0.0.15 — 2026-10-01

* Added fully documented configuration controls for propagation mode, front
  pattern, endpoint policy, component order, spacing, cleanup, extrusion, fan,
  speed, and iteration safety.
* Added `component_order="nearest"` for shorter same-distance travel moves
  around holes while preserving near-to-far mechanical anchoring.
* Kept `auto` propagation and `support` component order as safe defaults.
* Refined front cleanup so only short endpoint stubs are removed after curve
  simplification; interior points are retained for smooth wall and hole
  termination. Connected obstacle fronts are merged before cleanup. The
  captured Cube fixture now emits 399 smoothed Wave moves; a fresh 0.0.15
  Orca export and physical print are still required.

## 0.0.14 — 2026-10-01

* Added obstacle-aware propagation for holes entirely inside the unsupported
  plane. Fronts now continue around both sides instead of only reacting when a
  hole touches the supported boundary.
* Prevented curved Wave fronts from being simplified into straight chords
  through holes or concave voids. Topology-safe cleanup keeps the original
  boundary when a shortcut would leave the unsupported region or enter an
  interior hole.
* Added internal-hole and circular-hole regressions alongside the concave-front
  test. Disconnected components remain separate and their travel remains
  non-extruding.
* The captured Cube export keeps its 3-layer, 386-Wave-move result and the
  existing cleanup/bridge-retention measurements. A fresh 0.0.14 Orca export
  and physical print are still required.

## 0.0.13 — 2026-10-01

* Fixed the Wave-to-original-toolpath handoff. Every replaced bridge segment
  now ends with an explicit non-extruding `G0` travel to its original endpoint,
  so the next retained `G1` move cannot draw a sharp line from a Wave endpoint.
* Refined cleanup with configurable minimum front length, minimum Wave segment
  length, simplification tolerance, and retained-bridge fragment threshold.
  The captured fixture emitted 386 cleaned Wave extrusion moves and removed
  30 isolated short fronts.
* Added real pattern and endpoint settings: `smart`, `monotonic`, and `zigzag`,
  plus `supported`, `consistent`, and min/max X/Y endpoint policies. Monotonic
  keeps one direction across fronts while all inter-front travel remains
  non-extruding.
* Added safe uniform absolute-E support: the temporary relative block restores
  `M82` and the prior command value with `G92`; mixed E-mode sections fail
  closed. The fixture retains 31 substantial uncovered fragments and remains
  idempotent. A fresh 0.0.13 Orca export and physical print are still required.

## 0.0.12 — 2026-10-01

* Fixed the real-print Z alignment: Orca's `;Z:` comment is nominal and the
  owner's profile adds `z_offset = 0.25`. Waves now use the bridge extrusion's
  actual modal nozzle Z, changing the test layers from 5.4/9.6/14.4 mm to the
  correct 5.65/9.85/14.65 mm.
* Cleaned up edge dots by simplifying each conforming wavefront, merging
  sub-0.15 mm chatter, deriving Wave width from Orca's real bridge width
  (0.573 mm in the captured export), and dropping isolated uncovered remnants
  shorter than half a line width.
* On the captured 0.30 mm cube export, wave extrusion moves fall from roughly
  1,850 tiny moves to 436 clean moves. Twenty-five substantial uncovered
  fragments remain; 121 sub-nozzle remnants are removed.
* This cleanup is offline-tested against the captured export. Alignment and
  surface quality still require a fresh Orca export and physical print.

## 0.0.11 — 2026-10-01

* Replaced the unreliable two-stage Orca slice-object design with one
  transactional exported-G-code pass. It no longer depends on an internal
  Polygon constructor, `slice_z`, `print_z`, object-to-bed calibration, or an
  in-memory plan surviving between Orca callbacks.
* The pass reconstructs the preceding layer's support footprint and each
  `Bridge` / `Internal Bridge` footprint from Orca's actual extrusion moves,
  then propagates wavefronts only through unsupported bridge area.
* Original bridge extrusion is subtracted only where buffered wave paths cover
  it. Every uncovered fragment is re-emitted. Any parsing or generation error
  returns the original G-code unchanged.
* Tested against the owner's real 0.30 mm `Cube^2_3m53s.gcode`: three layers
  receive wave blocks, 112 covered bridge moves are replaced, 158 uncovered
  fragments remain, fan state is restored, and a second pass is a no-op.
* This is offline proof against the real export, not yet proof from a new Orca
  export or a physical print.

## 0.0.10 — 2026-10-01

* Fixed the two failures measured with the owner's `Cube^2.STL` at 0.30 mm:
  wave plans now use Orca's exported `print_z` instead of its offset internal
  `slice_z`, and replacement geometry is built with Orca's supported empty
  `Polygon()` plus `append(Point)` API.
* The failed 0.0.9 export planned four wave layers but matched and inserted
  none, while carving failed safely. This release is intended to make those
  four layers match and remove that constructor error.
* Unsafe early slice carving is disabled. It happened before insertion could be
  proven, so a later splice failure could have left a print hollow. This test
  release keeps the original bridge while we verify wave insertion; final
  replacement will remove only G-code bridge moves covered by inserted waves.
* Bridge replacement is still not claimed working until a new real-Orca export
  contains wave blocks and proves geometrically bounded bridge removal.

## 0.0.9 — 2026-09-30

* **The permanent package name is now simply `Wave Overhangs`.** Release
  numbers will never be placed in the package or capability names again. Read
  Orca's separate Version column for the installed release.
* This is the final naming migration. Reselect `Wave Overhangs` once after
  updating; future releases keep that exact identity.
* Replacement remains the chosen behavior. Safe same-export removal of only
  bridge extrusion covered by successfully inserted waves is still open and
  is not claimed working in this release.

## 0.0.8 — 2026-09-30

* **Restored `Wave Overhangs v0.0.6` as the permanent compatibility
  identity.** Version 0.0.7 changed the identity and could disconnect Orca's
  saved pipeline selection/configuration just as 0.3.2 did to Unlayered
  Infill. The actual release remains visible as 0.0.8 in Orca's Version
  column, Check setup, logs, G-code stamp, and updater output.
* Confirmed the desired print behavior: replacement, not reinforcement. Waves
  must be generated and inserted successfully before only the covered bridge
  extrusion is removed. A failure must leave the original bridge untouched.
* A successful first export now says plainly that the original bridge was kept
  by the safety gate and instructs you to slice/export once more. Later exports
  report that replacement carving is enabled.
* The same-export replacement surgery is still under development and is not
  claimed as working in this release.

## 0.0.7 — 2026-09-30

* **The plugin name is now permanently `Wave Overhangs`.** Orca's development
  guide says a process preset saves the plugin name as part of its full
  capability reference. Putting the version in that name could leave a preset
  pointing at yesterday's identity, so the plugin appeared installed and
  selected but was never called. The version remains visible in Orca's Version
  column, Check setup, logs, G-code stamps, and updater output.
* The PEP 723 dependency declaration and plugin structure were audited against
  the repository's OrcaSlicer Plugin Development PDF. Orca's bundled `uv`
  installer remains responsible for installing `numpy` and `shapely`; the
  plugin does not run `pip` itself.
* After updating, select Wave Overhangs once more in the process preset so Orca
  saves the stable reference.

## 0.0.6 — 2026-09-30

* **Fixed: the plugin could fail to load, showing up in the Plugins list as
  failed or disabled.** v0.0.5 wrote its first log line while OrcaSlicer was
  still loading the plugin. Orca watches file activity during loading, so that
  write could either throw a permission prompt at you mid-install or stop the
  plugin loading altogether. The first log line now waits until the plugin is
  actually used.
* A log line that cannot be written can no longer interfere with a slice under
  any circumstances.

## 0.0.5 — 2026-09-30

* **A readable log now lands in your Downloads folder** as
  `orca-plugins.log`, shared with Unlayered Infill. It records whether the
  plugin loaded, whether it was selected in your preset, and whether the
  export step ran. Set `"log": false` in the capability config to switch it
  off.

## 0.0.4 — 2026-09-30

* **The version is now part of the plugin's display name**, so the Plugins
  dialog shows exactly which build is installed.
* **Check setup prints the running version** as its first line.

## 0.0.3 — 2026-09-30

* First version published in this repository, installable with
  `Update-Orca-Plugins.bat`.
* Experimental: aims to print steep overhangs support-free by replacing the
  overhang region with wave-propagated toolpaths.
* Port of the WaveOverhangs idea from Dennis Klappe's OrcaSlicer fork.
* Requires `numpy` and `shapely`, which OrcaSlicer downloads for you the first
  time the plugin is installed. If that download fails the plugin will not
  load — fully quit and reopen OrcaSlicer to let it retry.
