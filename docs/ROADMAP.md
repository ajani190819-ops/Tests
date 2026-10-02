# Roadmap — OrcaSlicer plugin lane

This document is the plan, not a release promise. There are **two** shipped
plugins: Wave Overhangs **0.0.32** and Unlayered Infill **0.4.1**, with
updater **1.4.0**, on the session test branch. Package and capability names
remain permanently `Wave Overhangs` and `Unlayered Infill`.

Wave Overhangs Geometry was **archived on 2026-10-02** — see
[`archive/README.md`](../archive/README.md). It is not installed, not in the
catalogue, and not in the launcher. Do not add it back to a plan without the
owner asking for it.

## Finish `wake_blend` (wave rejoining behind a hole)

**Status: shipped in 0.0.26 but off by default, because it regresses real
geometry.**

The problem it addresses is real: where the field flows around a hole and
closes up behind it, the two arriving sides meet in a sharp V, and because
every later front is an offset of the same region, they all inherit the kink.
The print shows a hard seam running downstream of the hole.

The implementation is a morphological closing of the reached region
(`_heal_wake` in the embedded `wave_core`), gated on the region having gained
an interior ring, which is exactly when the two sides have met. On
`synthetic_overhang_with_hole` it works: the V becomes a smooth curve.

**Why it is off.** On `Cube_39m10s.gcode` at `wake_blend=1.0`:

| metric | blend 0 | blend 1.0 |
| --- | --- | --- |
| total wave path | 1911 mm | 1833 mm (-4%) |
| tiny fragments dropped | 8 | 40 |
| paths under 2 mm | 24 | 37 |
| runtime | 1.5 s | 3.7 s |

The fronts come out dashed. The cause is that healing snaps the notch to
nearly the same place on consecutive steps, so consecutive fronts partly
coincide, and `front.difference(previous.buffer(1e-5))` -- which exists to
stop a hole or wall boundary being emitted twice -- deletes the coincident
stretches.

**Tried and rejected:**

* Propagating the raw region and healing only the front being drawn
  (non-cumulative). Did not remove the dashes.
* Measuring the subtraction against the raw region rather than the healed
  one. Also did not remove the dashes.

**The idea not yet tried:** stop treating this as a region problem. Keep the
propagation exactly as it is now, and smooth the *polyline* instead -- detect
the crease vertex on each front (the point nearest the obstacle's downstream
medial axis), and replace a short span around it with a fillet whose radius
grows with distance past the obstacle, clipped to `unsupported`. That leaves
the region, the spacing and the subtraction untouched, so it cannot create
dashes or lose coverage; the only risk is the fillet leaving the legal area,
which the existing `_clean_wave_polyline` guards already check.

Values above 1.5 are clamped: beyond that the closing swallows whole fronts
and breaks them into stubs.

## Status at a glance

| Work | Status |
| --- | --- |
| One-click updater and catalogue | Built; contract-tested. Direct updater defaults to released `main`. |
| Branch chooser | Built; selects, downloads, validates, and runs the updater from exactly the selected branch. Missing branch files fail closed. Windows behavior was previously verified for the chooser flow; rerun after future batch changes. |
| Unlayered Infill | 0.4.1. Full control set preserved, plus the 0.4.0 wave-shaping controls (`pattern`, `wave_angle`, `shape`, `layer_phase`, `max_lift_mm`) and an in-Orca settings guide. Defaults reproduce 0.3.4 output exactly, pinned by `tests/test_unlayered_waves.py`. Not yet printed. |
| Wave Overhangs | 0.0.23. Visible conforming waves were confirmed in the owner's real Orca export with 0.0.11. Z correction, endpoint cleanup, taper, and the new wall-bounded Wave area pass the captured real-export regression: ends along each wall lie on one line within 0.02 mm, and a synthetic overhang-with-hole export puts every hole end on one radius. |
| Wave Overhangs Geometry | **Archived 2026-10-02** to `archive/`. Never completed a verified real-Orca slice or print. `archive/README.md` has the revival steps. |
| Repository organization | This pass groups reference PDFs and real fixtures, removes the runtime log from source control, and reconciles the documentation. |
| Physical print | The owner printed a part with 0.0.20 and photographed the first layer. That photograph is what drove 0.0.22. A print with 0.0.22 itself is still required. |

