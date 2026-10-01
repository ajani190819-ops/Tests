# Changelog — Wave Overhangs

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog in its separate Version column, and running
**Wave Overhangs - Check setup** prints it with a short version history.

**This plugin is still experimental and has not completed a verified physical
print.** The owner confirmed that 0.0.11 produced visible, perimeter-conforming
waves in a reopened real Orca export; 0.0.13 still needs a fresh export and
physical print. Treat every version here as a work in progress.

Dates are the day the change was made, not a release date.

## 0.0.15 — 2026-10-01

* Added fully documented configuration controls for propagation mode, front
  pattern, endpoint policy, component order, spacing, cleanup, extrusion, fan,
  speed, and iteration safety.
* Added `component_order="nearest"` for shorter same-distance travel moves
  around holes while preserving near-to-far mechanical anchoring.
* Kept `auto` propagation and `support` component order as safe defaults. The
  captured Cube result remains unchanged; a fresh 0.0.15 Orca export and
  physical print are still required.

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
  The captured fixture now emits 386 cleaned Wave extrusion moves and removes
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
