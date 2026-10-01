# Roadmap — OrcaSlicer plugin lane

This document is the plan, not a release promise. Current versions are Wave
Overhangs **0.0.15**, Wave Overhangs Geometry **0.1.3**, Unlayered Infill
**0.3.4**, and updater **1.4.0** on the
session test branch. Package and capability names remain permanently:
`Wave Overhangs`, `Wave Overhangs Geometry`, and `Unlayered Infill`.

## Status at a glance

| Work | Status |
| --- | --- |
| One-click updater and catalogue | Built; contract-tested. Direct updater defaults to released `main`. |
| Branch chooser | Built; selects, downloads, validates, and runs the updater from exactly the selected branch. Missing branch files fail closed. Windows behavior was previously verified for the chooser flow; rerun after future batch changes. |
| Unlayered Infill | 0.3.4. Full 0.3.0 control set preserved, including percentage amplitude, frequency, segment length, nozzle-width grid, blending, and full-strength controls. Restore defaults handles stale Orca configuration. |
| Wave Overhangs | 0.0.15. Visible conforming waves were confirmed in the owner's real Orca export with 0.0.11. The 0.0.15 Z correction and edge cleanup pass the captured real-export regression. |
| Wave Overhangs Geometry | 0.1.3. Separate experimental `posSlice` prototype. The owner confirmed it registers and produces much cleaner preview geometry; 0.1.3 now removes dot crumbs, keeps one outer non-bridge overhang-wall shell, clips bridge-classified Wave ribbons inside it, and orders bridge pieces from support outward. Physical output remains unverified. |
| Repository organization | This pass groups reference PDFs and real fixtures, removes the runtime log from source control, and reconciles the documentation. |
| Physical print | Not verified. A fresh 0.0.15 export and print are still required. |

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
8. Restore the expected XY, fan, and E mode/value state. Write the file only
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

## Wave Overhangs Geometry — experimental prototype

`plugins/wave-overhangs-geometry/` is a separate plugin so the proven
post-processing Wave path remains available. It runs at `posSlice`, reads the
previous layer's live slice, generates obstacle-aware Wave fronts, removes tiny
clipped preview crumbs, keeps one continuous non-bridge outer overhang-wall
shell, and writes bridge-classified Wave ribbons inside that shell into
`LayerRegion.slices` before Orca generates perimeters and infill. This is the
route intended to make Wave geometry visible in the normal preview.

Current Orca bindings expose existing `ExtrusionPath` objects read-only. The
prototype therefore creates geometry ribbons rather than injecting raw
extrusion paths. It orders those bridge surfaces from the supported edge
outward, but Orca still owns the final path order. It must not be described as
a direct toolpath injector until Orca exposes a writable extrusion collection.
If the host geometry bindings are missing or a mutation fails, the prototype
returns a recoverable error and does not continue with a partial edit.

A real current-Orca slice is required to verify preview visibility, geometry
lifetime, island handling, and the resulting physical toolpath. The synthetic
regression only proves hole-safe Wave geometry and fail-closed behavior.

## Unlayered Infill — maintenance plan

Unlayered has two front ends sharing one engine:

- `plugins/unlayered-infill/unlayered_infill_post.py` is the readable standalone
  source and command-line/window tool.
- `plugins/unlayered-infill/unlayered_infill_orca.py` contains the generated
  escaped copy used by Orca.

Edit the standalone engine, run `python3 tools/sync_engine.py`, and run
`--check`. Keep relative-E refusal, idempotence, input-preserving defaults,
no-prompt plugin-storage logging, and the complete control set. The owner's preserved Orca
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
- **Still open:** fresh 0.0.15 export, physical print quality, and any future
  changes to the Windows batch files.

## Verification commands

```bash
python3 tests/test_installer.py
python3 tests/test_post_script.py
python3 tests/test_plugin_runtime.py
python3 tests/test_plugin_audit.py
PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py
python3 tools/sync_engine.py --check
python3 tools/sync_changelog.py --check
python3 -m py_compile plugins/wave-overhangs/wave_overhangs_orca.py
```

The Wave regression needs `numpy` and `shapely`. The `.bat` files need a raw
CRLF check and the Windows run cannot be reproduced in this Linux sandbox.
