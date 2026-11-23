# Undahang

Current version: **0.0.53**. Install with [Orca-Plugins.bat](../../Orca-Plugins.bat), then choose **Wave Overhangs** in **Process → Others → Slicing Pipeline Plugin**. The Plugins dialog Version column reads **0.0.53**.

**Wave Overhangs** remains the package/selector identity inside Orca and in
saved presets. Undahang is the public brand; its description, setup report
and logs use that name. The Python module, plugin folder, settings backups,
capability names and `; wave-overhangs v...` G-code stamps stay unchanged, so
existing selections, output recognition and repeat-export idempotence survive.
Unlayered Infill remains independent. Do not rename or reselect capabilities
just to match the brand.

This is experimental: inspect exported G-code and test a small part before a long print. Run **Wave Overhangs - Settings guide & check** after installing.

Detailed configuration, limitations, troubleshooting, and print history are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).

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
