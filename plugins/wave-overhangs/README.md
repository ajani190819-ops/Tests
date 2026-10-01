# Wave Overhangs (OrcaSlicer pipeline plugin)

**Experimental.** Prints steep overhangs without supports by replacing the
overhang region with wave-propagated toolpaths: each line anchors to the one
before it and the nozzle marches into thin air one fused-plastic rung at a
time, slowly (2 mm/s) with the cooling fan forced to 100%.

This is a port of the algorithm behind
[dennisklappe/OrcaSlicer-WaveOverhangs](https://github.com/dennisklappe/OrcaSlicer-WaveOverhangs)
(a C++ fork of OrcaSlicer, algorithm by Janis A. Andersons) as a Python
slicing-pipeline plugin. Nothing here has run on a real OrcaSlicer yet — see
`docs/ORCA-PLUGIN-FACTS.md` ("Known gaps") before trusting output.

## Install

Run `Update-Orca-Plugins.bat` (repo root). It lands here:

```
<Orca data dir>\orca_plugins\WaveOverhangs\wave_overhangs_orca.py
```

## Use

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm *Wave Overhangs* is enabled and its separate
   Version column reads **0.0.7**. The plugin name deliberately stays stable
   because Orca saves it in process-preset references.
3. Process preset → Others → **Slicing Pipeline Plugin** → *Wave Overhangs*.
   After upgrading from 0.0.6 or earlier, reselect it once so Orca replaces
   the old versioned plugin-name reference.
4. Slice a part with a small overhang, then run the
   **Wave Overhangs - Check setup** capability (Plugins dialog) — it reports
   which pipeline steps actually fired.
5. Inspect the G-code preview before printing.

## How it works (two seams, one plugin)

1. **Planning** (`posSlice`, per object): reads each layer's sliced polygons,
   computes the overhang vs the layer below, grows the wavefronts, and may
   **carve** the overhang out of the slices so Orca doesn't also fill it.
2. **Export** (`psGCodePostProcess`): splices the wave moves into the
   exported G-code with wave-specific speed / fan / flow.

**Carving is gated:** carving without the splice leaves a hole in the part,
so carving enables itself only after the G-code splice has been observed
running at least once. The first slice after a fresh install never carves —
that is intended.

## Configuration

`enabled`, `apply_to`, `carve_overhang`, `overhang_tol`, `min_overhang_area`,
`line_spacing`, `line_width`, `perimeter_overlap`, `pattern`, `flow_ratio`,
`print_speed`, `travel_speed`, `fan`, `max_iterations`, `xy_offset`.

The fork's reference defaults (for comparison): spacing 0.35 mm, speed
2 mm/s, fan 100%, flow = nozzle² in mm³/mm (0.16 for a 0.4 nozzle), 1 outer
perimeter kept inside the overhang, 2 solid floor layers above.

## Planned

A standalone **post-processing script** version that runs on exported
G-code and needs no plugin system at all — see `docs/ROADMAP.md` §C.

## Dependencies

numpy>=2.0, shapely>=2.0 (declared in the PEP 723 header; Orca installs them
on first run).
