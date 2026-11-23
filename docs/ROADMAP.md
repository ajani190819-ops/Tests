# Orca plugin roadmap

Current releases: plugins: Undahang **0.0.52** and Unlayered Infill **0.4.10**, with the package names kept version-free.
Capability names, module/storage paths and G-code stamp prefixes stay stable.

| Plugin | Current state |
| --- | --- |
| Unlayered Infill | 0.4.10. Current engine and regressions retained; owner-reported good for current use. |
| Undahang | 0.0.52. Owner-reported good for current use; experimental print/safety limits still apply. |
| Wave Overhangs Geometry | Archived, not a new development target. |

## Current review - proposals only, not implementation approval

The owner asks for addition/change ideas after Orca's plugin-system updates.
This is a source/API review, not a new real-Orca callback, export or print test.
The owner's installed Orca version is unknown; do not infer it from upstream.
The existing ORCA-01/02/03 current-use approvals remain. ORCA-04 wake/seam and
ORCA-05 curved-boundary research stay **PARKED** until explicitly selected.
Further PLAN-02 optimization stays **DEFERRED**. PLAN-01 tracks this ideas review;
no extra task ID or algorithm change is approved by asking for suggestions.

### Recommended order

| Priority | Proposed change | Value and boundary |
| --- | --- | --- |
| 1 | Usable settings forms for both plugins | Units, sliders/dropdowns, simple/advanced groups, clear preset/global scope explanations, and diagrams of existing controls. Custom HTML config UI is already supported; it is unused here, not a newly invented API. Keep JSON fallback, migrations and user values. |
| 2 | Review-and-apply setup assistant | The newer staged preset API can offer an explicit process-settings diff, e.g. relative E for Infill. Commit only user-approved edits through Orca, leave them unsaved, and respect cancel/dirty-preset prompts. No silent printer/filament changes, selection rewiring or automatic save. |
| 3 | Analyze-only / before-after layer inspector | Reuse the existing Check setup Script capability for a docked or ordinary HTML report. Show actual changed/unchanged regions, skip reasons, resolved values, max lift and existing clearance estimates on a chosen exported file. A dry run must not rewrite the print file. A separate 2D layer view is feasible work to build, not a promise of native 3D preview integration or a collision-free print. |
| 4 | Opt-in adaptive experiments after visibility improves | Infill: reduce local wave amplitude where the existing clearance estimate warns, rather than only warning/refusing or requiring a global clamp. Undahang: investigate cooling/revisit-aware ordering, distinct from the existing smart/monotonic/zigzag policies. These change toolpaths and need regressions, mutations and print validation; no strength, cooling or collision-safety guarantee. |

Start with **1 + 2**, then **3**, before changing the working path generators.
Inspector statistics, clearance checking, multiple Infill patterns/shapes, and
Undahang ordering already exist in some form; the additions are better exposure,
a non-writing comparison view and optional new adaptive policies, not rebranding
those existing features as new engines. A calibration coupon/export pair can
support later comparisons without claiming an unexposed model-insertion API.

### Upstream evidence and compatibility gates

Reviewed Orca **v2.5.1** (release commit
`eef7710cb102fdb51a9bcdbd9efd61047f44774a`) and date-bounded main
`5841f9652d2fb2ab437797850829c19f3191efaf` (through 2026-11-16).
The six referenced API/demo files were byte-identical in those snapshots.

- [Preset API #16518](https://github.com/OrcaSlicer/OrcaSlicer/pull/16518), merged
  2026-10-23: staged process/printer/filament edits and Script background/async
  execution. [Preset contract](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/docs/HLSD/preset-plugin-api.md)
  preserves unsaved-edit decisions; it is not a general preset create/fork API.
- [Custom config UI](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/src/slic3r/plugin/PythonPluginInterface.hpp)
  exposes `has_config_ui()` / `get_config_ui()` with JSON fallback.
  [Dock panel demo](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/sandboxes/orca_dock_panel_plugin_any.py)
  and [host UI](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/src/slic3r/plugin/host/PluginHostUi.cpp)
  show Script-owned panels; this is not direct toolpath injection.
- [Async boundary](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/docs/HLSD/python-plugin-async-bridge.md):
  only Script capabilities gain background/async execution. SlicingPipeline hooks
  remain synchronous; UI calls from them can deadlock. Never move UI/preset
  prompts into `psGCodePostProcess`, or retain live slicing references in a panel.
- [Native infill proposal #16941](https://github.com/OrcaSlicer/OrcaSlicer/pull/16941)
  is **unmerged**, not a released compatibility promise. It advertises 2D XY
  paths for sparse/solid/top/bottom infill. That does not directly replace
  Infill's non-planar Z changes or Undahang's bridge-replacement transaction.
  Reassess if it lands; do not migrate production plugins to its proposed API.

Any selected implementation must feature-detect the installed host, retain the
**two capabilities per plugin**, stable preset identities, top-level imports,
no import-time disk I/O, local approved storage, idempotent export rewrites,
fail-closed file handling, GPL attribution and synchronized `nonplanar_core`.
Keep the two plugins independent. Do not promise changed native preview or
Print/Send behavior from an API/source review; verify the chosen host first.

The longer roadmap/history remains in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
