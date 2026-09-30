# Unlayered Infill (OrcaSlicer pipeline plugin)

**Experimental.** Rewrites sparse infill so it rides a sine wave in Z:
`dz = amplitude * scale * sin(f * x)`. Successive layers interlock instead of
stacking as clean planes, so the part is no longer only as strong as its
weakest layer boundary. The wave tapers to flat where it meets the solid
skin above or below.

Adapts Roman Tenger's NonPlanarInfill (GPL-3.0,
https://github.com/TengerTechnologies/NonPlanarInfill). The simpler
predecessor tool is kept at `tools/nonplanar-infill-tool/`.

## Install

Run `Update-Orca-Plugins.bat` (repo root). It lands here:

```
<Orca data dir>\orca_plugins\UnlayeredInfill\unlayered_infill_orca.py
```

## Use

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. **Enable "Use relative E distances"** (Printer Settings → Advanced).
   The plugin refuses absolute-E (M82) G-code rather than corrupt it.
3. Process preset → Others → **Slicing Pipeline Plugin** → *Unlayered Infill*.
4. Slice, then run **Unlayered Infill - Check setup** (Plugins dialog) — it
   reports whether the export step actually fired.

## Configuration

| Key | Default | Notes |
| --- | --- | --- |
| amplitude | -0.2 | mm, or `-150%` / `-1.5x` of layer height. Negative dips into the part, keeping the nozzle clear. |
| frequency | 1.5 | ripples per mm along X |
| segment_mm | 1.0 | move subdivision length |
| cell_mm | 0.6 | XY resolution of the solid-skin column map |
| blend_mm | 2.0 | smooths the taper across neighbouring columns |
| full_strength | false | classic taper peaks at 0.5; this reaches 1.0 mid-span |
| require_relative_e | true | refuse M82 rather than corrupt it |

## How the taper works

Solid extrusions are rasterised into XY columns `cell_mm` across. Each column
records the Z heights that are solid in that column, so each infill move is
bracketed by its own local floor and roof: `scale = min(d_above, d_below) /
span`, peaking at 0.5 mid-span (1.0 with `full_strength`), then
`dz = amplitude * scale * sin(frequency * x)`.

A single global list of solid heights — the obvious implementation, and what
the upstream script does — is wrong on any part whose skins are not flat
planes across the whole footprint (two towers of different heights: the
short one's roof pinches the tall one flat). Per-column bracketing fixes it;
`tests` pin it.

## Engine fixes vs the reference tool

The engine (inlined as `nonplanar_core`) improves on the reference tool in
six test-pinned ways: extrusion attached to the right move, no duplicated
points (no blobs), Z restored on exit, Orca's skin markers recognized, taper
can't invert above the topmost solid, and per-column bracketing.

## Planned

A standalone **post-processing script** version wrapping this same engine —
see `docs/ROADMAP.md` §D.

## Licence

GPL-3.0 (derivative of NonPlanarInfill).
