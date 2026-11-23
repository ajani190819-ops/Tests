# Unlayered Infill

Current version: **0.4.11**. Install with [Orca-Plugins.bat](../../Orca-Plugins.bat), enable **Use relative E distances**, then choose **Unlayered Infill** in **Process → Others → Slicing Pipeline Plugin**.

The plugin appears as *Unlayered Infill* in the Plugins dialog. Its separate
Version column currently reads **0.4.11**, and exported G-code is stamped with
`; unlayered-infill v0.4.11`.

Run **Unlayered Infill - Check setup** after installing and test a small part before a long print. Detailed settings, compatibility notes, and history are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).

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
