# Roadmap — OrcaSlicer plugin lane

This document is the plan, not a release promise. Current versions are Wave
Overhangs **0.0.12**, Unlayered Infill **0.3.4**, and updater **1.2.8** on the
session test branch. Package and capability names remain permanently:
`Wave Overhangs` and `Unlayered Infill`.

## Status at a glance

| Work | Status |
| --- | --- |
| One-click updater and catalogue | Built; contract-tested. Direct updater defaults to released `main`. |
| Branch chooser | Built; selects, downloads, validates, and runs the updater from exactly the selected branch. Missing branch files fail closed. Windows behavior was previously verified for the chooser flow; rerun after future batch changes. |
| Unlayered Infill | 0.3.4. Full 0.3.0 control set preserved, including percentage amplitude, frequency, segment length, nozzle-width grid, blending, and full-strength controls. Restore defaults handles stale Orca configuration. |
| Wave Overhangs | 0.0.12. Visible conforming waves were confirmed in the owner's real Orca export with 0.0.11. The 0.0.12 Z correction and edge cleanup pass the captured real-export regression. |
| Repository organization | This pass groups reference PDFs and real fixtures, removes the runtime log from source control, and reconciles the documentation. |
| Physical print | Not verified. A fresh 0.0.12 export and print are still required. |

## Wave Overhangs — current implementation

The shipped Wave plugin uses one transaction at
`psGCodePostProcess`. It does not mutate Orca's internal slice polygons and it
does not depend on a plan surviving from `posSlice`:

1. Parse exported layers, modal XY/Z, extrusion mode, widths, fan state, and
   `Bridge` / `Internal Bridge` sections.
2. Buffer the previous layer's actual extrusion to form the support footprint.
3. Buffer the bridge extrusion and find its unsupported area.
4. Grow wavefronts from supported material through that unsupported area.
5. Simplify the wave polylines so short edge chatter does not become printed
   dots.
6. Remove only original bridge centerline portions covered by generated Wave
   coverage. Re-emit substantial uncovered fragments with proportional E.
7. Restore the expected XY and fan state. Write the file only after all stages
   succeed; otherwise return the original G-code unchanged.

The plugin uses the bridge section's actual modal nozzle Z. This matters because
the supplied export has nominal `;Z:` comments that differ from the actual
printing height by the profile's `z_offset`.

The regression fixture is `tests/fixtures/Cube^2_3m53s.gcode`; the model is
`tests/fixtures/Cube^2.STL`. The test currently verifies three Wave layers,
actual offset Z values, bounded bridge replacement, 25 substantial retained
fragments, edge cleanup, fail-closed behavior, fan restoration, and exact
second-pass idempotence.

### Wave follow-up

1. Install the session branch and export the model again in Orca.
2. Reopen the exported G-code and compare each Wave block's Z with nearby
   original bridge moves.
3. Inspect the outer edge for isolated dots and gaps.
4. Print a small test before changing the geometry thresholds again.
5. If the fresh export differs from the fixture, save the new G-code and
   `Downloads\\orca-plugins.log` so the fixture and parser can be updated from
   evidence rather than guesses.

No standalone Wave post-processing script is currently shipped. Do not claim
that it is installed or tested; the standalone form can be considered later
only if the owner asks for it.

## Unlayered Infill — maintenance plan

Unlayered has two front ends sharing one engine:

- `plugins/unlayered-infill/unlayered_infill_post.py` is the readable standalone
  source and command-line/window tool.
- `plugins/unlayered-infill/unlayered_infill_orca.py` contains the generated
  escaped copy used by Orca.

Edit the standalone engine, run `python3 tools/sync_engine.py`, and run
`--check`. Keep relative-E refusal, idempotence, input-preserving defaults,
Downloads logging, and the complete control set. The owner's preserved Orca
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
- **Still open:** fresh 0.0.12 export, physical print quality, and any future
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
