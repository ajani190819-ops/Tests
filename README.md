# Tests

Public source repository for the OrcaSlicer plugin lane and the companion Windows tools. GitHub delivery remains intact; runtime files, installers, plugin metadata, tests, and current release notes stay here.

## Current plugin releases

| Plugin | Version | Purpose |
| --- | --- | --- |
| [Undahang](plugins/wave-overhangs/) | 0.0.55 | Replaces supported bridge extrusion with wall-bounded wave toolpaths. |
| [Unlayered Infill](plugins/unlayered-infill/) | 0.4.11 | Writes sparse infill through Z so adjacent layers interlock. |

## Install

1. Download and run [Orca-Plugins.bat](Orca-Plugins.bat) on Windows.
2. In OrcaSlicer, select the desired plugin in **Process → Others → Slicing Pipeline Plugin**.
3. Use the plugin’s setup-check capability before trusting a long print.

Undahang keeps **Wave Overhangs** as its Orca package name and slicing
capability, and **Wave Overhangs - Settings guide & check** as its check
capability. Those are saved-preset/config identities, not an incomplete update.
Its description, setup report and Hub card use **Undahang**. Unlayered Infill
remains a separate plugin; no preset migration or re-selection is required.

### Undahang print-order and fan safeguards

Undahang now moves an eligible whole wall loop after the waves even when
only a small part overhangs. Enabled export passes limit **primary part-fan
M106** commands, including restoration, to **S127 (about 49.8%, never above
50%)**. Lower/off requests are retained; printer/filament presets are not edited.
**PWM is not 24 V → 12 V conversion or electrical protection.** Use correctly
rated fan power. Curl's cause and the owner's exact failing output remain
unconfirmed. See [scope and limits](plugins/wave-overhangs/README.md#print-safeguards).

### Settings and explicit setup

Both main capabilities now show every tuning control in labelled groups, with
units, global/preset scope and a JSON fallback. Save/restore is explicit; values
are preserved across updates. The existing setup Script actions inspect active
prerequisites and apply only an exactly confirmed process diff, leaving it
unsaved. Relative E stays manual printer advice; setup never edits printer or
filament profiles. No new inspector or toolpath experiments are included.
See [settings/setup and verification limits](docs/ORCA-SETTINGS-SETUP.md).

The Windows app names are **Weather Report** and **Prism Lighting**. Existing
EXE/data/shortcut paths stay compatible. Prism branding is applied in the Hub
only for now: its own files remain gated on a fresh owner `engine.log`.

| Check | Expected value |
| --- | --- |
| File → Plugins, **Version** column | `0.0.55` |
| The updater's output | `Wave Overhangs v0.0.55` |
| **Settings guide & check**, first line | `Undahang v0.0.55 -- setup check` |

The current test-build version applies to `Unlayered Infill`; its current test-build version is `0.4.11`. A successful build reads **0.4.11** for Unlayered Infill and **0.0.55** for Undahang. Use the Version column to confirm 0.0.55 / 0.4.11.

For detailed configuration, print evidence, older releases, and app overview, see the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk) and the [Tests Project Guide](https://docs.google.com/document/d/1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ/edit?usp=drivesdk).
