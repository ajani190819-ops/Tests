# Changelog — Wave Overhangs

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog in its separate Version column, and running
**Wave Overhangs - Check setup** prints it with a short version history.

**This plugin is still experimental and has not completed a verified physical
print.** The owner confirmed that 0.0.11 produced visible, perimeter-conforming
waves in a reopened real Orca export; 0.0.27 still needs a fresh export and
physical print. Treat every version here as a work in progress.

Dates are the day the change was made, not a release date.

## 0.0.27 — 2026-10-01

One file to run, and the settings explain themselves inside OrcaSlicer.

* **There is now one file: `Orca-Plugins.bat`.** Previously there were two
  and the difference between them was never clear -- a "chooser" and an
  "updater". Now you download and double-click one thing. It shows which
  build and which OrcaSlicer folder it remembers, and pressing Enter installs
  with those. The menu also offers choosing a different version, and
  forgetting your remembered choices to start fresh.

  It updates itself, the same careful way the installer already did: it
  fetches the newest copy, checks it really is the launcher and not a 404
  page or a wifi login portal, and runs that for this run. It never
  overwrites itself while running, because Windows reads a .bat by byte
  position as it executes and a file that rewrites itself mid-run can jump
  into garbage. So a bad download can never leave you without a working
  launcher.

* **Your old files keep working.** `Choose-Orca-Plugin-Version.bat` is now a
  short forwarder that hands over to `Orca-Plugins.bat`, so existing
  shortcuts do not break. `Update-Orca-Plugins.bat` deliberately keeps its
  name and its download URL, because every copy already on someone's disk
  checks that exact address for its own updates -- renaming it would have
  stranded those copies on an old version with no warning. It is now the
  install engine underneath, and still works on its own.

* **The plugin explains its settings inside Orca.** The menu item is now
  **Wave Overhangs - Settings guide & check**. It still reports whether the
  plugin is working, and then lists every setting with a plain-English
  explanation, showing the value you actually have in force and marking
  anything you have changed away from the default. No more opening a README
  on GitHub to find out what `perimeter_overlap` does. Set `settings_guide`
  to false in that item's own settings once you know them and you get just
  the short status report.

  **Note:** renaming that menu item changes its identity in OrcaSlicer. If
  you had it selected in a process preset you may need to pick it again from
  the list once.

## 0.0.26 — 2026-10-01

Updater convenience, a latent crash fixed, and an honest non-result on wave
blending.

