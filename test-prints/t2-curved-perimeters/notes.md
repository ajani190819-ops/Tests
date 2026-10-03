# t2 — curved perimeters

Uploaded 2026-10-02. **This export is already processed** by Wave Overhangs
v0.0.39, so it is evidence of a result, not an input: the original bridge
moves it replaced are gone, and re-running the plugin on it is a no-op
because of the build stamp. A clean export of the same model (plugin off)
would let the fix be iterated against it -- worth adding.

* Two wave layers: **Z 5.4** and **Z 9.3**.
* 0.63 mm wave line width, bridge speed 20 mm/s.
* `jagged-wall-snapping.png` is the owner's screenshot: wave ends against
  the curved outer wall look scalloped rather than landing on one clean line.

## What has been measured so far (2026-10-02)

Layer **Z 5.4** — the snapping is behaving:

| | |
| --- | --- |
| wave ends that land on a wall | 368 of 368 |
| distance from the wall centreline | 0.156–0.174 mm (median 0.157) |
| spacing between neighbouring ends, rounded corner | 0.479 mm, sd 0.513 |
| spacing between neighbouring ends, flat side | 0.533 mm, sd 0.413 |

0.157 mm is exactly `wall_overlap` (0.25) x the 0.63 mm line, so the ends are
sitting where they are told to, and the rounded corner is not measurably
worse than the straight sides.

Layer **Z 9.3** — this one is NOT fine:

| | |
| --- | --- |
| distance from the wall | 0.77 mm min, 3.35 mm median, 8.76 mm max |
| ends within 0.3 mm of a wall | 0% |

Every front on that layer stops well short of the wall. That is a different
failure from the one in the screenshot and is the better lead.

**Open question:** which layer is the screenshot from? If it is 9.3 the
cause is whatever stops the fronts reaching the wall there; if it is 5.4 the
jaggedness is something the numbers above do not capture and needs a render.
