# MEMORY.md — current repository handoff

Read `AGENTS.md` first. This file is the current state, not a replacement for
that rulebook. `docs/ROADMAP.md` is the plan; `docs/ORCA-PLUGIN-FACTS.md` is the
binding record of OrcaSlicer behavior.

* **Last updated:** 2026-10-02, archive + Unlayered Infill 0.4.0 +
  launcher 1.0.1 session.
* **Repository:** `ajani190819-ops/Tests`, public.
* **Session branch:** `arena/01a0fb0f-tests`. Never switch branches or push to
  `main`. (The branch is different every session — use the one you were
  handed, not this one.)
* **Latest code state:** Wave Overhangs 0.0.25 measures the overhang against
  the layer's real wall moves, so Wave ends land on the wall and hole
  perimeters instead of the castellated bridge-line edge. It can read and
  write G2/G3 arc moves but **no longer does so by default**, and the whole
  G-code pass now runs under a 30-second `time_budget` that returns the file
  untouched rather than ever stalling an export.
* **There are now TWO shipped plugins, not three.** Wave Overhangs Geometry
  was archived on 2026-10-02 at the owner's request; see `archive/README.md`.
  It is out of `plugins.json`, the launcher's fallback plan,
  `tools/sync_changelog.py` and the plugin tests. Do not reinstate it unless
  the owner asks.
* **Current versions:** Wave Overhangs 0.0.28, Unlayered Infill 0.4.0,
  updater 1.4.0, launcher (`Orca-Plugins.bat`) 1.0.1.
* **Permanent identities:** `Wave Overhangs` and `Unlayered Infill`. Release
  numbers must remain out of package and capability names.

## One-minute orientation

This repository contains two experimental OrcaSlicer pipeline plugins, a
Windows updater, a strict branch chooser, standalone Unlayered Infill tooling,
reference documentation, and tests.

Important locations:

- `Update-Orca-Plugins.bat` — released-`main` default updater. It keeps CRLF,
  never overwrites itself while running, and installs from the selected ref.
- `Choose-Orca-Plugin-Version.bat` — branch chooser. It validates and runs the
  updater downloaded from exactly the selected branch; it never fills a test
  branch from `main`.
- `plugins.json` — catalogue and version source used by the updater.
- `plugins/wave-overhangs/` — exported-G-code Wave plugin and its release notes.
- `archive/` — not shipped. Holds the archived Wave Overhangs Geometry
  prototype, its tests, and a README saying why it was archived and how to
  revive it.
- `plugins/unlayered-infill/` — Unlayered plugin, standalone tool, and notes.
- `tests/fixtures/` — the supplied `Cube^2.STL` and captured
  `Cube^2_3m53s.gcode` real-export fixture.
- `docs/reference/` — supplied OrcaSlicer wiki/PDF snapshots used for the
  plugin contract and lifecycle facts.
- `tests/` — fake Orca harness, installer/runtime/audit tests, standalone
  engine test, the real-export Wave regression, and the geometry-stage Wave
  regression.
- `keyboard-lighting/` — unrelated personal project. Do not reorganize or
  modify it.
- `orca-plugins.log` is runtime output, not source; it is ignored and must not
  be committed.

## Current implementation

### Wave Overhangs 0.0.25

The active implementation is one transactional G-code pass at
`psGCodePostProcess`:

1. Parse actual exported layers, modal XY/Z, relative-E mode, line widths, fan
   state, and `Bridge` / `Internal Bridge` sections.
2. Build the previous layer's support footprint, the current layer's wall
   material (wall moves joined into loops first, then given their width), and
   the current bridge footprint, all from actual extrusion paths in absolute
   bed coordinates.
2b. Square the bridge footprint up against that wall material: a closing
   operation fills the narrow channel Orca leaves between its last bridge line
   and the wall, ends finish 25% of a line width inside the wall bead
   (`wall_overlap`), and nothing may be placed outside the part. The decision
   about *where* a Wave belongs is unchanged -- the area still has to come from
   exported bridge extrusion over unsupported space. `wall_snap=false`
   restores the 0.0.19 edges for comparison.
3. Generate expanding wavefronts from supported material through unsupported
   bridge area; internal holes in that plane are treated as obstacles so fronts
   continue around both sides.
4. Simplify wave polylines with a topology guard, merge connected obstacle
   fronts before cleanup, retain interior curve points, and remove only short
   endpoint stubs. Drop isolated short fronts so edge chatter does not become
   blobs. Surviving front endpoints remain on the valid overhang wall or hole
   boundary.
5. Order fronts using configurable smart, monotonic, or zigzag patterns plus
   deterministic endpoint policies. Travel between fronts stays non-extruding.
6. Remove only bridge centerline portions covered by generated Wave paths and
   re-emit substantial uncovered fragments with proportional extrusion.
7. Restore the expected XY, fan, and E mode/value state. Uniform absolute-E
   sections restore `M82` and `G92`; mixed E-mode sections remain untouched.
8. Snap Wave endpoints back onto non-support detail boundaries by extending in
   the Wave endpoint direction, then taper E on existing moves by default.
   Optional centerline clearance and taper micro-segmentation exist for
   comparison but are off by default; the support-side anchor boundary is
   excluded from snap, clearance, and taper.
9. Write only after parsing, generation, subtraction, and assembly succeed. Any
   exception returns the original G-code unchanged.

The Wave pass uses the bridge move's actual modal nozzle Z. In the supplied
fixture, the nominal `;Z:` comments differ from the actual height because the
profile contains a 0.25 mm Z offset. The 0.0.20 fixture output uses 5.650,
9.850, and 14.650 mm for the three Wave blocks.