## Wave Overhangs — current implementation

The shipped Wave plugin uses one transaction at
`psGCodePostProcess`. It does not mutate Orca's internal slice polygons and it
does not depend on a plan surviving from `posSlice`:

1. Parse exported layers, modal XY/Z, extrusion mode, widths, fan state, and
   `Bridge` / `Internal Bridge` sections.
2. Buffer the previous layer's actual extrusion to form the support footprint.
3. Buffer the bridge extrusion and find its unsupported area.
4. Grow wavefronts from supported material through that unsupported area;
   internal holes are obstacles and fronts continue around them.
5. Simplify the wave polylines with a topology guard, merge connected obstacle
   fronts, retain the curve points, and remove only short endpoint stubs. Drop
   isolated short fronts so edge chatter does not become printed dots or chords
   through holes. Every surviving front endpoint must remain on the wall or
   hole boundary.
6. Order fronts according to the configurable `smart`, `monotonic`, or `zigzag`
   pattern and deterministic endpoint policy. Connections between fronts remain
   non-extruding travel moves.
7. Remove only original bridge centerline portions covered by generated Wave
   coverage. Re-emit substantial uncovered fragments with proportional E.
8. Snap emitted Wave endpoints back onto non-support detail boundaries by
   extending in the Wave endpoint direction, then taper E on the existing moves
   by default. Optional centerline clearance and taper micro-segmentation exist
   for comparison but are off by default; the support-side anchor boundary is
   excluded from snap, clearance, and taper.
9. Restore the expected XY, fan, and E mode/value state. Write the file only
   after all stages succeed; otherwise return the original G-code unchanged.

The plugin uses the bridge section's actual modal nozzle Z. This matters because
the supplied export has nominal `;Z:` comments that differ from the actual
printing height by the profile's `z_offset`.

The regression fixture is `tests/fixtures/Cube^2_3m53s.gcode`; the model is
`tests/fixtures/Cube^2.STL`. The test currently verifies three Wave layers,
actual offset Z values, bounded bridge replacement, 31 substantial retained
fragments, short-front cleanup, absolute-E restoration, fail-closed behavior,
fan restoration, and exact second-pass idempotence.

### Wave follow-up

1. Install the session branch and export the model again in Orca.
2. Reopen the exported G-code and compare each Wave block's Z with nearby
   original bridge moves.
3. Inspect the outer wall and circular hole perimeter for isolated dots,
   jagged endpoint branches, and gaps.
4. Print a small test before changing the geometry thresholds again.
5. If the fresh export differs from the fixture, save the new G-code and the
   plugin-storage `orca-plugins.log` path reported by Check setup so the fixture
   and parser can be updated from evidence rather than guesses.

No standalone Wave post-processing script is currently shipped. Do not claim
that it is installed or tested; the standalone form can be considered later
only if the owner asks for it.

## Wave Overhangs Geometry — archived

Moved to `archive/wave-overhangs-geometry/` on 2026-10-02 with its README,
changelog and still-passing regression test. It was an unverified prototype
and carrying it taxed every release. `archive/README.md` records what was
unfinished — chiefly that Orca's bindings expose existing `ExtrusionPath`
objects read-only, so it could only hand Orca bridge-tagged fill surfaces and
never controlled the final path order — and the exact steps to bring it back.

## Unlayered Infill — maintenance plan

Unlayered has two front ends sharing one engine:

- `plugins/unlayered-infill/unlayered_infill_post.py` is the readable standalone
  source and command-line/window tool.
- `plugins/unlayered-infill/unlayered_infill_orca.py` contains the generated
  escaped copy used by Orca.

