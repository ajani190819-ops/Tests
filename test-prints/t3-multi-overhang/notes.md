# t3 — multiple overhang layers, many holes

Uploaded 2026-10-02: `t3.stl` and `t3_20m37s.gcode` (1.9 MB, OrcaSlicer
2.5.0-dev, 0.2 mm layers, 0.57 mm wave line).

The owner reported that Wave Overhangs did nothing on this part while
simpler models worked.

## The plugin handles this file (measured 2026-10-02, v0.0.42)

Running the real capability against this exact export:

| | |
| --- | --- |
| wave layers | 3 |
| sections replaced | 7 |
| original bridge moves removed | 688 |
| fragments retained | 259 |
| overhanging wall moves reordered | 104 |
| time | 20.0 s |

So the geometry, the hole count and the multiple overhang layers are all
handled. **The earlier timeout theory does not explain this file**: 0.0.41
with its flat 30 s budget also completes it, in 19.9 s.

## What the file itself says

Line 1 is `; unlayered-infill v0.4.8 (non-planar sparse infill)` and there
is no `; wave-overhangs v...` stamp anywhere. Unlayered Infill ran on this
export; Wave Overhangs never did.

`Others -> Slicing Pipeline Plugin` is ONE preset field
(`docs/ORCA-PLUGIN-FACTS.md`, "The preset field"), and both plugins want it.
Whether it holds a list is documented but **not confirmed on a real build**.
If it is effectively single-valued, selecting Unlayered Infill means Wave is
never called -- which is exactly the evidence here.

**To confirm:** the first line of an export tells you which plugins ran.
Wave's Check setup now says the same thing when it has never been handed a
file.

## Still open

20 s on a 1.9 MB file is close enough to the old 30 s limit that a slower
machine or a larger part would have tripped it, so `time_budget: auto`
(0.0.42) still matters -- it just is not what happened here.


## Which sections should be waved (measured 2026-10-02, drove 0.0.44)

"Reach" is the distance from solid material to the furthest point of the
unsupported patch; a straight bridge has to cross twice that.

| Z | type | area | reach | verdict |
| --- | --- | --- | --- | --- |
| 5.4 | Bridge | 1035 mm2 | 37.5 mm | genuine thin air -- wave it |
| 7.8 | Bridge | 18-54 mm2 | 2.4-3.7 mm | the owner's "divots" |
| 8.1 | Bridge | 1.3 mm2 | 0.2-0.4 mm | specks |
| 9.3 | Internal Bridge | 1090 mm2 | 5.5 mm | solid over sparse infill |

`3tt_20m37s.gcode` is the processed export that showed the problem: 8 wave
blocks, one of them inside `;TYPE:Internal Bridge` at Z 9.3.


## Why the waves looked jagged on curved walls (drove 0.0.45)

`3t2_20m37s.gcode` + `jagged-curve-waves.png`. Turn angle at each wave
vertex, measured on that export:

| | before 0.0.45 | after |
| --- | --- | --- |
| median turn | 15.1 deg | 13.7 |
| 90th percentile | **90.3 deg** | 21.7 |
| vertices over 20 deg | 38% | 13% |
| points | 1770 | 3456 |

A median of 15 degrees is a smooth curve. A 90th percentile of 90 is
hairpins -- the front folding back on itself where the field rejoins behind
an obstacle. It is not simplification faceting, which is what it looks
like.

Measured and ruled out first: sweeping `simplify_tolerance` (almost no
effect on turn angle), and morphologically closing the support footprint at
r = 0.3, 0.6, 1.0, 1.5 mm (moved the median by less than a degree).
