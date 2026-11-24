# Undahang

Current version: **0.0.53**. Install with [Orca-Plugins.bat](../../Orca-Plugins.bat), then choose **Wave Overhangs** in **Process → Others → Slicing Pipeline Plugin**. The Plugins dialog Version column reads **0.0.54**.

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
Relocation still refuses absolute E, unmatched retracts and overlapping edits;
it preserves layer Z, complete retract/wipe plumbing, final XY and modal feed.
Ineligible features are left in place, not split or moved indiscriminately.

The owner requests a **maximum 50% part-fan duty** for a modified Ender 3.
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
The small-overhang regression is synthetic, not the owner's failing G-code.
Automated geometry/order/fan checks do not prove physical print quality.

### Automated verification

The 25 focused print-safeguard tests pass, alongside the existing Cube/t3
geometry, extrusion-total and retract/wipe regressions. All **525/525** retained
mutation cases are detected, including all **38 new cases** in their targeted
behavior tests without fallback. Both clean **296-test** gates pass with exact
source restoration and no skips/errors/survivors (211.880 seconds on this host).
These are offline source/G-code checks, not the owner's file or a physical
print. `tools/check_all.py` and the full mutation fallback include the focused
suite; numpy/Shapely are required as in `requirements-dev.txt`.

## Settings and setup

The main capability's Config form shows **all existing controls in labelled
groups**, with units, inline notes, global/preset scope and a JSON fallback.
Saving is explicit; no preset is automatically saved. Restore global defaults
and removing a preset override are different actions and are labelled as such.
The scope bridge does not expose the preset category: check the host title.

Run the existing setup-check Script action for active prerequisite advice.
Only a precisely confirmed process diff can be applied. **Printer and filament
profiles stay read-only in setup**, including relative E; selections are not
rewired. Engines/defaults are unchanged. Native Orca UI/exports/prints for this
increment are not claimed from source/DOM tests. See the
[settings/setup guide](../../docs/ORCA-SETTINGS-SETUP.md) for details.