The captured regression reports three Wave layers, 104 covered bridge moves,
28 substantial retained fragments, 115 tiny remnants removed, 16 short Wave
fronts removed, three wall-bounded sections, 488 default Wave moves (713 with
`wall_snap=false`, which still shows the 0.0.19 castellated edges), 44 arcs
replacing 180 straight moves when the profile asks for arc fitting, restored fan
state, exact second-pass idempotence, and byte-for-byte unchanged output after
deliberate generation failure. It also measures edge quality directly: Wave
ends along each wall of the Cube's overhang now lie on one line within
0.02 mm (0.0.19 varied by 0.29 mm), no end stops in the 0.001-0.30 mm "just
short of the wall" band, and a new synthetic overhang-with-hole export puts
every hole end on one radius within 0.001 mm with nothing inside the hole or
outside the part. The owner confirmed that the previous 0.0.11 output visibly
produced perimeter-conforming waves in real Orca. A fresh 0.0.20 export and
physical print remain open.

There is no standalone Wave post-processing script in this repository. The
plugin waits for exported Bridge G-code; do not claim a Wave standalone tool
is installed or tested.

### Wave Overhangs Geometry 0.1.4

A separate experimental geometry-stage plugin now runs at `posPrepareInfill`.
It reads live `LayerRegion.fill_surfaces` and the previous layer's `lslices`,
generates obstacle-aware fixed-spacing Wave fronts, converts them to narrow
preview ribbons, removes tiny clipped dot islands, and replaces only reachable
unsupported prepared fill with `stBottomBridge` Wave ribbons. It deliberately
leaves `LayerRegion.slices` alone, so Orca keeps the original overhang perimeter
instead of generating dark-blue overhang-wall loops around every Wave ribbon.
Bridge fill surfaces are handed to Orca from the supported side outward. It
keeps the post-processing Wave plugin unchanged as the fallback.

Current Orca bindings expose existing `ExtrusionPath` objects read-only, so
this is preview-visible Wave fill-surface geometry rather than direct raw path
injection or path reordering. The plugin documents that limitation, snapshots
fill surfaces, rolls back an object on mutation failure, and fails closed when
dependencies or host fill-surface bindings are unavailable. Routine logs/state
still default to plugin storage to avoid normal approval prompts. Synthetic
geometry, installer, runtime, audit, G-code, sync, compilation, whitespace, and
CRLF checks passed for 0.1.4 in the sandbox; a real Orca preview and physical
print remain unverified.

### Unlayered Infill 0.3.4

Unlayered retains the complete working control set from 0.3.0: amplitude as a
percentage of layer height, frequency, segment length, automatic nozzle-width
column grid, blending, full-strength mode, relative-E safety, and logging.
The standalone script and pipeline plugin share one engine. Edit the readable
standalone engine, run `tools/sync_engine.py`, and verify the copies match.
Preserved Orca settings can be cleared with the plugin UI's **Restore defaults**
control.

## Verified evidence

These checks have passed in the repository:

- `python3 tests/test_installer.py` — catalogue, updater fallback, selected
  branch structure, package names, version surfaces, install replay, and batch
  safety.
- `python3 tests/test_post_script.py` — standalone Unlayered transformation,
  extrusion conservation, Z restoration, relative-E refusal, input safety, and
  idempotence.
- `python3 tests/test_plugin_runtime.py` — fake-Orca registration, logging,
  configuration, G-code behavior, and recoverable Wave dependency reporting
  for both Wave capabilities.
- `python3 tests/test_plugin_audit.py` — all shipped plugins import under a
  hook that denies filesystem writes at import time.
- `PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py` — captured real
  export, Wave replacement, cleanup, actual Z, fail-closed behavior, fan
  restoration, and idempotence.
- `python3 tools/sync_engine.py --check` — Unlayered engine copies match.
- `python3 tools/sync_changelog.py --check` — embedded plugin notes match their
  source changelogs.
- Raw CRLF assertions — `Update-Orca-Plugins.bat` has 690 CRLF lines and
  `Choose-Orca-Plugin-Version.bat` has 136 CRLF lines.
- Python compilation and `git diff --check` pass for normal text files.

The owner's real-machine evidence is separate from those static/sandbox
checks: Unlayered previously rewrote a real export, and Wave 0.0.11 visibly
appeared in a reopened real export. The sandbox cannot run OrcaSlicer, Windows,
or a printer.

## Important decisions and safety rules

- Wave replaces covered bridge extrusion; it does not reinforce the original
  bridge. Uncovered material is retained unless it is geometrically covered by
  successfully generated Wave paths.
- Wave no longer mutates pre-export slice geometry or depends on `_PLAN`,
  object-to-bed calibration, or mutable geometry surviving between callbacks.
- A Wave parse, geometry, generation, insertion, or assembly failure retains
  the original exported G-code.
- Export processing must be idempotent because Orca can invoke it more than
  once. The geometry prototype also processes each PrintObject once per slice
  execution and rolls back failed object edits.
- Third-party dependencies are imported at module load because Orca's audit
  rules can block lazy imports during capability execution.
- Logging is lazy, bounded, best-effort, and never allowed to break a print.
- The updater's own batch file must never overwrite itself while it is running.
- A selected test branch is all-or-nothing. Missing or invalid files stop the
  install before Orca's folders change.
- Do not touch `keyboard-lighting/`.
- Keep GPL-3.0 attribution for Unlayered Infill and the predecessor tool.

## What remains to do

1. Install this branch with the launcher and confirm both plugin versions in
   Orca's separate Version column: Wave Overhangs 0.0.28, Unlayered Infill
   0.4.0.
