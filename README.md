# Tests

Public source repository for the OrcaSlicer plugin lane and the companion Windows tools. GitHub delivery remains intact; runtime files, installers, plugin metadata, tests, and current release notes stay here.

## Current plugin releases

| Plugin | Version | Purpose |
| --- | --- | --- |
| [Wave Overhangs](plugins/wave-overhangs/) | 0.0.51 | Replaces supported bridge extrusion with wall-bounded wave toolpaths. |
| [Unlayered Infill](plugins/unlayered-infill/) | 0.4.10 | Writes sparse infill through Z so adjacent layers interlock. |

## Install

1. Download and run [Orca-Plugins.bat](Orca-Plugins.bat) on Windows.
2. In OrcaSlicer, select the desired plugin in **Process → Others → Slicing Pipeline Plugin**.
3. Use the plugin’s **Check setup** capability before trusting a long print.

| Check | Expected value |
| --- | --- |
| File → Plugins, **Version** column | `0.0.51` |
| The updater's output | `Wave Overhangs v0.0.51` |
| **Settings guide & check**, first line | `Wave Overhangs v0.0.51 -- setup check` |

The current test-build version applies to `Unlayered Infill`; its current test-build version is `0.4.10`. A successful build reads **0.4.10** for Unlayered Infill and **0.0.51** for Wave Overhangs. Use the Version column to confirm 0.0.51 / 0.4.10.

For detailed configuration, print evidence, older releases, and app overview, see the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk) and the [Tests Project Guide](https://docs.google.com/document/d/1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ/edit?usp=drivesdk).
