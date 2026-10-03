# t2 / 3t2 — curved perimeters

* `clean.gcode` — exported with Wave Overhangs OFF. Re-runnable, so fixes
  can be measured against it.
* `waved-0.0.49.gcode` — the owner's own output, stamped
  `; wave-overhangs v0.0.49`. **This is CURRENT build output**, not old.
* `jagged-wall-snapping.png` — the jagged curve.

0.3 mm layers, 0.63 mm wave line.

## Correction (2026-10-02)

An earlier note here claimed the jagged screenshots were 0.0.39 output and
therefore predated the point-density, bridge-selection and crease-rounding
fixes. **That was wrong** -- the file was mislabelled from an earlier
upload. The jaggedness is present in 0.0.49.

## The open lead

Their 0.0.49 output and a re-run of `clean.gcode` on the same build are NOT
the same:

| | their output | my re-run |
| --- | --- | --- |
| fronts | 189 | 189 |
| median turn at a vertex | 16.1 deg | 13.8 deg |
| 90th percentile | 28.1 deg | 21.7 deg |
| worst | 119.6 deg | 86.7 deg |
| vertices turning >60 deg | **2.0%** | **0.0%** |

Same build, same input, different output: the difference has to be
CONFIGURATION. Their panel is not at this build's defaults -- most likely
`smooth_creases`, `simplify_tolerance` or `line_spacing`.

**Next step: get their actual config.** Either the JSON from the Config
tab, or add the resolved config to what Check setup prints so it comes back
with the next report. Reproducing their numbers from `clean.gcode` is the
prerequisite for fixing what they see; measuring my own defaults has twice
now produced "nothing wrong here" while they were looking at something
real.

## What was measured, and still holds

On `clean.gcode` with this build's defaults: median wall-to-nearest-wave
distance 0.157 mm (= `wall_overlap` x line width), and all 22 wall
stretches more than a line width from a wave lie OUTSIDE the overhang
region. So the ends are not the problem; the front SHAPE is.