2. **Try the new Unlayered Infill wave controls on a real slice.** The most
   valuable single test: print the same part twice, once with
   `pattern = "linear"` (the old behaviour) and once with `pattern = "cross"`,
   and break both. Nothing here has been printed. Also worth checking that
   `shape = "square"` does not cause audible Z chatter at the ramps on the
   owner's machine — it is the shape most likely to.
3. Re-print the same part with 0.0.22 and photograph the same corner: the
   0.22 mm^2 void should be gone. `Cube_39m10s.gcode` in the repo root is the
   0.0.20 print it is being compared against.
3a. Turn **Arc fitting** on in the print profile for that export, so the
   arc paths get exercised in real Orca and real firmware. Check the printer
   accepts the G2/G3 Wave blocks and that curved walls still read back
   correctly (they now arrive as arcs).
3b. The owner offered a fresh export plus screenshots of the jagged 0.0.19
   result from their own model (one with holes would be the most useful). Ask
   for it, drop it in `tests/fixtures/`, and re-measure edge straightness
   against it before claiming 0.0.20 is correct on real geometry.
4. Save the fresh export and the plugin-storage log path reported by Check
   setup if behavior differs from the fixtures.
5. Perform a small physical print; no physical Wave result is claimed yet.
6. Run the Windows batch flow again whenever either batch file changes.

## Session log

### 2026-10-02 — Wave 0.0.28: the modal feedrate leak (owner's own export)

The owner posted an OrcaSlicer preview legend screenshot of a Wave print and
asked three things: the Travel line looks wrong, why is the gram usage so
high, and why does it take so long. They said they had attached the log, the
G-code and the part.

**Process note worth remembering: they had uploaded the files to GitHub, not
to the chat.** They arrived as two `Add files via upload` commits on
`arena/01a0fb0f-tests` (`fc863c9`, `f662e6a`) containing
`test print_19m50s.gcode`, `test print.3mf`, `test print.stl` and an
OrcaSlicer debug log. Two separate filesystem sweeps for `/home/user/uploads`
found nothing and the owner was twice told the files had not arrived, which
was wrong and wasted their time. **Check `git fetch` and the remote branch
before concluding an upload is missing.**

Also observed twice this session: the sandbox's **git history rolled back to
the base commit `2d084d5` while the working tree kept all its changes**, and
a stale index then made `git diff` show phantom reversions (the archived
Geometry plugin appearing to come back). The recovery is `git fetch origin`,
`git reset --soft origin/<branch>`, then a plain `git reset` to refresh the
index. Verify with `git write-tree` against the last known commit's tree
before trusting any `--hard` operation.

**The defect.** G-code feedrates are modal. `_emit_wave_gcode` ends a block
with `M106 S<restore_fan>` and the END marker but never restores the
feedrate, leaving `print_speed` (2 mm/s, `F120`) in force; and the
replacement moves written for covered bridge extrusions carried no `F` of
their own. Those moves therefore ran at 2 mm/s.

Measured by walking every move in the owner's export:

| | moves | distance | time | speed |
|---|---|---|---|---|
| Wave fill printing | 12,114 | 7.06 m | 58.8 min | 2.0 mm/s |
| **Stranded on the wave speed** | **373** | **3.95 m** | **32.9 min** | **2.0 mm/s** |
| Normal Orca moves | 7,588 | 22.91 m | 11.6 min | 33.1 mm/s |
| Travel inside wave blocks | 277 | 4.66 m | 0.6 min | 120 mm/s |

104.0 min total, matching the 1h46m in the legend. All 342 replacement moves
in that file had no feedrate. The filename (`19m50s`) is Orca's own pre-plugin
estimate, so the plugin was turning a 20-minute print into a 106-minute one.

**Fix.** `_parse_layers` now tracks the modal feedrate and records it on each
section and segment; every move the emitter writes states its feedrate, and
the segment's original feedrate is handed back with a bare `G1 F…` before
untouched moves resume. A bare `G1 F…` sets a speed and moves nothing, so
`test_wave_gcode.py`'s "retained extrusion must follow a travel" checks were
given an `_is_motion()` predicate that requires an X or Y word.

**Do not "fix" these — they were investigated and are correct:** in-block
travels already carry `F7200` (all 186 in `Cube_39m10s.gcode`); the `put()`
taper helper does not emit zero-E moves; acceleration cannot explain the
numbers (1300 moves × 9.9 mm needs a *commanded* 5–6 mm/s, not a ramp).

**The grams question was a false alarm.** Total extrusion in that file is
7.62 g, of which wave blocks are 1.50 g (20%). The wave fill is labelled
Bridge/Internal Bridge, so those rows dominate the *time* column and look
like they dominate material. In the legend, "Usage" is filament length and
the unlabelled fourth column is grams for extruding types but a plain **count**
for Travel/Wipe/Retract/Unretract/Seams — `1.3K` on the Travel row is 1300
moves, not grams.

**Still open for the owner:** `print_speed` 2 mm/s is the remaining 58.8 min
and is deliberate (droop); `pattern = "zigzag"` would cut the ~486
retract/travel cycles. Neither was changed. No physical print has been run
with 0.0.28.

### 2026-10-02 — Launcher 1.0.1: FINDSTR noise on every start

The owner ran `Orca-Plugins.bat` on Windows and pasted the output. Before the
menu it printed `FINDSTR: Cannot open >nul`, `FINDSTR: Cannot open 2>nul`, and
one leaked internal line.

**Cause, worth remembering as a general rule:** `cmd.exe` decides what is a
redirection by toggling a quote flag on EVERY `"`. It does not understand
`\"` as an escape. `findstr /b /c:"set \"FRONTDOOR_VERSION=" "%NEWBAT%" >nul
2>nul` has five quotes, so cmd ends the line still inside a quoted string and
passes `>nul` / `2>nul` to findstr as filenames. Never put `\"` inside a
batch string. Fixed by searching for `FRONTDOOR_VERSION=`, which needs no
embedded quote.

