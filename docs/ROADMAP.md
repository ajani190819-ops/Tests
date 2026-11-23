# Orca plugin roadmap

Current releases: plugins: Undahang **0.0.53** and Unlayered Infill **0.4.11**, with the package names kept version-free.
Capability names, module/storage paths and G-code stamp prefixes stay stable.

| Plugin | Current state |
| --- | --- |
| Unlayered Infill | 0.4.11. Settings/setup increment; engine/defaults retained. Earlier current-use approval is not new native UI evidence. |
| Undahang | 0.0.53. Settings/setup increment; engine/defaults retained. Experimental print/safety limits still apply. |
| Wave Overhangs Geometry | Archived, not a new development target. |

## Approved first pass — settings and setup only

The owner explicitly selected settings/setup for both plugins, **all controls
visible** in labelled groups, and exact approved **process** diffs with no
preset autosave. A separate decision keeps **printer/filament read-only**;
relative E stays advisory because it belongs to the printer profile. No
Simple/Advanced hiding, inspector or toolpath experiments in this pass.
Implementation and compatibility details: [settings/setup](ORCA-SETTINGS-SETUP.md).

This is source/API and mocked/DOM verification, not a new real-Orca callback,
export or print test. The owner's installed Orca version is unknown; do not
infer it from upstream. Existing ORCA-01/02/03 current-use approvals remain.
ORCA-04 wake/seam and ORCA-05 curved-boundary research stay **PARKED** until
explicitly selected. Further PLAN-02 optimization stays **DEFERRED**. PLAN-01
tracks this increment; further additions require owner questions/approval.

### Sequence and retained later proposals

| Priority | Proposed change | Value and boundary |
| --- | --- | --- |
| 1 — implemented | Settings forms for both plugins | All controls visible in labelled groups, with units, typed selects, numeric/auto inputs, schematics, clear global/preset scope, explicit save/restore, JSON fallback and user-value preservation. Custom HTML config UI was already supported; it is not a new host API. |
| 2 — implemented | Review-and-apply setup assistant | Explicit Script action reads prerequisites. Infill can offer only a confirmed process `enable_arc_fitting` on → off diff for its existing linear parser. Relative E is manual printer advice; Undahang needs no universal process fix. Leave edits unsaved, honor cancel/dirty state, reject stale approval, never rewire selection. Printer/filament remain read-only. |
| 3 | Analyze-only / before-after layer inspector | Reuse the existing Check setup Script capability for a docked or ordinary HTML report. Show actual changed/unchanged regions, skip reasons, resolved values, max lift and existing clearance estimates on a chosen exported file. A dry run must not rewrite the print file. A separate 2D layer view is feasible work to build, not a promise of native 3D preview integration or a collision-free print. |
| 4 | Opt-in adaptive experiments after visibility improves | Infill: reduce local wave amplitude where the existing clearance estimate warns, rather than only warning/refusing or requiring a global clamp. Undahang: investigate cooling/revisit-aware ordering, distinct from the existing smart/monotonic/zigzag policies. These change toolpaths and need regressions, mutations and print validation; no strength, cooling or collision-safety guarantee. |

Only **1 + 2** are approved/implemented in this pass. **3 + 4 are proposals
only**, not selected for implementation. Inspector statistics, clearance
checking, multiple Infill patterns/shapes and Undahang ordering already exist
in some form; do not rebrand those controls as new engines. A later calibration
coupon/export comparison would not imply an unexposed model-insertion API.

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
