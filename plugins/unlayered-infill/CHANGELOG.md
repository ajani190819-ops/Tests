# Changelog — Unlayered Infill

The Plugins → Config page and Check setup report carry the three newest releases. Orca's native Changelog tab is cloud-backed and is not populated by these local .py installs. Full version history is preserved below and in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).

## 0.4.13 — 2026-11-16

* Fixes Save plugin settings in Orca's sandbox: a direct button click sends
  the config instead of waiting for a form-submit event the host blocks.
* Applies to both existing capability Config pages and global/preset scopes.
  Keeps typed/unknown values, validation, read-only checks and host acknowledgments;
  preset overrides still require saving the preset in Orca.
* Adds actual-click regressions and browser checks using the pinned host
  JavaScript bridge. Browser round trips use a simulated native store, not
  native Orca disk-persistence or physical-print acceptance. Engine behavior,
  tuning defaults, setup permissions and identities stay unchanged.

## 0.4.12 — 2026-11-16

* Adds Changelog — recent changes directly to Plugins → Config, with the
  installed version and a jump link on both existing capability pages.
* Displays the newest three entries from this changelog as read-only text,
  including without a configuration bridge. Check setup notes and the
  Description's current-release summary stay synchronized from the same file.
* Makes the local .py/cloud-backed native Changelog-tab limitation explicit.
  No infill engine, tuning-default, preset or capability-identity changes;
  automated source/DOM coverage is not a native Orca visibility test.

## 0.4.11 — 2026-11-16

* Adds a grouped settings form with every tuning control visible, units,
  inline guidance, explicit global/preset scope and a value-preserving JSON fallback.
* Check setup offers an exact, confirmed process-only arc-fitting diff when
  needed. Relative E remains printer-profile advice only; no printer/filament
  edits, selection rewiring or automatic preset saves.
* Keeps engines, tuning defaults, migration/recovery and slicing callbacks
  unchanged; setup/DOM tests are not native Orca or physical-print validation.

## 0.4.10 — 2026-10-20

* Fixes settings recovery when Orca clears the Config panel to `{}` after
  an update, makes setting `restore_backup: true` take effect on the very
  next slice without restarting OrcaSlicer, and sharpens the nozzle
  clearance check.
* Standalone CLI now accepts `"auto"` on `-f`, `--angle`, and `--max-lift`,
  and G-code rewrites replace files atomically via a temporary file so an
  interrupted write cannot leave a 0-byte slice.
* Clarifies in the Config panel note that `nozzle_clearance = "auto"` allows
  `1.25x` layer height of key-in depth before warning (matching the code).

## 0.4.9 — 2026-10-02

* Settings now come back by themselves after an update. If the Config
  panel reappears at factory defaults and this plugin remembers values you
  had set under an earlier version, they are put back on the next slice
  and the log says what was restored. New `auto_restore_settings` (true).

**Why it needed more than the manual switch added in 0.4.5/0.0.37.** That
switch worked, but only for someone who knew it existed -- which is no use
when the symptom is "my settings are gone".

The rule is deliberately narrow, so it can never fight the Config tab's
own **Restore defaults** button:

* the saved config must be pristine -- every value at this build's
  default, which is what a wipe looks like;
* the newest remembered snapshot holding non-default values must come from
  a DIFFERENT build than the one running.

Press Restore defaults without updating and the newest snapshot is from
the running build, so nothing happens and the button means what it says.
Update, and the snapshot is from the older build, so your values return.
Tested in `tests/test_plugin_runtime.py` as all three cases: restore after
a version change, Restore defaults sticking inside one version, and the
switch turning it off.

## 0.4.8 — 2026-10-02

* New: a nozzle-clearance check. This plugin prints one layer at several Z
  heights, so a later move can pass underneath material already laid down.
  The finished file is now walked in print order, remembering the highest
  material in each small XY cell, and anything that passes below it is
  reported with the exact place and depth.
* collision_check: warn (default) | refuse | off, and nozzle_clearance:
  "auto" or millimetres.

**Why it is measured rather than reasoned about.** Within one wave the
displacement is a function of position, so two moves crossing at the same XY
always agree on Z and cannot collide. The hazard is between the waved region
and everything flat around and above it -- perimeters, solid skin, and above
all the next layer running into a crest. Walking the output catches all of
those regardless of which feature caused them.

**The threshold is calibrated, not zero.** This plugin exists to make layers
key into each other, so the nozzle grazing a crest it laid down earlier is
the feature working. "auto" is 1.25 layer heights: the shipped 200%
amplitude measures 0.23 mm of interference and stays quiet, 400% measures
0.46 mm and is reported, 800% measures 0.92 mm. A check that fires on every
print would be ignored, which is the same as not having one.

**warn is the default.** The author of a part is better placed than this
plugin to decide whether a few hundredths of interference matters on their
machine -- but they cannot decide it if nobody tells them. Set
collision_check to refuse to have the export stop instead, leaving the
G-code untouched.
