# archive/ — things that are no longer shipped

Nothing in this folder is installed by `Orca-Plugins.bat`, listed in
`plugins.json`, or kept in sync by the tools in `tools/`. It is here so the
work is not lost and can be read or revived later.

**If you are looking for the plugins you actually use, they are in
[`plugins/`](../plugins):**

* `plugins/wave-overhangs/` — Wave Overhangs
* `plugins/unlayered-infill/` — Unlayered Infill

## What is in here

| Folder | What it is |
| --- | --- |
| [`wave-overhangs-geometry/`](wave-overhangs-geometry) | the retired third plugin, last at 0.1.4 |
| [`tests/`](tests) | that plugin's regression test |
| [`test-prints/`](test-prints) | real exports, models and logs from the owner's printer, kept as evidence |

---

## `wave-overhangs-geometry/` — Wave Overhangs Geometry (archived 2026-10-02)

An experimental second Wave plugin, last at **0.1.4**. Where the shipped Wave
Overhangs plugin rewrites the exported G-code *after* slicing, this one tried
to change Orca's geometry *during* slicing, at the `posPrepareInfill` stage, so
the waves would show up in Orca's normal 3D preview instead of only in a
reopened `.gcode` file.

**Why it was archived.** It was always a prototype. It never completed a
verified real-Orca slice, let alone a physical print, so there was no evidence
it did anything useful on a real machine — and carrying it meant every release
had a third plugin to version, test, document and install. Two maintained
plugins are worth more than two maintained plugins plus an unverified one.

**What was unfinished.** Orca's current Python bindings expose existing
`ExtrusionPath` objects read-only, so the prototype could only create
bridge-tagged *fill surfaces* and let Orca generate the actual toolpaths from
them. It never controlled the final path order. Reviving it sensibly needs
Orca to expose a writable extrusion collection.

**What is kept here**

| File | What it is |
| --- | --- |
| `wave-overhangs-geometry/wave_overhangs_geometry_orca.py` | the plugin, exactly as it was at 0.1.4 |
| `wave-overhangs-geometry/README.md` | how it worked and what it did not do |
| `wave-overhangs-geometry/CHANGELOG.md` | its history, 0.1.0 to 0.1.4 |
| `tests/test_wave_geometry.py` | its regression test, still passing |

The test still runs, from the repo root:

```bash
PYTHONPATH=/tmp/wavedeps python3 archive/tests/test_wave_geometry.py
```

It needs `shapely` and skips cleanly without it. It is **not** part of the
normal verification run in `AGENTS.md`; nothing in `plugins/` depends on it.

### If you ever want it back

1. Move `archive/wave-overhangs-geometry/` back to `plugins/`.
2. Add its entry to `plugins.json` and to the hardcoded fallback plan inside
   `Update-Orca-Plugins.bat` (keep that file CRLF — hard rule 2). Note the
   first fallback row uses `>` and the rest `>>`.
3. Add it back to the `PLUGINS` lists in `tools/sync_changelog.py` and
   `tests/test_plugin_audit.py`.
4. Move the test back to `tests/` and fix the two paths at the top of it.

---

## `test-prints/` — real exports kept as evidence (archived 2026-10-02)

Actual files from the owner's printer and OrcaSlicer: two sliced parts, their
models, and a debug log. They are **not** test fixtures — nothing in `tests/`
reads them and no code depends on them. They are kept because changelog and
`MEMORY.md` entries cite measurements taken from them, and those numbers
cannot be re-checked without the files.

The most important one is `test print_19m50s.gcode`, the export behind the
0.0.28 and 0.0.29 work: it is where the 32.9 wasted minutes, the 342
feedrate-less moves and the 7.62 g total were all measured.

See [`test-prints/README.md`](test-prints/README.md) for what each file is and
what was learned from it.

The fixture the automated tests *do* use is `tests/fixtures/Cube^2_3m53s.gcode`
and it has not moved.

## Other archived items (2026-10-08 reorganisation)

- `spatial-hud-methods-1-2/` — Spatial HUD Methods 1 and 2 (green affine,
  blue mesh), with a source snapshot and the list of what was removed.
- `spatial-hud-v0-draft/` — the original yarn-era Spatial HUD draft. Superseded
  by `spatial-hud-template-26.3/`, and it does not build.
- `modpack-demo-mod/` — a small demo Fabric mod (a HUD and a key binding). No
  build or script refers to it.
- `spatial-gui-reference/` — a prebuilt jar of the separate Spatial GUI mod,
  kept for reference only. Spatial HUD's config screen shares its design.
