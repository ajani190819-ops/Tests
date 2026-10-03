# t3 — multiple overhang layers, many holes

Uploaded 2026-10-02. **STL only, no G-code yet.** The owner reported that
this part came back completely unprocessed -- "the whole system just didn't
work".

Export it with Wave Overhangs **off** and drop the .gcode here. Without it
the diagnosis below is a reconstruction, not a measurement of this part.

## Diagnosis so far (0.0.42)

The most likely cause is the **time budget**, which was a flat 30 s. The
pass costs what the geometry costs, and when it runs out the file is
returned exactly as Orca wrote it -- correct behaviour that is impossible to
tell apart from the plugin not running.

Measured on a synthetic 40 mm block with round holes over three overhang
layers:

| holes | 0.0.41 | 0.0.42 |
| --- | --- | --- |
| 1 | 0.6 s | 0.7 s |
| 16 | 2.1 s | 1.6 s |
| 36 | 7.4 s | 4.0 s |

0.0.42 makes the budget `auto` (30 s + 45 s per MB, capped at 300 s), caches
`_interior_voids()` per region instead of recomputing it per endpoint, and
makes a timeout shout in Check setup.

**To confirm on the real part:** run Check setup after a slice. If it leads
with `*** THE LAST EXPORT RAN OUT OF TIME ***`, this was it.
