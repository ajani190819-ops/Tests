# Project changelog

## Unreleased (Prism Lighting - ASUS input contract; physical M4 unresolved)

* Preserves all eight bits of ASUS 0x5A action IDs. High-bit events such as the
  G-Helper carrier 181 (0xB5) are presses, not guessed key-up scan codes.
* Pairs zero releases only with the same raw HID device, and distinguishes
  verified zero-frame release metadata from the old ambiguous value. Existing
  key bindings/modes/preferences are retained; keyboard up cannot certify HID.
* Keeps keyboard and HID hold evidence separate: a remembered inactive VK no
  longer cancels a valid HID stream. Silent press-only gestures expire without
  granting release capability or swallowing the next genuine Custom launch,
  including either queue order and pending-learning correlation.
* Adds full-byte action, mixed-source, device-pairing and one-shot/repeat
  regressions. No speed, vendor/startup or settings-default changes. The owner
  still reports physical M4 hold failure; a Custom launch has no key-up, and
  synthetic replays do not establish the owner's emitted signals. No new Hub
  release marker is issued for this narrower correction as a held-key repair.

## 2026-11-16 (Prism Lighting - brightness rebuild and saved-state restoration)

* Rebuilds M4/down and M5/up around one exact 0..1000 native master. Custom
  commands enter once; raw events supply hold/release evidence, not synthetic
  press flags. Keeps the 5% tap, 1% down/1.5% up per 30 ms, saved modes,
  independent bindings, rails and bounded release handling.
* Removes native/engine double consumption and rounded readback echoes. Slider,
  registered hotkeys, engine Fn input and restart now share the master channel;
  a stale live file cannot replace the saved startup value.
* Restores independent Balance Colours and Steady Colours, including false
  values and all eight zone/Steady combinations. Scoped legacy-compatible
  loading and three-way saves preserve external Hub/Tray edits, unknown fields
  and custom colour slots instead of serializing stale preferences over them.
* Applies Steady before the first engine frame and bundles the matching Aura
  script in the native EXE, so an EXE-only update cannot use an old protocol.
* Uses explicit .NET null strings for atomic settings/live replacements under
  stock PowerShell 5.1; an empty backup filename is invalid on Framework.
* Adds production-function input/state regressions, deliberate-break coverage
  and isolated PowerShell adapter execution in Windows CI. Automated checks do
  not establish the owner's physical-key delivery, installed result or cause;
  owner acceptance remains open. No vendor/startup reset or forced-off defaults.

## 2026-11-16 (Prism Lighting - M4/M5 mode interaction)

* Stops native hold-repeat tokens outside tap mode: those repeats were toggling
  press-to-glide on/off or restarting burst mode instead of sustaining a hold.
* Uses the same mode guard for M4/down and M5/up, preserving learned bindings,
  real release tracking, saved mode/preferences and tap-mode pacing/safety caps.
* Adds a production-receiver red/green regression for both directions and raw
  keyboard/HID input. This proves the mode defect, not the owner's selected
  mode, physical-key delivery or original cause. No engine/ramp-rate change.

## 0.0.58 — 2026-11-16 (Undahang)

* Makes fan default/fallbacks explicitly 0.5 (50%); the existing independent
  S127 primary-fan ceiling needs no successful settings save. Lower/off
  requests, original-command/restoration limiting and wall safeguards remain.
* Records the owner's continued Save failure after 0.0.57 as unresolved.
  Adds actual file-export checks with failed config saves, not native Orca
  persistence or physical-print certification. No new Save diagnosis loop.
* Infill remains 0.4.13; no printer/filament edits, preset autosave or preview
  integration. Preview remains next; paused/parked/deferred scope is unchanged.

## 0.0.57 — 2026-11-16 (Undahang)

* Fixes Save plugin settings: Orca's allow-scripts iframe blocks form submission
  before the submit handler, so Save now uses a plain button and click handler.
* Covers all four plugin capability pages through shared UI, with actual-click
  regressions and browser reproduction using unmodified pinned Orca JavaScript.
  Native C++ persistence is simulated in browser checks, not certified here.
* Retains validation, host acknowledgments, preset manual-save boundary and
  changelogs. No toolpath/fan/default changes; preview remains next and
  paused/parked/deferred work is unchanged.

## 0.4.13 — 2026-11-16 (Unlayered Infill)

* Ships the same sandbox-compatible Save fix in its independent plugin file.
* Preserves engine behavior, configuration types/unknown keys, capabilities
  and read-only printer/filament setup; only normal release stamps advance.

## 0.0.56 — 2026-11-16 (Undahang)

* Shows the installed version and three recent changelog entries in both
  Plugins → Config capability pages, with a direct jump link and read-only text.
* Retains synchronized Check setup/Description notes; documents why local .py
  installs do not populate Orca's cloud-backed native Changelog tab.
* No print-path/settings changes. Processed preview remains next; renumbering
  paused and parked/deferred work unchanged. Native Orca visibility unverified.

## 0.4.12 — 2026-11-16 (Unlayered Infill)

* Adds the same Config-page changelog, generated from its own Markdown history.
* Keeps independent delivery, engine/defaults, preset safety and two capabilities.

## 0.0.55 — 2026-11-16 (Undahang)

* Fixes the speed-only G1 F wall-run split demonstrated by the owner's older
  0.0.53 output; this case was not solved by removing the 25% exposure cutoff.
* Keeps eligible wall features intact after waves and preserves their retract
  state, entry feed and resume handoff, with conservative unsupported-case
  refusal. Original wall deposition and net E stay unchanged.
* Retains the S127 fan ceiling, independent Infill 0.4.11, stable identities
  and paused renumbering. Preview handoff and physical acceptance remain open.

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