The verification was never actually broken — findstr still matched and still
returned success, and a 404 page would still have failed all three checks.
Noise, not a hole.

**Bumped the front door to 1.0.1 deliberately:** a copy on disk compares the
`rem FRONTDOOR_VERSION ... end` marker to decide whether to hand over to a
download, so a fix that does not move the number reaches nobody.
`tests/test_installer.py` now parses both markers and fails if they disagree
instead of hardcoding the number.

**New guard:** the installer test walks every line of all three .bat files the
way cmd.exe does and fails if a redirection or pipe lands inside an unclosed
quote. Verified by re-introducing the old line and watching it fail. The only
other odd-quote line in the repo is the `set "VAR=%VAR:"=%"` quote-stripping
idiom, which has no redirection and is correctly ignored.

**Also noticed:** the owner's launcher remembered `arena/01a0f908-tests`, a
branch from an earlier session. Pressing Enter would have installed that old
build, not this session's work. There is no staleness warning on the
remembered build — worth considering.

### 2026-10-02 — Archived the Geometry prototype; Unlayered Infill 0.4.0

**Asked for:** "add changelogs for my plugins". On investigation all three
plugins already had one, so the ambiguity was put back to the owner, who
answered: archive Wave Overhangs Geometry, keep Wave Overhangs and Unlayered
Infill, make the changelogs visible inside Orca, and add features to Unlayered
Infill.

**Archived Wave Overhangs Geometry** to `archive/wave-overhangs-geometry/`
with its README, changelog and regression test (moved to
`archive/tests/test_wave_geometry.py`, paths fixed, still passing). Removed
from `plugins.json`, the hardcoded fallback plan in `Update-Orca-Plugins.bat`,
`tools/sync_changelog.py`, `tests/test_plugin_audit.py` and
`tests/test_plugin_runtime.py`. **Watch out:** the first fallback row in the
.bat uses `>` and the rest `>>`, so deleting the first row meant promoting the
next one — otherwise the plan file would have been appended to a stale one.
The .bat was edited in binary and the CRLF count asserted before and after
(705 to 702 lines, exactly the three removed).

**Unlayered Infill 0.4.0** added five wave-shaping controls to the engine:
`pattern` (`linear`/`cross`), `wave_angle`, `shape`
(`sine`/`triangle`/`square`), `layer_phase` and `max_lift_mm`. The motivating
defect is real and worth remembering: the old displacement was
`sin(frequency x)`, varying along X alone, so **an infill line running along Y
was lifted to a single constant height and keyed into nothing**. On 0/90
infill that is half the infill doing no work. `cross` averages its two axes
rather than summing them, deliberately, so the unit wave stays within [-1, 1]
and `amplitude`/`max_lift_mm` keep meaning millimetres. `square` is a
saturated sine, never a true square — a vertical Z step is not printable.

**Changelog visibility.** Both plugins print their recent changelog from their
check capability; Unlayered Infill additionally gained the settings guide that
Wave Overhangs got in 0.0.27 — notes interleaved into the JSON config panel
plus the full guide printed by Check setup, with a `settings_guide` toggle.
One honest difference from Wave's version: Orca only lets a capability read
its **own** config, so the guide prints the defaults and says so, rather than
printing a default and labelling it the user's value.

**New test:** `tests/test_unlayered_waves.py`. Its most important assertion is
that the 0.4.0 defaults produce byte-identical G-code to the pre-change
engine, proved by diffing real generated output rather than by reading the
defaults. It also pins the premise (the old wave really is flat along a
Y-running line), that no pattern or shape exceeds the requested amplitude,
that `square` never steps hard enough to be unprintable, and idempotence with
all five controls on. Note for the next session: comparing waves *between
layers* must be done on the **sign** of the displacement, not its size — the
taper legitimately scales each layer and column differently, and comparing
heights produces false failures.

**Verified here:** all repo tests plus the two new ones; engine and changelog
sync `--check`; CRLF intact; end-to-end standalone run on the real
`Cube^2_3m53s.gcode` fixture conserving total extrusion to 0.000000 mm and
idempotent on a second pass. **Not verified:** no real OrcaSlicer, no print.

### 2026-10-01 — Export hang identified as OrcaSlicer #7433 (0.0.25)

**Cause found.** The owner reported the failed export only happens when **Arc
fitting** is on in OrcaSlicer. With the `auto` default of 0.0.21-0.0.23, that
is exactly when Wave wrote G2/G3 into the finished file.

