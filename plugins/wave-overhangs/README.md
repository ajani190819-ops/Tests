# Wave Overhangs (OrcaSlicer pipeline plugin)

**Experimental.** Prints steep overhangs without supports by replacing the
overhang region with wave-propagated toolpaths: each line anchors to the one
before it and the nozzle marches into thin air one fused-plastic rung at a
time, slowly (2 mm/s) with the cooling fan forced to 100%.

This is a port of the algorithm behind
[dennisklappe/OrcaSlicer-WaveOverhangs](https://github.com/dennisklappe/OrcaSlicer-WaveOverhangs)
(a C++ fork of OrcaSlicer, algorithm by Janis A. Andersons) as a Python
slicing-pipeline plugin. Earlier builds ran in real Orca but inserted no waves.
The owner confirmed that 0.0.11 produced visible, perimeter-conforming waves
in real Orca. Version 0.0.12 is regression-tested against the captured export
with corrected Z alignment and edge cleanup; it still needs a fresh Orca export
and physical-print validation.

## Install

Run `Update-Orca-Plugins.bat` (repo root). It lands here:

```
<Orca data dir>\orca_plugins\WaveOverhangs\wave_overhangs_orca.py
```

## Use

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm *Wave Overhangs* is enabled and its separate
   Version column reads **0.0.12**. The package name is permanently
   version-free.
3. Process preset → Others → **Slicing Pipeline Plugin** → *Wave Overhangs*.
4. Slice a part with a small overhang, then run the
   **Wave Overhangs - Check setup** capability (Plugins dialog) — it reports
   which pipeline steps actually fired.
5. Export the G-code, then reopen that exported file in OrcaSlicer; the normal
   slicer preview is generated before post-processing and will not show the
   Wave result.

## How it works (one transactional export pass)

At `psGCodePostProcess`, the plugin reads Orca's actual exported toolpaths. It
reconstructs the previous layer's support footprint and each `Bridge` or
`Internal Bridge` footprint, then propagates wavefronts through unsupported
bridge area. It removes only original extrusion geometrically covered by those
successful wave paths and re-emits every uncovered fragment. Planning,
insertion, and subtraction happen in memory before the file is written; any
failure returns the original G-code unchanged.

## Configuration

`enabled`, `overhang_tol`, `min_overhang_area`, `line_spacing`,
`line_width`, `perimeter_overlap`, `pattern`, `flow_ratio`, `print_speed`,
`travel_speed`, `fan`, and `max_iterations`. For an exported bridge section,
Wave uses Orca's measured bridge width for coverage and extrusion geometry.

The fork's reference defaults (for comparison): spacing 0.35 mm, speed
2 mm/s, fan 100%, flow = nozzle² in mm³/mm (0.16 for a 0.4 nozzle), 1 outer
perimeter kept inside the overhang, 2 solid floor layers above.

## Current boundary

The shipped Wave implementation is the Orca slicing-pipeline plugin. It waits
for exported `Bridge` and `Internal Bridge` sections and performs the complete
replacement in one pass; it does not rely on pre-export slice plans. A
standalone Wave post-processing script is not shipped yet, so do not describe
one as installed or tested.

## Dependencies

numpy>=2.0, shapely>=2.0 (declared in the PEP 723 header; Orca installs them
on first run).