Edit the standalone engine, run `python3 tools/sync_engine.py`, and run
`--check`. Keep relative-E refusal, idempotence, input-preserving defaults,
no-prompt plugin-storage logging, and the complete control set.

0.4.0 added `pattern`, `wave_angle`, `shape`, `layer_phase` and
`max_lift_mm`. Two rules for them. **Every one defaults to the pre-0.4.0
behaviour**, and `tests/test_unlayered_waves.py` enforces that by diffing
generated G-code rather than by reading the defaults — an update must never
change output for someone who changed no settings. **The unit wave stays
inside [-1, 1]** whatever the pattern and shape, so `amplitude` and
`max_lift_mm` keep meaning millimetres; `cross` averages its two axes for
exactly this reason rather than adding them. The owner's preserved Orca
configuration can be cleared with **Restore defaults**; never remove controls
to make an old preset look clean.

## Updater and chooser maintenance

- Keep `Update-Orca-Plugins.bat` CRLF on every line and never let it overwrite
  itself while running.
- Keep the catalogue and fallback plan synchronized. The updater reads fresh
  plugin files from its selected ref and stamps the installed version from the
  plugin header.
- Directly running the updater with no branch configuration must remain the
  released-`main` behavior.
- The chooser must run the updater from the selected branch, never silently
  combine branch files with `main`, and stop before changing Orca if a required
  file is missing or invalid.
- Run the Windows flow again whenever batch code changes. This sandbox can only
  perform static analysis and install replay.

## Documentation and evidence rules

Keep these sources in sync with the implementation:

- `README.md` — beginner-facing installation and troubleshooting.
- `CHANGELOG.md` — whole repository history.
- `plugins/*/README.md` — plugin-specific use and boundaries.
- `plugins/*/CHANGELOG.md` — release notes shown by Check setup.
- `MEMORY.md` — current handoff and session log.
- `docs/ORCA-PLUGIN-FACTS.md` — binding Orca facts; do not re-derive them.

State clearly which evidence exists:

- **Verified here:** static structure, fake-Orca runtime, audit behavior,
  installer replay, standalone engine behavior, embedded-engine synchronization,
  captured real-export Wave transformation, idempotence, and fail-closed output.
- **Confirmed by the owner:** visible Wave output in a real Orca export from the
  previous release; Unlayered real-Orca operation from earlier testing.
- **Still open:** fresh 0.0.23 export, physical print quality, and any future
  changes to the Windows batch files.

## Known open item: a Wave edge against a curved wall

Every Wave end already lands on the wall: measured on the owner's 13 mm radius
corner, 21 ends sat 0.456 to 0.457 mm from the silhouette, a spread of
0.001 mm. The edge still *reads* as a staircase because each rung ends flat
and the rungs are one line spacing apart, so a curve is met in 0.35 mm steps.

Making that edge smooth needs a rung laid along the wall, with the rungs that
land on it trimmed back by half a line width so the plastic is moved rather
than added. A first attempt (0.0.22 development) placed the rung on the
inset boundary wherever fronts landed and trimmed every front whose tip it
covered. The rung coverage was patchy, so the trimming bit scallops out of the
edge and the result was worse than doing nothing. It was reverted. A correct
attempt needs the trim driven by the rung that actually covers each tip, and a
rung that is continuous along the whole stretch of wall being hugged.

## Verification commands

```bash
python3 tests/test_installer.py
python3 tests/test_post_script.py
python3 tests/test_plugin_runtime.py
python3 tests/test_plugin_audit.py
python3 tests/test_unlayered_waves.py
PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py
PYTHONPATH=/tmp/wavedeps python3 archive/tests/test_wave_geometry.py  # archived
python3 tools/sync_engine.py --check
python3 tools/sync_changelog.py --check
python3 -m py_compile plugins/wave-overhangs/wave_overhangs_orca.py
```

The Wave regression needs `numpy` and `shapely`. The `.bat` files need a raw
CRLF check and the Windows run cannot be reproduced in this Linux sandbox.