This is [OrcaSlicer issue #7433](https://github.com/OrcaSlicer/OrcaSlicer/issues/7433),
"Post processing script results in corrupted gcode / crash when previewing
model": opened Nov 2024 against 2.2.0, still reproducing in 2.3.2 nightly as of
Jan 2026, closed only by the stale bot. The reporter's trigger was ArcWelder,
which like Wave replaces straight moves with arcs. **Orca cannot reliably
re-read post-processed G-code containing arcs.** This is not fixable from
inside the plugin.

Resolution: `arc_fitting` stays `false` by default (set in 0.0.24). Check setup
prints the setting and warns with the issue number when it is on; both READMEs
carry the warning.

Wave's arcs were audited and exonerated: 118 arcs across all fixtures plus the
owner's export, radii 0.78-12.1 mm, sweeps 11-149 degrees, zero major arcs,
zero near-full circles, zero chords longer than the diameter, zero reversed
directions, all with positive E.

**Separate real defect found and fixed during that audit:** the emitter decided
whether to write a move from the unrounded step length while writing
coordinates to three decimals, so sub-micron steps became moves whose X/Y
repeated the previous line -- dead lines, mostly `E0.00000`. The owner's export
had 542. `_emit_wave_gcode` now keeps `emitted`/`pending` cells, skips a move
whose rounded coordinate is unchanged, and rolls its extrusion into the next
real move. Arc I/J are now measured from the last written coordinate instead of
the unrounded point (worst-case radius inconsistency 0.0013 mm). Owner's export:
2,006 -> 1,910 Wave moves, material conserved.

Test note: a file-level "no no-op moves" assertion is **vacuous** -- the Cube^2
fixture contains no sub-micron steps and passes with the fix removed. The real
guard drives `_emit_wave_gcode` directly with a hand-built hairline front and
was verified to fail when the dedup is reverted.

**Still open:** no fresh real-Orca export or physical print has been done on
0.0.24/0.0.25. The owner should confirm an export now completes with arcs off.

### 2026-10-01 — Export hang (0.0.24)

The owner reported that Orca would no longer export: it sat on "exporting" and
crashed roughly a minute later. Their last good print came from 0.0.20, and
0.0.21/0.0.22/0.0.23 had all shipped since, so the cause was in those three.

**Not reproduced.** Everything measurable was measured and came back clean:

* Arc fitter cost is linear -- 800-point smooth arc 0.097 s, 400-point wiggly
  front 0.035 s. Not a hang source.
* A synthetic part that overhangs 3 mm further on every single layer costs
  0.08-0.18 s per bridge layer (2/4/8/16 layers = 0.16/0.47/1.28/2.86 s), peak
  RSS 49 MB, no error. Linear; ~100 bridge layers would be ~20 s, slow but not
  a crash.
* The owner's own export reconstructed: 1.65 s (parse 0.20, plan 1.44).
* Every emitted arc re-validated across all four fixtures plus the owner's
  export (215 arcs): zero zero-radius, zero full-circle, zero endpoint/radius
  mismatch after 3-decimal rounding, every one with positive E.

So the fix is a backstop rather than a diagnosis. `time_budget` (30 s default)
is checked before every layer and every bridge section; when it fires the pass
returns the input byte-for-byte with no Wave stamp and sets `timed_out` in the
stats, and the Orca result message says so instead of reporting that nothing
was found. `0` disables it; a junk value falls back to 30. The plugin can no
longer be the reason an export does not finish, whatever the cause was.

`arc_fitting` also went back to `false` by default (was `auto`). G2/G3 is the
only genuinely new *kind* of output since the owner's last good print, and
Orca re-parses the finished file for its preview and time estimate, so it is
the best remaining suspect; arcs are opt-in via `"auto"` until a real export
clears them.

**Still open:** the actual cause. The next evidence needed is the log file
(path printed by **Check setup**), which records `seconds`, `parse_seconds`,
`plan_seconds`, `geometry_layers`, `layers_scanned`, `timed_out` and `error`.
If a 0.0.24 export succeeds with arcs off, that points at the arcs; if it times
out, the log says how far it got; if it still crashes with the plugin
`enabled=false`, the problem is not this plugin at all.

### 2026-10-01 — Processing cost (0.0.23)

The owner said exporting after a slice was taking forever. Measured on their
own export in the sandbox the Wave pass is about two seconds, so the pass was
never the whole story -- but it was doing a lot of pointless work and 0.0.20's
output was punishing everything downstream.

* Geometry is now built only for layers with a Bridge section plus the layer
  under each: 7 of 134 on their part, so 95% of the shapely objects built were
  never used. Parse 1.65 s -> 0.2 s. A no-bridge export costs 0.01 s.
* `_footprint` buffers once per line width instead of once per move.
* Cleanup builds its guard shapes once per section, not once per front.
* Stats/log now carry `seconds`, `parse_seconds`, `plan_seconds`,
  `geometry_layers`, `layers_scanned`.
* What probably caused their wait: 0.0.20 wrote 29,374 Wave moves, 0.89 MB, a
  third of the file. 0.0.23 writes 2,006 moves and 74 arcs, 0.07 MB; the file
  goes 2.66 MB -> 1.82 MB. Orca re-reads and re-estimates every move after
  post-processing.
* Tried and reverted: simplifying the reachable region each propagation step
  to cap vertex growth. GEOS threw on degenerate rings and the plugin failed
  closed with no waves at all. `wave_tracks` is still 58% of the remaining
  time (0.48 s per bridge section); a faster propagation would need a raster
  distance transform instead of repeated buffering.

### 2026-10-01 — The owner's first print, measured (0.0.22)

The owner printed the part with 0.0.20, photographed the first layer from
below, circled two areas and uploaded the export (`Cube_39m10s.gcode`, now in
the repo root along with `Cube.stl`). Having the real file meant both could be
measured instead of guessed at.

**The corner was real.** Reconstructing that layer (index 45, z 13.8) and
subtracting the Wave beads from the area inside the walls left exactly one
defect: 0.22 mm^2, 0.53 x 0.75 mm, in the corner of the plate. Cause: a
wavefront is a contour of equal distance from the supported edge and the
contours step out one line spacing at a time, so the tip of a corner is always
left short. Fixed with an explicit gap fill plus keeping short fronts that are
anchored to a full-length rung.

**The rounded wall was not.** Every Wave end along the 13 mm radius sat 0.456
to 0.457 mm from the silhouette -- 21 ends, 0.001 mm of spread. The ends are
already on the wall; the staircase in the preview is the flat end of each rung
meeting a curve at 0.35 mm intervals. A wall-hugging rung with trimming was
built and measured, made the edge worse, and was reverted. Written up in
`docs/ROADMAP.md` under the open item, with what a correct attempt needs.

Worth remembering for next time: their profile has `enable_arc_fitting = 1`
and the export carries 12,546 G2/G3 moves, so the 0.0.21 arc parser matters
for this owner specifically. On this particular layer none of the walls were
arcs, so arc blindness was not the cause of either defect -- that hypothesis
was checked and rejected before the measurement work started.

### 2026-10-01 — Wave speaks arcs; the hole file-size bug (0.0.21)

The owner asked whether Wave could emit arc moves, since they can switch Arc
fitting on in their print profile. Three things came out of it.

1. Wave runs at `psGCodePostProcess`, after Orca has written the file, so
   Orca's arc fitter never sees Wave's moves. Wave now fits its own arcs.
   `arc_fitting` defaults to `auto`, which follows the export's own
   `enable_arc_fitting` line, so a printer whose firmware cannot read G2/G3
   never receives one. An arc has to pass through the kept points, must not
   bow more than a quarter of the rung spacing, and is re-checked against the
   overhang region afterwards so a bow cannot push into a wall or a hole.
2. The reverse direction mattered more. Wave's parser ignored G2/G3 entirely.
   With arc fitting on, a round hole's wall is exported as arcs, so Wave would
   have gone blind to that wall and quietly lost the 0.0.20 perimeter fix on
   the parts that need it most. The parser now expands `I J` and `R` arcs;
   a hole wall written as four arcs gives the same wall material as the same
   hole written as 72 straight moves (10.208 vs 10.207 mm^2).
3. A file-size bug, found while measuring the arcs. Cleanup refused to
   simplify any front that *touched* a hole, and 0.0.20 had just made ends
   finish on hole walls, so 46 of 58 fronts fell back to raw rasterised form:
   a 9.9 mm front written as 980 moves instead of 9. Touching is no longer
   treated as crossing (the void is shrunk by the guard margin before the
   test), and a shortcut that really would cut a corner now retries at a
   tighter tolerance before giving up. The synthetic hole part's Wave G-code
   went from 316 KB to 9 KB.

Sandbox measurements only. The owner will re-export with Arc fitting on.

### 2026-10-01 — Wave ends snap to the real perimeter (0.0.20)

The owner reported that Wave ends would not snap to the overhang perimeter:
instead of progressing from the supported perimeter all the way out to the
overhang perimeter and around holes, fronts finished on a jagged edge that made
it hard to lay the following outer perimeters down.

Diagnosis (reproduced in the sandbox against the captured Cube export): Wave
measured the overhang from the footprint of Orca's exported bridge *lines*. The
union of those line footprints is castellated — alternating in and out by about
half a line width — and stops short of the wall, and the fronts were clipped to
it. The 0.0.19 "snap to boundary" step could not help because it snapped to
that same castellated boundary and its guard refused to move an end more than
0.08 mm.

Fix: parse `Outer wall` / `Inner wall` / `Overhang wall` moves, join them into
loops before giving them width, close the bridge footprint against that wall
bead, and let ends finish 25% of a line width inside the bead. Two safety
rules keep it honest: the growth may reshape a Wave area but never create one
(the area must still contain bridge extrusion over unsupported space), and
nothing may be placed deeper than the overlap into the outer wall, because a
wall loop is not always a sealed band.

Two further bugs were found and fixed along the way: a wall buffered one G-code
move at a time leaves a hairline slit at every vertex of a curved wall (a Wave
end slipped through one and finished on the visible surface of a hole), and
reaching for a wall could put fronts in the gaps between the sparse-infill
lines of the layer below.

Sandbox evidence is in the regression; a real Orca export and a physical print
remain open. The owner will supply a fresh export and screenshots on request.

### 2026-10-01 — Wave endpoint taper without default micro-moves

After the owner marked rectangular/grid-like endpoint texture in preview, Wave
Overhangs moved to 0.0.19. `edge_taper_segment` now defaults to `0`, so taper
changes E on existing straight moves instead of adding tiny endpoint subdivision
moves. Endpoint snapping now extends along the Wave endpoint direction to reach
the wall/hole boundary, avoiding sideways doglegs from nearest-point snapping.
Sandbox regressions pass; a real Orca export/print remains open.

### 2026-10-01 — Wave endpoints snap to perimeter by default

After the owner showed that v0.0.17's default centerline clearance left visible
gaps around walls and holes, Wave Overhangs moved to 0.0.18. `edge_clearance`
now defaults to `0` and is only a comparison/debug control. The active default
snaps Wave endpoints back onto nearby non-support detail boundaries before
applying endpoint flow taper, so the paths should conform to wall and hole
perimeters more like Orca's normal bridge infill while still reducing blobs.
Sandbox regressions pass; a real Orca export/print remains open.

### 2026-10-01 — Wave edge clearance before taper

Wave Overhangs moved to 0.0.17. The post-processing plugin now clips emitted
Wave centerlines back from non-support detail boundaries before applying endpoint
flow taper. The old cleaned Wave paths still drive bridge-removal coverage, so
straight Bridge fragments do not reappear just because the visible Wave bead was
inset. `edge_clearance="auto"` follows the exported bridge width; set it to `0`
to compare against the previous full-length endpoint behavior. Sandbox
regressions pass; a real Orca export/print remains open.

### 2026-10-01 — Wave endpoint flow taper

After the owner decided to stick with the original post-processing Wave plugin,
Wave Overhangs moved to 0.0.16. The geometry/path planner stays the same, but
the emitted Wave G-code now tapers E near endpoints that touch outer walls,
holes, or concave detail boundaries. The support-side anchor boundary is
excluded from tapering so starts keep full flow. `edge_taper_distance=0`
restores the old no-taper 399-move Cube output for comparison. The post-
processing plugin already used Orca plugin storage for log/state writes, so the
no-approval-prompt behavior remains in place.

### 2026-10-01 — Wave Geometry fill-surface method

After the owner clarified that the preview showed Wave shapes but they were
still behaving as overhang-wall islands, the geometry prototype moved from
`LayerRegion.slices` at `posSlice` to `LayerRegion.fill_surfaces` at
`posPrepareInfill`. This should preserve Orca's original perimeter and make the
unsupported Wave ribbons bridge-classified fill surfaces instead of separate
slice islands. It still cannot guarantee final bridge/perimeter G-code order
because generated `ExtrusionPath` collections remain read-only in Python.
Sandbox verification passed for the implementation in this branch.

### 2026-10-01 — Wave Overhangs Geometry prototype

The owner chose a separate alternate plugin that runs before G-code so Wave can
interact with Orca's geometry and appear in the preview. Current Orca source
bindings expose editable `LayerRegion.slices` and `Layer.make_slices()`, but
existing `ExtrusionPath` collections are read-only. Added
`plugins/wave-overhangs-geometry/` v0.1.3: it generates obstacle-aware Wave
fronts and writes a non-bridge outer wall shell plus bridge-classified interior
Wave ribbons at `posSlice`, while keeping the post-processing Wave plugin as
fallback. It reports the raw-path limitation, uses object-level rollback on
mutation failure, and fails closed when the host API or dependencies are
unavailable.

The owner confirmed the geometry version registers in Orca and is much cleaner
than the previous version, then reported remaining preview issues: tiny dots,
missing/broken outer overhang wall, overhang-wall classification instead of
bridge, and a thin-air start order. Version 0.1.3 addresses those with dot
filtering, a continuous non-bridge outer wall shell, `stBottomBridge` interior
Wave ribbons clipped inside that shell, and support-outward bridge surface
ordering. It also moves routine logs/state to plugin storage to avoid normal
approval prompts. Synthetic Wave geometry, circular-hole safety, installer,
runtime, audit, changelog, compilation, and CRLF checks pass. A follow-up
real-Orca preview of 0.1.3 and physical printing remain unverified.

### 2026-10-01 — Wave endpoint cleanup

The supplied screenshots showed that the topology was correct but individual Wave
starts and ends were frayed around the outer wall and circular opening. The
cleanup now keeps the simplified interior curve points, removes only short
stubs at each endpoint, and merges connected obstacle-front pieces before
cleanup. Geometry tests assert that surviving front endpoints stay on the valid
wall or hole boundary and that cleaned curves remain inside the allowed domain.
The captured Cube result changed from 386 to 399 Wave extrusion moves because
more valid curve points are retained; covered-move, bridge-fragment, idempotence,
and fail-closed behavior remain covered by the regression.

A fresh Orca export and physical print are still required. The screenshots are
visual evidence of the old behavior, not proof of the new output.

### 2026-10-01 — repository cleanup

Asked for a repository-wide organization pass. The prior branch state was
fetched and verified byte-for-byte before local alignment. The supplied PDFs
were moved to `docs/reference/`; the model and captured G-code were moved to
`tests/fixtures/`; runtime `orca-plugins.log` was removed from source control
and added to `.gitignore`. README, plugin guides, roadmap, Orca facts, AGENTS,
and changelog content were reconciled with the current implementation. The
obsolete Wave slice-object planning helpers and their stale documentation were
removed from the active plugin module; the one-pass G-code design remains.
Tests and synchronization checks are the release gate before commit.

### 2026-10-01 — Wave 0.0.15

The owner asked for smoother, cleaner fronts and more control. The configuration
then documented every setting, including obstacle propagation, pattern,
endpoint, component ordering, cleanup, extrusion, speed, fan, and iteration
safety. `component_order="nearest"` can reduce same-distance travel moves while
preserving near-to-far anchoring; the safe default remains
`component_order="support"`. The Cube regression remained unchanged.

A fresh 0.0.15 Orca export and physical print were still unverified at that
point.

### 2026-10-01 — Wave 0.0.14

The owner supplied a comparison showing a stray diagonal through a circular
opening. The likely cause was safe boundary geometry being simplified into a
straight chord, not legitimate diffraction. Wave cleanup now checks the
simplified front against the allowed unsupported region and its interior holes;
if a shortcut would cross a void, it keeps the original curved boundary. A
circular-hole regression protects this case, and the existing Cube measurements
remain unchanged.

A fresh 0.0.14 Orca export and physical print remain unverified.

### 2026-10-01 — Wave 0.0.13

The owner reported three refinements: a sharp extruding handoff when the nozzle
returned to the original bridge, remaining small edge blobs, and a desire for
pattern/start controls. Wave now emits a non-extruding `G0` to every replaced
segment endpoint before any retained original `G1` move. Cleanup has configurable
minimum front and segment lengths, simplification tolerance, and retained
fragment threshold. `smart`, `monotonic`, and `zigzag` patterns plus deterministic
endpoint policies are available. The embedded geometry diffracts around support
boundaries and now treats internal holes in the unsupported plane as obstacles;
synthetic concave, internal-hole, and circular-hole regression checks accompany
the Cube fixture. The fixture passed with 386 Wave extrusion moves and no
Wave-block-to-retained-extrusion handoff without a G0. Monotonic is a mechanical
near-to-far ordering, not a thermal guarantee: recently printed plastic may
still be warm and soft.

A fresh 0.0.14 Orca export and physical print remain unverified.

### 2026-10-01 — Wave 0.0.12

The owner reported that Wave was finally visible but one layer low and had
small edge dots. The captured export showed nominal `;Z:` comments and actual
moves separated by the profile's 0.25 mm Z offset. Wave now uses actual modal
Z, derives coverage width from the exported bridge, simplifies wavefronts, and
drops sub-nozzle retained fragments. The captured fixture passes with three
layers and 436 cleaned Wave extrusion moves.

### 2026-10-01 — Wave 0.0.27: one launcher, and self-documenting settings

**Two .bat files became one front door.** The owner could not explain the
difference between the "chooser" and the "updater", which was a naming
problem hiding a structure problem. `Orca-Plugins.bat` is now the single file
to download: it shows the remembered build and data dir, installs on Enter,
and offers version switching and forgetting. It self-updates with the same
fetch-verify-handover pattern as the engine, never rewriting the running file.

Both old names are kept, for *different* reasons, and this distinction
matters. `Choose-Orca-Plugin-Version.bat` is a convenience forwarder for
shortcuts. `Update-Orca-Plugins.bat` **must** keep its name and URL: every
copy already on a user's disk polls that exact address for its own
self-update, so renaming it would silently strand them forever. Never rename
a file that older copies of itself fetch by hardcoded URL.

**The settings now explain themselves in Orca.** Following the 0.0.26 finding
that JSON cannot carry comments, the script capability -- renamed to
`Wave Overhangs - Settings guide & check` -- prints every setting with its
note, the live value, and a marker on anything changed from default, wrapped
to 72 columns because Orca's message box clips rather than reflows. A
`settings_guide` toggle silences it. Renaming a capability changes its
identity in Orca and can detach it from a process preset, so the
identity-pinning test was updated deliberately, with a user-facing note.

**New safety net:** cmd.exe cannot run in this sandbox, so the .bat files now
get a static check that every `goto`/`call` target exists. It immediately
caught a self-test bug of my own: a case-sensitivity mistake where I compared
an uppercase needle against a lowercased haystack, so a "never overwrite
yourself" assertion was silently passing. Always prove a new assertion fails.

### 2026-10-01 — Wave 0.0.26: updater memory, a GEOS crash, and an honest no

Three owner requests handled together.

**Arc moves to shrink the file: measure before building.** The answer turned
out to be the opposite of the request. Wave's own arcs are worth 9.8 KB, 0.55%
of the export, because 0.0.21-0.0.23 had already cut the wave blocks from
33.5% of the file to 3.4%. Orca's own arc fitting, by contrast, encodes 37% of
the printed path length (12,546 arcs, 30.3 m of 82.7 m). So the lever is
Orca's setting, not ours, and since 0.0.24 stopped emitting G2/G3 the owner
can safely turn Orca's arc fitting back on. Nothing was built for this.

**The settings panel had no explanations, and that was invisible from here.**
The owner went looking for "arc fitting" in the plugin, could not find it, and
asked for comments in the config. Both complaints have the same root cause:
Orca renders the capability config as JSON, JSON has no comment syntax, and so
the thorough comments in `_DEFAULTS` never left the source file. The panel was
33 bare keys. Notes now ship *in* the config as `_`-prefixed keys, built by
`annotated_defaults()`; `_cfg()` already copied only keys present in
`_DEFAULTS`, so a note can never become a setting. Tests assert every setting
is explained, each note sits directly above its setting, and the panel
round-trips to exactly `_DEFAULTS` with the notes present and with them all
deleted. Lesson: a setting the user cannot interpret is not a feature, and
source comments are not user documentation.

**Updater memory.** The chooser stored the last branch but still required a
menu pick; empty Enter now reuses it. The updater now also remembers its data
directory in `%LOCALAPPDATA%\OrcaPluginUpdater\datadir.txt`, written on every
successful run including `%1` and `%ORCA_DATA_DIR%` overrides. Both .bat files
stay CRLF; note `Update-Orca-Plugins.bat` is now 705 lines, and the tests
assert byte-exact CRLF but not a line count.

**`wake_blend`: built, measured, shipped off.** Morphological closing of the
reached region smooths the sharp V where the wave rejoins behind a hole, and
it works on the synthetic round hole. On the owner's real part it loses 4% of
wave coverage (1911 -> 1833 mm) and takes tiny fragments from 8 to 40, because
healing makes consecutive fronts partly coincide and the "already reached"
subtraction then cuts them into dashes. Two fixes failed. It ships at default
0, where output is byte-identical to 0.0.25 on all five shapes, and the
untried polyline-fillet approach is written up in `docs/ROADMAP.md`. The
lesson worth keeping: path and move counts are blind to wave *shape* -- four
`wake_blend` values all gave 56 paths on geometry that rendered very
differently. Judge shape by rendering it, and quantify with total path length
and fragment counts.

Chasing that exposed a genuine latent crash. `linemerge` raises
`GEOSException` on a single-point crumb left by clipping, and `GEOSException`
is not a `ValueError`, so the existing `except (TypeError, ValueError)` could
not catch it and the entire layer failed closed. Same fault that had killed an
earlier `simplify` optimisation, so that is worth revisiting now. The retry
lives only inside the `except`, keeping the success path bit-identical.

### 2026-10-01 — Wave 0.0.11

Replaced the ineffective slice-object/cross-callback architecture with one
transactional G-code bridge replacement pass. Real captured-export evidence
showed three generated layers, covered bridge removal, retained uncovered
fragments, fan restoration, fail-closed generation, and idempotence. The owner
then confirmed visible waves in real Orca.

### Earlier sessions

- The chooser was made strict: selected branch only, all-file preflight, safe
  manual/API failure behavior, and selected-branch updater execution.
- Plugin identities were made permanently version-free after Orca preset
  identity research.
- Unlayered Infill received its standalone tool, complete controls, shared
  engine synchronization, relative-E refusal, and later no-prompt plugin-storage
  logging.
- Import-time filesystem writes were removed from all shipped plugins and
  guarded by the audit test.
