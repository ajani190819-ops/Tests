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
