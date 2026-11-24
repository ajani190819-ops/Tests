# Undahang

Install with [Orca-Plugins.bat](../../Orca-Plugins.bat), then choose **Wave Overhangs** in **Process → Others → Slicing Pipeline Plugin**. The Plugins dialog Version column reads **0.0.58**.

**Wave Overhangs** remains the package/selector identity inside Orca and in
saved presets. Undahang is the public brand; its description, setup report
and logs use that name. The Python module, plugin folder, settings backups,
capability names and `; wave-overhangs v...` G-code stamps stay unchanged, so
existing selections, output recognition and repeat-export idempotence survive.
Unlayered Infill remains independent. Do not rename or reselect capabilities
just to match the brand.

This is experimental: inspect exported G-code and test a small part before a long print. Run **Wave Overhangs - Settings guide & check** after installing.

Detailed configuration, limitations, troubleshooting, and print history are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).

## Print safeguards

With `wall_last=true` (the existing default), an eligible wall loop moves
whole after the layer's waves if any portion lies beyond the support tolerance.
The former >25%-of-loop test missed short unsupported portions of large loops.
Standalone `G0/G1 F…` speed changes now remain inside the same wall feature,
including transitions between Outer wall and Overhang wall. Actual movement,
E/mode changes, wipes and non-wall/layer boundaries still split features.

The owner's **0.0.53** output exposed this separate grouping gap: a continuous
14-move wall was split at speed changes, so its hanging run could not take a
balanced retract/wipe block with it and was left before the waves. Its exposed
wall length was about 49.5%; removing the 25% cutoff alone did not fix this case.
A read-only current-helper diagnostic now selects the intact 14-move feature
and preserves its deposition geometry/Z/E/feed and retract state after moving
it behind the existing waves. The old stamped upload was not reprocessed as
an untouched slicer control, changed, or published.

Relocation still refuses absolute E, unmatched retracts and overlapping edits.
Copied prime/retract plumbing must match the source entry retraction. Handoffs
retain that debt between moved features and prime at the wave resume XY,
without double priming or leaving the next deposition retracted. Unknown
firmware/tool retraction state, mixed XY/E retract plumbing, unsafe/missing
retract feeds and nonprimed or absolute-E insertion points are refused.
Layer boundaries/Z hops stay in place; original wall commands, final wipe XY,
source entry/exit feed and net E survive. Ineligible features stay in place,
not split or moved indiscriminately.

The owner requests a **maximum 0.5 (50%) part-fan duty** for a modified Ender 3.
The shipped `fan` default and missing/invalid-value fallbacks are now explicitly
**0.5**, not auto/full duty. The existing output cap is hard-coded and needs
**no successful settings save**, even with older auto/high saved values.
On an **enabled post-processing pass**, Auto and explicit Wave fan requests,
restoration and copied **unaddressed/P0 `M106`** commands are limited to
**S127/255 ≈ 49.8%**. S128 would exceed 50%. Lower speeds and off remain lower/off;
other fan channels are untouched, not assumed to be part cooling. Auto still
reads the exported bridge-fan request; a missing or high value cannot bypass
the ceiling. Saved values/profiles are not silently overwritten. Limiting also
runs when no waves are generated, the file is already waved or geometry times
out. Fan-only work does not stamp an otherwise unchanged toolpath.

Ambiguous/unsupported primary M106 syntax, or a failed write, returns an export
error rather than claiming uncapped success. Numbered/checksummed high-speed
commands are refused, not rewritten with a stale checksum. Reviewed Orca 2.5.2
post-processing source treats this error as a failed export; installed-host
error handling is not independently executed here. This is **not a firmware or
system-wide limiter**: disabled/unselected plugins, later processors, fan macros,
other channels, firmware startup/boost and control outside this pass are not
covered. Inspect the final file, not the unchanged slice Preview.

**50% PWM is NOT 24 V → 12 V conversion or protection for a 12 V fan.** Use a
correctly rated supply/control arrangement; the owner's wiring is unverified.
PLA at 205°C and Auto fan are owner-confirmed. Excess cooling is an unproven
curl hypothesis, not a diagnosis; no temperature/profile change is included.
The fresh-rewrite regressions are synthetic; the separate owner-file check
uses its exact existing wall block diagnostically, not as a fresh export.
Automated geometry/order/fan checks do not prove physical print quality.

### Automated verification

The **33 focused tests** pass, including real temporary-file exports with
config saves refused or raising, empty/legacy auto/high/low/off values and
with/without waves. These use a fake host API, not native Orca persistence.
They pass alongside the existing Cube/t3 geometry,
extrusion-total and retract/wipe regressions. The new synthetic fresh rewrite
checks the whole speed-segmented wall after all waves exactly once, unchanged
wall commands and deposition geometry/Z/E/feed/retract state, no surplus prime,
old-seam plumbing removal and idempotence. Separate tests cover multi-feature
retract handoffs and conservative refusal cases.

For the **0.0.55 wall-order increment**, all **550/550 mutation cases** were detected: the prior 525 definitions remain
unchanged, plus 25 wall-feature cases detected in their targeted tests without
fallback. Both clean **302-test** gates pass with exact source restoration and
zero skips/errors/survivors/stale entries (216.407 seconds on this host).
The exact 14-move owner-block diagnostic is read-only evidence of these helpers,
not a freshly exported 0.0.55 file, native-host trial or physical print.
`tools/check_all.py` and the full mutation fallback include the focused suite;
numpy/Shapely are required as in `requirements-dev.txt`.

## Changelog in Orca

Open **File → Plugins → Wave Overhangs → Config**, then **Changelog —
recent changes** near the top. Both existing capability Config pages show
this installed version and the three newest entries from [CHANGELOG.md](CHANGELOG.md).
These notes are read-only and need no settings save. **Settings guide & check**
includes the same notes; Description keeps a current-release summary. Orca
2.5.2's native **Changelog** tab uses cloud release metadata, not local `.py`
notes, so that separate tab may still say no changelog is available. This
limitation is not hidden by inventing metadata or modifying Orca.

## Settings and setup

The main capability's Config form shows **all existing controls in labelled
groups**, with units, inline notes, global/preset scope and a JSON fallback.
**Save plugin settings still fails per the owner** after the prior direct-click
correction. Keep Save unresolved; this release supplies the built-in fan
workaround, not a new Save claim. Global settings and preset overrides
remain separate; **save the preset in Orca** to persist an override. No preset
is automatically saved. Restore global defaults
and removing a preset override are different actions and are labelled as such.
The scope bridge does not expose the preset category: check the host title.

Run the existing setup-check Script action for active prerequisite advice.
Only a precisely confirmed process diff can be applied. **Printer and filament
profiles stay read-only in setup**, including relative E; selections are not
rewired. Only the explicit fan default/fallback changes here; geometry and
other tuning stay unchanged. Native Orca UI/exports/prints for this
increment are not claimed from source/DOM tests. See the
[settings/setup guide](../../docs/ORCA-SETTINGS-SETUP.md) for details.
