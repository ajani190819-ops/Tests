# t2 — curved perimeters

* `clean.gcode` — exported with Wave Overhangs OFF. **This is the useful
  one**: it can be re-run, so fixes can be measured against it.
* `waved-0.0.39.gcode` — the same part through Wave 0.0.39, which is what
  the screenshots show.
* `jagged-wall-snapping.png`, and see also the curved-arc screenshot in
  `../t3-multi-overhang/`.

0.3 mm layers, 0.63 mm wave line.

## Measured on clean.gcode (2026-10-02, v0.0.50)

| | |
| --- | --- |
| wave fronts on the waved layer | 189 |
| ends within 0.3 mm of a wall | 98% |
| median end distance | 0.157 mm (= wall_overlap x line width) |
| median wall-to-nearest-wave distance | 0.157 mm |
| wall more than one line width from a wave | 5% of its length |
| **those stretches that lie inside the overhang** | **0 of 22** |

So the waves reach the wall everywhere they are supposed to. The worst
"gap", 3.81 mm at X91.2 Y97.2, is not overhang at all.

**The jagged appearance is 0.0.39 output**, which predates the point
density fix (0.0.40), internal bridges no longer being waved (0.0.44) and
crease rounding (0.0.45 — 90th-percentile turn 90 degrees to 22). Re-slice
on the current build before chasing this further.