* **The chooser and the updater now remember what you picked.** The branch
  chooser already stored your last branch but still made you pick it from the
  menu; pressing Enter on its own now just reuses it. The updater now
  remembers the OrcaSlicer data folder it installed into and offers it the
  same way, so a repeat update is Enter, Enter. Both still show the full menu,
  so switching is exactly as easy as it was. The remembered values live in
  `%LOCALAPPDATA%\OrcaPluginUpdater\` (`branch.txt` and `datadir.txt`);
  delete them to be asked from scratch.
* **The settings panel now explains itself.** OrcaSlicer shows the plugin
  config as JSON, and JSON cannot hold comments, so every explanation written
  in the source was invisible to you -- the panel was 33 bare names with no
  hint what any of them did. Each setting now has a plain-English note
  directly above it saying what it does, what the units are, and what happens
  if you change it. Notes are the keys beginning with `_`; the plugin ignores
  them, so you can edit or delete them freely and nothing breaks.

  The note on `arc_fitting` in particular now spells out that it controls
  *Wave's own* arcs and that OrcaSlicer's arc fitting is a separate setting in
  **Print Settings > Quality > Precision > Arc fitting**, which this plugin
  does not touch.

* **A real crash in the wave propagation, fixed.** Clipping one boundary
  against another can leave a single-point line behind, and shapely's
  `linemerge` raises `GEOSException` on those. `GEOSException` is not a
  `ValueError`, so the handler that was there could not catch it, and the
  whole layer was lost — the plugin failed closed and produced no waves at
  all. This is the same fault that killed an earlier optimisation attempt.
  `wave_tracks` now retries without the crumbs, and only when the normal
  merge has already failed, so ordinary fronts are untouched.
* **`wake_blend`: new, experimental, and off by default.** When the field
  flows around a hole and rejoins behind it, the two arriving sides meet in a
  sharp V and every later front inherits the kink, leaving a hard seam
  downstream. `wake_blend` rounds that crease off, in multiples of
  `line_spacing`.

  It works on a simple round hole — the V is replaced by smooth curves. It is
  **off by default because it is not good enough yet**: on the owner's real
  part it also loses about 4% of the wave coverage (1911 mm of path down to
  1833 mm) and turns 8 tiny fragments into 40. Healing makes consecutive
  fronts partly coincide, and the "already reached" subtraction then cuts
  them into dashes. Two different fixes for that were tried and neither
  worked; the remaining idea is written up in `docs/ROADMAP.md`. Set
  `wake_blend` to 1.0 to try it; values over 1.5 are clamped because beyond
  that the closing swallows whole fronts.

  With `wake_blend` at its default of 0 the output is byte-for-byte identical
  to 0.0.25 on all five test shapes, including the owner's own export.

## 0.0.25 — 2026-10-01

The export hang is an OrcaSlicer bug, and this version stops Wave from
triggering it. The owner confirmed the failure only happens when **Arc
fitting** is switched on in OrcaSlicer — which, with 0.0.21–0.0.23's `auto`
default, is exactly when Wave wrote G2/G3 arcs into the finished file.

* **This is OrcaSlicer issue #7433**, "Post processing script results in
  corrupted gcode / crash when previewing model", opened in November 2024
  against Orca 2.2.0 and still reproducing in 2.3.2 nightly as of January
  2026. The reporter triggered it with ArcWelder, a post-processor that does
  the same thing Wave was doing: replacing straight moves with arcs. Orca
  crashes or shows corrupt G-code when it re-reads post-processed output
  containing arcs. Nothing in the plugin can fix that, so the plugin stops
  provoking it: `arc_fitting` shipped as `false` from 0.0.24 and stays that
  way. **Check setup** now prints the arc setting and warns, with the issue
  number, if arcs have been switched back on.
* The arcs themselves were audited and are not malformed. Across all test
  fixtures and the owner's own export: 118 arcs, radii 0.78–12.1 mm, sweeps
  11–149 degrees, no major arcs, no near-full circles, no impossible chords,
  no reversed directions, every one carrying positive extrusion. The problem
  is on Orca's side of the handover, not in the geometry.

Separately, a real defect found while auditing that output:

* **542 dead moves removed from the owner's export.** Coordinates are written
  to three decimals, but the emitter decided whether to write a move using
  the unrounded step length. Steps shorter than a micron were therefore
  written out as moves whose X/Y rounded to the same values as the line
  before — literal no-ops, most of them `E0.00000` as well. Their export
  carried 542 of them. The emitter now tracks the position it has actually
  written and rolls any skipped step's extrusion into the next real move, so
  the dead lines disappear without losing material. Wave moves in that export
  drop from 2,006 to 1,910 (1,464 plus 74 arcs with arcs on).
* Arc `I`/`J` offsets are now measured from the last coordinate actually
  written rather than from the unrounded geometric point, which could sit
  half a micron away. Worst-case arc radius inconsistency in the owner's
  export improves to 0.0013 mm.

## 0.0.24 — 2026-10-01

Fixes an export that never finishes. The owner reported that after updating
past 0.0.20 OrcaSlicer sat on "exporting" and then crashed about a minute
later. Two changes, both aimed at making that impossible rather than at any
one suspected cause.

* **Wave can no longer hang an export.** The whole G-code pass now runs
  against a wall-clock ceiling, `time_budget`, set to 30 seconds. If the
  pass is still going when the clock runs out it stops and hands back the
  file exactly as OrcaSlicer wrote it — not a byte changed and no Wave
  stamp, so a later run will happily try again. The result message says so
  plainly instead of quietly reporting that it found nothing. Set
  `time_budget` higher if you have a big model and the time to wait, or to
  `0` to remove the ceiling. A slow Wave is a nuisance; an export that
  never finishes is a broken printer, so this gives the feature up rather
  than ever blocking a slice.
* **Arc moves are off by default again.** `arc_fitting` now ships as
  `false` instead of `auto`. G2/G3 is the one genuinely new *kind* of
  output Wave started writing in 0.0.21, and OrcaSlicer re-parses the
  finished file for its preview and time estimate, which makes it the most
  likely suspect for an export that stalls after the plugin has run. The
  arcs themselves were re-checked and are well formed — across every test
  fixture and the owner's own export there is not a single zero-radius,
  full-circle or mismatched-endpoint arc, and they track the real wavefront
  about 3.5x more accurately than the straight moves they replace — but
  "off until proven on real hardware" is the right default for something
  that could stop a print being made at all. Set `arc_fitting` to `"auto"`
  to get the old behaviour back, where arcs follow the Arc fitting setting
  in your print profile.

Honest note: the crash could not be reproduced here. The arc fitter was
measured and is linear (under 0.1 s for an 800-point front), and a part
that overhangs on every single layer costs 0.08–0.18 s per bridge layer
with no memory growth, so neither explains a crash. The time budget is a
backstop that works whatever the real cause turns out to be. If it fires,
the log file — its path is printed by **Wave Overhangs - Check setup** —
will record `timed_out`, how far the pass got and how long it took.

## 0.0.23 — 2026-10-01

* Much less work per export. Geometry is now built only for layers that have
  a Bridge section and the layer that holds each one up. On the owner's own
  export that is 7 layers out of 134, so 95% of the shapely objects that used
  to be created were never looked at: parsing went from 1.65 s to 0.2 s. An
  export with no Bridge section anywhere now returns in about 0.01 s without
  building a single piece of geometry.
* Extrusion footprints are buffered once per line width instead of once per
  move, which on a layer with twenty thousand moves is one GEOS call instead
  of twenty thousand.
* Cleanup no longer rebuilds its guard shapes for every front. They depend
  only on the region and the tolerance, so they are built once per section.
* The log now records `seconds`, `parse_seconds`, `plan_seconds`,
  `geometry_layers` and `layers_scanned`, so a slow export can be diagnosed
  from the log instead of guessed at.
* Note on what was actually making exports slow: 0.0.20 wrote 29,374 Wave
  moves into the owner's export -- 0.89 MB, a third of the whole file --
  because of the simplification fault fixed in 0.0.21. The same input now
  produces 2,006 moves and 74 arcs in 0.07 MB, and the finished file drops
  from 2.66 MB to 1.82 MB. Everything downstream that walks those moves
  (preview, time estimate, transfer to the printer) gets that back.

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
