# Project changelog

## 0.0.54 — 2026-11-16 (Undahang)

* Fixes the demonstrated minority-overhang wall-order gap, retaining safe
  whole-feature relocation and layer/retract/wipe/position/feed handoffs.
* Applies the owner's 50% primary part-fan request ceiling, including Auto,
  original M106 commands and restoration; S127 avoids rounding above 50%.
  This is not voltage conversion, a curl diagnosis or physical validation.
* Keeps the 0.0.x series, stable identities, independent Infill 0.4.11 and
  read-only printer/filament setup. Processed-preview integration is still open.

## 0.0.53 — 2026-11-16 (Undahang)

* All existing controls are visible in labelled form groups, with units,
  scope-aware save/restore and JSON fallback. Setup stays in the Script capability.
* No engine/default/path-generation changes, native Orca test claim or app changes.

## 0.4.11 — 2026-11-16 (Unlayered Infill)

* Adds the same visible settings form and a confirmed process-only arc-fitting
  prerequisite diff. Relative E stays manual/read-only; no preset autosave.
* Preserves independent single-file plugins, existing capabilities and user values.

## 0.0.52 — 2026-11-16

* Undahang adopts public branding without renaming Orca package/capability,
  preset, Python, storage or G-code identities. Unlayered Infill is unchanged.
* Hub 2.8.70 labels Weather Report and Prism Lighting; Weather Report updates
  its title, tray and notices. No lighting files or security policy change.
* Existing legacy shortcut filenames remain valid; updates repoint only
  already-present public/legacy aliases and create no extra Desktop icons.

## 2026-10-20 — current plugin releases

Wave Overhangs **0.0.51**, Unlayered Infill **0.4.10**, Orca updater **2.2.0**.

- Both plugins recover settings safely after an update and write rewritten G-code atomically.
- Detailed historical release notes are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
