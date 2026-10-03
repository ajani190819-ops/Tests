## 2026-10-02 — contour_finish, and an artifact I could not reproduce (0.0.49)

> "As the waves reach that outer perimeter I'd like a smooth line, because
> right now on these rounded off edges you can see as the waves go towards
> where that perimeter will be it curves back inwards into area that is
> already printed instead of following the contour."

The mechanism is real: wavefronts are contours of distance from the
SUPPORTED edge, so near a curved wall the outermost front is not parallel
to the wall and its tail points somewhere else. `contour_finish` adds a
bead along the far boundary itself after the fronts, covering only what
they missed.

It ships **off**, because on every export available here the fronts already
reach the wall and the pass adds nothing. On t3: 99% of wave ends are
within 0.3 mm of the wall, the median is 0.157 mm -- exactly
`wall_overlap` x line width -- and the median gap from wall to wave
material is 0.000 mm. `contour_finish` finds zero paths to add.

The part that shows the problem is the curved-perimeter model, and the only
export of it in the repo is already-processed 0.0.39 output, which cannot
be re-run. **A clean export of that model with the plugin switched off is
what is needed.** Until then this is a switch to try, not a fix to claim.

---

## 2026-10-02 — no, it was not fixed (Wave 0.0.48)

> "And you're certain you fixed that error that's in the last 200 lines for
> that layer?"

No. Reading the tail of the waved layer instead of measuring aggregates
found six stranded retract/wipe/travel cycles chained together, printing
nothing:

```
G0 F7200 X100.440 Y91.979
G1 E-1.75 F1800          <- retract
;WIPE_START ... ;WIPE_END
G1 X119.932 Y120.252 F7200
G1 E1.75 F3600           <- unretract
```

Relocating the overhang wall moved its extruding moves and left its
plumbing -- the travel in, the unretract, the retract and the WIPE block --
at the old position. Two previous attempts missed it because both were
measuring travel DISTANCE, which this barely moves.

A relocated run now takes that whole block with it, and a run that cannot
is not relocated at all. Stranded cycles on t3: 32 in the unprocessed
export, 41 in 0.0.47, **38** now. Absorption is refused unless the
extrusion inside it nets to zero, so an unmatched retract can never shift
the E values after it; extrusion totals are identical with relocation on or
off.

Five of the remaining cycles are the wave replacement's own, not the
wall's: removing a covered bridge move can leave the wipe that belonged to
it behind. `wall_last: false` gives 37, which separates the two. That is
the next thing to fix, and it is written down rather than claimed as done.

---

## 2026-10-02 — the wall loop was coming apart (Wave 0.0.47)

> "Specifically I'm talking about done printing the waves, how it kind of
> just goes back through the layer stopping at random points."

Found it, and it was mine. The wall relocation added in 0.0.36 judged each
wall MOVE on its own, so a loop that was partly over air came apart: the
hanging moves went after the waves, the supported ones stayed where they
were, and the nozzle crossed the layer to stitch the two together. On t3
that produced six separate relocated pieces, emitted in the order the
slicer happened to write them.

A loop is now judged as a whole -- mostly hanging means all of it moves,
mostly supported means none of it does -- and the pieces that do move are
ordered nearest-neighbour from where the waves ended. Travel after the
waves on t3: 59 travels / 729 mm in 0.0.45, 28 / 279 in 0.0.46, now
**27 / 240**.

Also new: `keep_uncovered_bridge: false` drops the trips back for leftover
bits of original bridge.

Honest caveat: counting travel-then-short-extrusion pairs across the whole
file finds 199, of which exactly one is this plugin's. The rest are
Orca's own infill ends and wipe sequences and are in the unprocessed
export too. If you still see stop-start motion after this, set
`wall_last: false` -- that turns the relocation off entirely and tells us
in one slice whether we are still looking at the same thing.

---

## 2026-10-02 — the post-wave jumping, properly this time (Wave 0.0.46)

> "It kind of jumps around after completing all of the waves. Once the
> waves are done it shouldn't need to go back."

0.0.41 was supposed to have fixed this and the synthetic tests agreed. On
the real export it barely fired: Orca writes `M73` progress lines between
the moves, and any non-comment line ended a run of travels, so the pattern
travel/M73/travel/M73 survived intact. 59 travels, 729 mm of motion to
print 250 mm.

M-codes that change state but do not move the nozzle now sit inside a
travel run without ending it. **28 travels, 279 mm** on the same file, and
what remains is genuine repositioning between separate pieces -- the
longest is a 34 mm hop to the relocated overhang wall. The per-move
comments also fold into one line naming a range, which took 390 lines down
to a handful on that layer.

The synthetic fixtures contain no `M73`, which is exactly why they could
not catch it. That is the second time this week a captured export has
contradicted a green test suite.

---

## 2026-10-02 — the jagged curves were hairpins, not facets (Wave 0.0.45)

> "Those curved perimeters don't do so well with the waves, they still look
> very jagged... it just looks like the waves on that curved arc aren't
> smooth at all."

Measured on the newly uploaded export: the **median** turn at a wave vertex
was 15 degrees -- a smooth curve -- but the 90th percentile was **90** and
the maximum 179. That is not faceting from too-coarse simplification, which
is what it looks like and what I went looking for first. Those are hairpins:
the front folds back on itself where it flows around something and rejoins,
and every later front inherits the kink. The line of kinks is the chevron
seam.

Three wrong theories were measured and discarded on the way: simplification
tolerance (sweeping it changes almost nothing), arc resolution, and a
scalloped support footprint (morphological closing from 0.3 to 1.5 mm moved
the median turn by less than a degree).

`smooth_creases` (on by default) chamfers each sharp vertex three times at a
shrinking setback, checked against the same guard that stops a front
entering a hole. 90th-percentile turn 90 -> 22 degrees, vertices over 20
degrees 38% -> 13%, path length within 0.2%. It costs about twice the points
on a crease-heavy layer, and thinning them back undoes the fix -- the extra
points are the roundness.

---

## 2026-10-02 — waves only where a straight bridge cannot do the job (0.0.44)

> "The only parts that should be receiving wave overhangs should be ones on
> the underside where you have horizontal overhangs that don't have any
> other method of support... bridges at the top are still using the wave
> overhangs instead of straight bridges... also the little divots on the
> underside, those are using waves but they don't need to."

Right on both counts, and the owner's t3 export separates the three cases
cleanly. "Reach" below is the distance from solid material to the furthest
point of the unsupported patch; a straight bridge has to cross twice that:

| Z | type | area | reach | verdict |
| --- | --- | --- | --- | --- |
| 5.4 | Bridge | 1035 mm2 | 37.5 mm | **wave it** |
| 7.8 | Bridge | 18-54 mm2 | 2.4-3.7 mm | the divots -- straight bridge |
| 8.1 | Bridge | 1.3 mm2 | 0.2-0.4 mm | specks -- straight bridge |
| 9.3 | Internal Bridge | 1090 mm2 | 5.5 mm | solid over infill -- straight bridge |

Two filters, both on by default: `Internal Bridge` sections are left alone
(`wave_internal_bridges: false`), and any unsupported patch a plain bridge
can cross is left alone (`straight_bridge_span: auto`, 10 mm). t3 goes from
7 waved sections to 1, and the pass drops from 21.7 s to 13.6 s.

`straight_bridge_span: 0` plus `wave_internal_bridges: true` restores the
old behaviour exactly, and the existing geometry regressions run that way so
they keep measuring geometry rather than selection.

---

## 2026-10-02 — t3 arrives, and the "Wave does nothing" cause is NOT the timeout

The owner's t3 export landed, and running the real capability against it
settles it: **the plugin handles that part fine** -- 3 wave layers, 7
sections, 688 bridge moves replaced, 20.0 s. The hole count and the
multiple overhang layers are not the problem, and neither is the time
budget: 0.0.41, with its flat 30 s, also completes the file in 19.9 s.

What the file shows instead: its first line is
`; unlayered-infill v0.4.8 (non-planar sparse infill)` and there is no
`; wave-overhangs v...` stamp anywhere. **Unlayered Infill ran on that
export and Wave Overhangs never did.** `Others -> Slicing Pipeline Plugin`
is one preset field and both plugins want it.

So 0.0.43 adds the message that would have caught this immediately: when
Wave has never been handed a file, Check setup now says so and points at
the selection rather than leaving you to suspect the geometry.

The previous entry's timeout work stands on its own merits -- 20 s against
a 30 s limit on a 1.9 MB file is uncomfortably close, and a bigger part or
a slower machine would trip it -- but it was not what happened here, and
the changelog entry claiming it would be was written before the evidence
arrived.

---

## 2026-10-02 — why a complex part came back unprocessed (Wave Overhangs 0.0.42)

> "The G-code did not get overwritten for this one, the whole system just
> didn't work. I have a theory that it has something to do with not being
> able to handle things with larger numbers of holes or having multiple
> overhangs."

The theory is right in effect, and the mechanism is the **time budget**. The
pass costs what the geometry costs; the limit was a flat 30 seconds, set
against a test cube. When it runs out the file is handed back exactly as
Orca wrote it -- correct, and indistinguishable from the plugin never
running.

Three changes: the budget is now `auto`, 30 s plus 45 s per megabyte capped
at 300 s; the pass is about 1.8x faster on parts with many holes, because
`_interior_voids()` was being recomputed once per endpoint instead of once
per region (40% of the total on a 36-hole stress case); and a budget that
does run out now leads the Check setup report with
`*** THE LAST EXPORT RAN OUT OF TIME ***`.

**This is a diagnosis by reconstruction, not from your file** -- only
`t3.stl` arrived, and there is no slicer in this sandbox, so I built a
synthetic part with up to 36 holes over three overhang layers to measure it.
Export t3 to G-code with the plugin OFF and drop it in `test-prints/` and I
can confirm it directly. The log line or Check setup will also now say
outright whether a timeout is what you hit.

---

## 2026-10-02 — nozzle clearance, the post-wave "scanning", and a home for test prints

Wave Overhangs **0.0.41**, Unlayered Infill **0.4.8**.

**The nozzle "scanning across the print" after the waves is fixed.** It was
not gap filling. Each covered bridge move was replaced by a comment and a
travel to where it used to start, so the nozzle re-traced the original
bridge raster in mid-air -- 63 travels for 11 extrusions. Consecutive
travels now collapse to the one that matters: 206 to 151 on the Cube export,
with extrusion identical to the digit.

**Unlayered Infill now checks the nozzle cannot drag through what it has
already printed.** The finished file is walked in print order, remembering
the highest material in each XY cell; anything passing below it is reported
with the place and the depth. Threshold "auto" is 1.25 layer heights, which
is quiet at the shipped amplitude (0.23 mm of deliberate keying) and
reports 400% (0.46 mm) and above. `collision_check: warn | refuse | off`.

**`test-prints/` exists now**, with a README on what to put in it and a
`notes.md` per model saying what to look at. Cube^2 has been copied in.

**Still open, waiting on files**: the jagged wall snapping on curved
perimeters. Drop that export into `test-prints/` and I will work from it --
the synthetic rounded-corner case in the suite does not reproduce what the
photo shows.

---

## 2026-10-02 — curves no longer cost hundreds of moves (Wave Overhangs 0.0.40)

> "There's certain points where a curve will have way more lines than it
> needs to... when the lines approach the hole in the test print sometimes
> there will be like hundreds of lines when a couple dozen should have
> sufficed."

Confirmed and fixed. Simplification was all-or-nothing per front: if the one
chord near a hole would have cut the corner, the whole front kept its raster
points. A 28 mm front came out as 132 moves; it is now 3.

When the ordinary simplification gives up, the front is now refined
chord-by-chord against the same boundary guard, splitting only the moves
that actually fail. Rounded-corner test case: 1048 wave moves to 606, path
length unchanged to 0.02%. Cube export: 439 to 432, 455 to 425 with
`wall_snap: false`.

Worth recording that the obvious version of this fix -- doing the per-chord
refinement in the main simplification pass -- was measured and **rejected**:
against a castellated bridge footprint nearly every chord leaves the region,
so the recursion splits down to the raster and the Cube export went from 455
moves to 2350. It is used only as the fallback.

---

## 2026-10-02 — fix "The preset stores invalid plugin capability configuration JSON"

Wave Overhangs **0.0.39**, Unlayered Infill **0.4.7**. Reported by the owner
against both plugins.

**Cause.** A preset override is a flat key=value record whose reference
separator is `;`. `docs/ORCA-PLUGIN-FACTS.md` already said a capability NAME
may not contain one; the same restriction applies to the configuration value
and that had been missed. When the notes were rewritten into short one-liners
for the readable panel (0.0.35 / 0.4.4), nine Wave notes and six Unlayered
notes picked up a semicolon -- "master switch; false leaves your G-code
untouched" -- and the config was written pretty-printed, 86 lines for Wave.
A preset cannot store that intact, and Orca reports exactly what it reads
back.

**Fix.** Every string the plugins write now passes through `preset_safe()`
(semicolons become commas, double quotes become single, newlines and tabs
become spaces, control characters are dropped), and `dump_config()` writes
the JSON on one line. A test fails if any key or value in either panel
contains a character a preset cannot carry.

**Clearing the error on your machine.** The bad value is stored in the
PRESET, so installing the new build does not remove it:

1. Update both plugins and restart OrcaSlicer fully.
2. Open the process preset's plugin configuration dialog, clear the stored
   override for the capability (or press Restore defaults there), and save
   the preset.
3. If the message persists, set Others -> Slicing Pipeline Plugin to None,
   save the preset, set it back to the plugin, save again. That rewrites the
   preset's plugin section from scratch.
4. The global settings in `data_dir()/orca_plugins/config.json` are a real
   JSON file and were never affected -- and if anything did get lost,
   `restore_backup: true` puts your values back.

---

## 2026-10-02 — everything that benefits from auto is now on auto

> "Put anything that would benefit largely from being on auto, like max
> iterations, on auto."

Wave Overhangs **0.0.38**, Unlayered Infill **0.4.6**. The three remaining
derivable settings are now on by default:

| plugin | setting | auto means |
| --- | --- | --- |
| Wave | `fan` | your profile's Bridges fan speed (100% if it states none) |
| Unlayered | `wave_angle` | square across your profile's infill angle |
| Unlayered | `max_lift_mm` | a Z ceiling of 1.5 layer heights |

`max_iterations` has been auto since 0.0.35, along with everything
width-derived; this finishes the job.

Two notes worth reading. **Wave's `fan` auto now reads `bridge_fan_speed`
only** -- it used to fall back to `overhang_fan_speed`, a different setting
about sloped walls that is often much lower (50% on the captured test
profile), and quietly under-cooling unsupported extrusion is the wrong way to
fail. **Unlayered's `wave_angle` is a visible change**: with 45 degree infill
the ripples now run at 135 degrees instead of along X. That is the point of
the setting -- a line parallel to the ripples never crosses one and gets
lifted rather than waved -- but it will look different from 0.4.5.

What deliberately stays a fixed number in both: amplitude, pattern, shape,
flow ratio, print order, the taper fractions, the on/off switches. These have
no Orca equivalent and nothing in the G-code implies them. Automating a
judgement about how you want the part to look is not automation, it is just a
different arbitrary number.

---

## 2026-10-02 — settings that survive the config being wiped, and a faster release path

> "When a config is updated it kind of erases whatever my settings were. Is
> there a way we could make it so that when it's updated it preserves my
> settings... I'd only imagine ones would be overridden or have to be erased
> if something extremely major happened, like the variable was entirely
> removed. I'd also want you to make these improvements faster... and add
> some stuff into the agents page so there is a more streamlined method."

Wave Overhangs **0.0.37**, Unlayered Infill **0.4.5**.

**Settings insurance.** Version-to-version migration already preserved every
value you had set; what it could not survive was the config file itself going
away, which OrcaSlicer can do for reasons outside the plugin's control
("Restore defaults", a reinstall, a data-directory change). Both plugins now
keep their own rolling backup of the values in force -- a short history
rather than one slot, because the wipe is followed by a run that would
otherwise overwrite the only copy with the defaults that just replaced your
settings. Restoration is a one-shot `restore_backup: true`, never automatic,
so "Restore defaults" still means what it says. The snapshot is taken both by
every run and by the config lifecycle hook, so a value typed into the Config
panel is remembered even if you close Orca without slicing. Settings this build no longer
has are dropped rather than resurrected.

**Faster releases.** Two new tools and a playbook in `AGENTS.md` section 5a:

* `tools/bump_version.py <plugin> <version>` writes the version to all six
  places at once -- PEP 723 header, `PLUGIN_VERSION`, `TOOL_VERSION`,
  `MARKER_VERSION`, `plugins.json`, and the `.bat` fallback line in BYTES so
  the CRLF survives -- and tells you what is still missing.
* `tools/check_all.py` runs every test in both dependency states plus the
  three consistency checks, one line each, and treats a SKIP as a failure so
  a missing shapely can never be mistaken for a pass.

The playbook also writes down the traps that have actually cost time here:
never write the `.bat` in text mode, lead changelog entries with bullets
because the description generator reads the first one, pinned geometry counts
measure geometry and not defaults, and ask clarifying questions before
writing code rather than after.

---

## 2026-10-02 — waves first, wall last (Wave Overhangs 0.0.36)

> "All of the waves should be printed first, 'cause then they can actually
> support one another as it bridges its way out, but those overhanging walls,
> if printed first, won't be able to do anything -- they'll just fall straight
> down... is there a way to match the way Arachne walls are done, where you
> vary the line width or flow rate, to optimize spacing?"

**Print order.** Orca emits a layer walls-first, which on an overhanging
layer puts the wall into open air before anything exists to hold it up. Wave
now lifts the overhanging part of the wall out of its original position and
re-emits it directly after the wave block, verbatim — same coordinates, same
E, same acceleration and jerk, total extrusion identical to the digit. Only
relative-E exports are touched, a travel is left where the run was cut out,
and the block hands the nozzle back where the waves left it. `wall_last:
false` restores Orca's order.

**Adaptive flow (experimental, off by default).** The Arachne idea is that a
fixed bead width cannot tile an arbitrary shape, so the width should vary.
Wave hits that in one dimension: the strip between the last front and the far
boundary is rarely a whole spacing wide. `adaptive_flow` assigns each
uncovered patch to the front beside it and widens that front's flow to absorb
it, capped at `adaptive_flow_max`. Paths never move — only extrusion — which
is the part of Arachne that can be done safely after slicing. On the Cube
export it is +0.03% extrusion, or +0.12% with `gap_fill` off; it is a
refinement, and it stays opt-in until someone has printed it.

---

# Changelog — the whole project

Everything that changed in this repository, newest first. Each plugin also has
its own changelog, which is the one to read if you only care about what
OrcaSlicer will do differently:

* [`plugins/unlayered-infill/CHANGELOG.md`](plugins/unlayered-infill/CHANGELOG.md)
* [`plugins/wave-overhangs/CHANGELOG.md`](plugins/wave-overhangs/CHANGELOG.md)

The archived Wave Overhangs Geometry prototype keeps its own history at
[`archive/wave-overhangs-geometry/CHANGELOG.md`](archive/wave-overhangs-geometry/CHANGELOG.md);
it is no longer shipped. See [`archive/README.md`](archive/README.md).

This file covers the repository as a whole: the updater, the tests, the
documentation and the handoff notes as well as the plugins. It is part of the
memory system described in [`MEMORY.md`](MEMORY.md) — a new chat should be
able to read `AGENTS.md`, `MEMORY.md` and this file and know where things
stand.

Dates are the day the work was done. "Not verified" means exactly that: no
real OrcaSlicer was involved.

---

## 2026-10-02 — readable config panels, and settings that inherit from Orca

> "These guides are very hard to read in this format... I'd also like for
> these configs to inherit as many of Orca's settings as possible so that the
> resolutions match up... for the iteration cap it should have an auto
> function so that it can adjust as needed without bloating the file... and
> for the things that don't have Orca equivalents, have them automatically
> defined where applicable — look at the G-code and determine what the value
> should be."

Wave Overhangs **0.0.35**, Unlayered Infill **0.4.4**. Three things, in both
plugins.

**1. The panels are grouped and the notes are one line.** 33 settings and 14
settings respectively, now in numbered sections, generated from a single
`_SECTIONS` list that also drives the "Check setup" guide — so the panel and
the guide can never fall out of step. The reason notes had grown into
paragraphs is that they were the only documentation; the reason they must not
be is that a JSON editor renders `\n` as two literal characters. The long
form moved to the READMEs.

**2. Twelve settings now default to "auto" and are derived from the print.**
The principle: most of these numbers were never constants, they were
multiples of something the export already states, written down once for a
0.4 mm nozzle. Every factor was chosen so that auto on a stock 0.4 mm
profile reproduces the shipped constant exactly — enforced by a test, because
"we improved your defaults" is not an acceptable surprise on someone's
printer. On the 0.6 mm captured fixture the waves are now spaced for a 0.6
and the file is *smaller*: 2,931 wave moves against 3,087.

Inherited straight from the user's Orca profile: Resolution (as the floor for
wave smoothing), travel speed, bridge speed (since 0.0.26), bridge fan
(opt-in), arc fitting and arc tolerance, nozzle diameter, bridge line width.
Derived from the G-code where Orca has no equivalent: everything width-
derived above, plus Unlayered's segment length, blend radius and ripple
frequency from the nozzle, and its wave angle from `fill_angle` (opt-in).

**3. The iteration cap sizes itself.** It was a flat 400 — too small for a
large overhang, pointless for a small one. Auto measures the region, asks for
enough fronts to cross it plus headroom, and clamps to 64..20000. It cannot
bloat the file because it never adds a front the geometry did not ask for.

**What deliberately stayed constant**: amplitude, pattern, print order,
flow ratio, the on/off switches. Reading the G-code cannot tell you what
someone wants their part to look like.

**Your settings survive.** The 0.4.2/0.0.33 migration is unchanged and was
re-checked for this: on the first slice after updating, the saved config is
merged with the new one, keeping every value you set.

Tests: `tests/test_wave_gcode.py` gains an "auto defaults" block (auto
reproduces the 0.4 mm constants, scales on the 0.6 mm fixture, follows the
profile, caps adaptively, still idempotent); the fixture's pinned geometry
counts now run against an explicit legacy config so they keep measuring
geometry rather than defaults. `tests/test_plugin_runtime.py` checks both
panels are complete, grouped, free of orphan notes, and one line per note.

---

## 2026-10-02 — Unlayered Infill 0.4.3 / Wave Overhangs 0.0.34: the Refresh failure

> "If I click refresh on the plugins page in Orca Slicer the Wave Overhangs
> is fine but then the Unlayered Infill will fail."

Two defects, both specific to Unlayered Infill, which is why Wave Overhangs
survived the same Refresh.

* **A lazy stdlib import inside a capability call.** The engine's
  `detect_layer_height()` did `from statistics import multimode` on first
  use, which is inside `psGCodePostProcess` — inside Orca's per-call audit
  scope, where every file open is audited. `docs/ORCA-PLUGIN-FACTS.md` has
  said since the numpy/#15944 investigation that every import must happen at
  module load time, and this one had slipped through because `statistics` is
  stdlib and looked harmless. It is now a top-level import in the engine.
* **Re-import could destroy the engine.** Refresh imports the plugin module
  again in the same interpreter. The module published an empty
  `nonplanar_core` into `sys.modules` before exec'ing the engine into it and
  popped it on failure, so a second pass could leave a previously working
  engine replaced by nothing — "engine MISSING", every capability failing.
  The replacement module is now built aside and published only after a clean
  exec; otherwise the working engine is kept. When the engine really cannot
  load, Check setup now prints the exception instead of a bare "MISSING".

Wave Overhangs 0.0.34 is the same two patterns closed preventively: its
`wave_core` registration got the same aside-then-publish treatment, and
`_pt()`'s lazy `from shapely.geometry import Point` now comes from the
module-level import. Its output is unchanged.

Guarded by a new case in `tests/test_plugin_audit.py`: a capability call must
not import any module that was not already imported at plugin load, and the
plugin must survive being imported repeatedly. Not verified in a real
OrcaSlicer — the reproduction is the audit/refresh model in the tests.

**Not** a fix for the stale Config panel; that is a separate thing and the
mechanism is written up in `plugins/unlayered-infill/CHANGELOG.md` 0.4.2 and
in `docs/ORCA-PLUGIN-FACTS.md`, "Capability configuration".

---

## 2026-10-02 — updater 2.1.0: only one .bat left in the repository

> "All three iterations of the updater are still visible from the main
> page... can we just actually only show the one that we're using now that
> they're unified and then we can get rid of the old ones?"

Done. `Update-Orca-Plugins.bat` and `Choose-Orca-Plugin-Version.bat` —
forwarders since 2.0.0 — are **deleted**. The repository now shows exactly
one updater file: `Orca-Plugins.bat`, which contains the menu, the build
picker, the OrcaSlicer folder picker, the remembered choices, the install
engine and self-update. `tests/test_installer.py` now **fails if a second
top-level .bat ever appears**, so the one-file state is a pinned contract.

**What happens to old copies already on disk** — checked before deleting:

* an old `Orca-Plugins.bat` launcher (≤ 1.0.1) self-updates straight into
  the unified file on its next run; that URL is unchanged;
* an old `Update-Orca-Plugins.bat` still installs the latest plugins from
  `main` exactly as before — its self-update check 404s and skips
  harmlessly. It is frozen, not broken; swap it for `Orca-Plugins.bat`
  when convenient;
* an old `Choose-Orca-Plugin-Version.bat` forwarder fetches
  `Orca-Plugins.bat` from `main`, which exists — it keeps working.

Older test branches still carry their own copies of the old files; that is
expected and harmless (branch isolation means nothing borrows from them).

---

## 2026-10-02 — updater 2.0.1: testing from a branch, without merging

> "Can we do this without merging since the updater is able to pull from
> branches?"

Yes. Every file is served at `.../<branch>/...` on raw.githubusercontent
(verified for the slashed session-branch name), the branch picker lists the
branch live from the GitHub API, and self-update follows the remembered
build — so the unified updater can be pulled from `arena/01a0fd3b-tests` and
used for real before PR #7 is merged.

That flow exposed one bug, fixed here: self-update treated **any** different
version as newer. With the unified file on a test branch and `main` still
holding the old two-file launcher, a run whose remembered build was `main`
would fetch main's 1.0.1 launcher, see "1.0.1 != 2.0.0", and hand the run
*back* to the old launcher — a downgrade that made the new file look like it
never took. `:self_update` now only hands over to a download that itself
carries the `rem UPDATER_VERSION` marker, i.e. another one-file updater;
anything else keeps this copy in charge and says so on screen. A numeric
"only if strictly newer" comparison was deliberately not attempted — batch
arithmetic on dotted versions is riskier than the marker check, and the
marker covers the real case. `tests/test_installer.py` pins the guard.

Worth knowing while testing: `main` and the branch currently ship the
**same** plugin versions (Wave 0.0.33, Unlayered 0.4.2, merged in PR #6).
Testing the branch tests the updater itself; the plugins it installs are
identical to main's until new plugin work lands on a branch.

---

## 2026-10-02 — updater 2.0.0: one file instead of three

> "I'd like these things to be more unified and concrete."

The chooser, the updater engine and the launcher are now **one file**.
`Orca-Plugins.bat` contains the menu, the build picker, the OrcaSlicer folder
picker, the remembered choices, the whole install engine and self-update.
Nothing else is needed, and nothing about what gets installed changed — same
catalogue, same all-or-nothing test-branch preflight, same sidecars.

**What the owner will notice:**

* The menu now has an item for each thing you used to need a different file
  for: **[2] Choose the build** (released `main` plus the five newest test
  branches, live from GitHub) and **[3] Choose which OrcaSlicer folder to
  install into** (the numbered OrcaSlicer version picker: nightly folders
  first, then the others).
* The remembered build **and** the remembered folder are both shown at the
  top of the menu, and a remembered folder that still exists is now used
  without re-asking — that was the point of remembering it.
* **[4] Forget my remembered choices** clears both at once.

**Why the old two filenames are still in the repository:** copies already
sitting in Downloads folders keep checking those exact URLs. Both are now
short forwarders that hand the run to `Orca-Plugins.bat`. The
`Update-Orca-Plugins.bat` forwarder deliberately keeps the version marker
lines and size the old two-file copies verified, so an old updater (≤ 1.4.0)
hands its run over cleanly and an old launcher (≤ 1.0.1) still accepts it as
its engine — nobody is stranded on a version that silently stops updating.

**One version number.** The launcher (1.0.1) and the engine (1.4.0) became
one file, so there is one version: 2.0.0. `tests/test_installer.py` enforces
that every marker in every file agrees, and re-verifies the whole handover
chain the old copies perform.

Not verified on Windows (no Windows in the sandbox): the batch flow was
re-checked by re-reading and by the static analysis in the installer test —
labels, parentheses, quote toggling, download verification, CRLF — but the
first real double-click is the real test, as always.

---

## 2026-10-02 — wave-overhangs 0.0.33, unlayered-infill 0.4.2: the settings repair themselves

> "I know there's features — especially for Unlayered Infill, where I should
> have the ability to change the shape and pattern — but that doesn't show up
> in the config in Orca."

The same report as the entry below, after the fix below had shipped. This
time it was investigated against OrcaSlicer's own documentation instead of
being reasoned about, and the answer was in the wiki all along.

**The facts, now recorded in `docs/ORCA-PLUGIN-FACTS.md` under "Capability
configuration":**

* A capability's settings are stored **globally**, in
  `data_dir()/orca_plugins/config.json`, keyed by capability identity — *not*
  in the process preset. A preset may additionally hold an override, which
  wins while it is present.
* `get_default_config()` is consulted when there is nothing saved and when the
  user presses **Restore defaults**. A saved copy is shown as-is otherwise,
  which is why settings added by a later release stay invisible.
* `get_config_version()` reports which plugin version last saved the config,
  `save_config()` writes a new one, and `migrate_config_if_needed()` is the
  documented hook for migrating an older schema.

**So the previous claim that this "cannot be fixed in code" was wrong**, and
both plugins now fix it. `_migrate_config()` merges the running build's
settings into an older saved configuration — keeping every value the user set,
adding new settings at their defaults, refreshing the notes, and preserving
keys it does not recognise — then writes it back. It is wired to
`migrate_config_if_needed()` and, because *when* the host calls that hook is
documented only by example, also to the first capability call of a session.
It can never break a slice: a refused save, a junk config or a raising host
are all swallowed.

The Check setup text in both plugins was rewritten. The manual fallback is now
Orca's documented one — Plugins dialog → *Config* → **Restore defaults** — and
the preset-reselect recipe is gone.

Also in this pass, a documentation reconciliation: the README called this a
home of "three" plugins when two ship, `AGENTS.md`'s repo map still described
`Update-Orca-Plugins.bat` as the file to download, `MEMORY.md` and
`docs/ROADMAP.md` carried Wave version numbers from 0.0.23/0.0.25 days, and
the Unlayered Infill source pointed at four paths that do not exist in this
repository. All corrected.

Verified here: the full test suite, including new assertions that a simulated
0.3.4 config gains `pattern`/`shape`/`wave_angle`/`layer_phase`/`max_lift_mm`
while keeping the user's own amplitude, that an already-current config is not
rewritten, and that a refused save still exports. **Not verified:** anything
on a real OrcaSlicer — in particular whether the host calls
`migrate_config_if_needed()`, and when. The per-session fallback exists
because of that uncertainty.

## 2026-10-02 — wave-overhangs 0.0.32, unlayered-infill 0.4.1: the settings you could not see

> "I'm not seeing all of those new config options for unlayered infill."

**Correction, same day:** the explanation in this entry — that OrcaSlicer
caches the settings in your process preset — was a **guess, and it was wrong**.
The real cause was that `Update-Orca-Plugins.bat` downloads from `main` by
default, and `main` was still on Unlayered Infill 0.3.4, which genuinely has
only 9 settings and no `pattern`, `shape` or `wave_angle` at all. The updater
reported success, so it looked like an update had happened. PR #5 merges the
new versions into `main`. The Check setup text added below is still useful —
it now reports the installed version and setting count, which is what makes
this diagnosable — but it was not the fix.

**Not a bug in the plugin, and not a missing feature.** All 14 Unlayered
Infill settings exist in the shipped file, including the four added in 0.4.0
(`pattern`, `wave_angle`, `shape`, `layer_phase`), and `get_default_config()`
hands every one of them to OrcaSlicer with its plain-English note attached.
`tests/test_plugin_runtime.py` has asserted that since 0.4.0.

The cause is on Orca's side: **it only asks once.** When you first select a
plugin, Orca copies its settings into your process preset, and from then on
the Settings panel shows you that saved copy rather than asking the plugin
again. A preset saved under 0.3.4 therefore keeps 0.3.4's ten settings
forever. A plugin cannot push new keys into an already-saved preset.

Exports were never affected — `_cfg()` starts from the plugin's own
`_DEFAULTS` and overlays whatever the preset stored, so a missing key just
uses its default. The settings were working; they were only invisible.

Since the plugin cannot fix this, it now explains it. Both plugins' **Check
setup** capabilities print a "not seeing all the settings?" section stating
how many settings the installed build has and the three steps that refresh
the saved copy (Slicing Pipeline Plugin → None → back → save the preset),
followed by what to do if they are *still* missing (stale installed file: run
`Orca-Plugins.bat`, then fully quit and reopen Orca). It prints even with the
long settings guide switched off, since that is precisely when it is needed.
Guarded by new assertions in `tests/test_plugin_runtime.py`.

No change to exported G-code from either plugin.

### GitHub reported this as a G-code project

The repository's language bar read **G-code 77.1%**, Python 12.1%, PowerShell
9.4%, Batchfile 1.4%, because GitHub Linguist counted the archived test-print
exports and the test fixture as hand-written source. They are machine output
from OrcaSlicer, kept as evidence so past measurements can be re-checked.

`.gitattributes` now marks them `linguist-generated` / `linguist-vendored`, so
the bar reflects the code actually written here (Python, then PowerShell for
`keyboard-lighting/`, then Batchfile). Nothing was deleted and nothing moved —
the archive is unchanged, and `linguist-generated` has the welcome side effect
of collapsing a 91,000-line file in diffs.

## 2026-10-02 — Wave Overhangs 0.0.31: the micro-move pile-up around holes

**Also fixed, caught while committing:** `* text=auto` in `.gitattributes`
had silently rewritten `archive/test-prints/Cube_39m10s.gcode` from CRLF to
LF — all 91,355 lines of it — when the file was re-added during a sandbox
rollback. These are evidence files kept so measurements can be re-checked, so
they must stay byte-for-byte as they arrived off the owner's machine. The file
was restored from the owner's original upload commit and verified
byte-identical, and `archive/test-prints/** -text` now stops git normalizing
anything in that folder. `tests/fixtures/` is deliberately left on
`text=auto`, and the `.bat` rule is unchanged.

Owner report: *"around the whole there an absurd number of extremely tiny
moves ... randomly you have an absurd number of lines just to do a tiny chunk
of curve next to the hole"*.

Measured on their export: **60.7% of all wave moves were under 0.1 mm, and
together they carried 0.9% of the distance printed.** Median wave move
0.015 mm.

Two causes, both in the path cleanup:

1. `_clean_guards` computed its stray margin as `tolerance * 0.4`.
   `_simplify_attempts` walks a ladder of ever-smaller tolerances looking for
   a simplification that does not cut into a hole — but the shrinking
   tolerance shrank the margin with it, so the guard tightened at every rung
   instead of relaxing. Fronts wrapping a hole failed all three rungs and
   `_clean_wave_polyline` fell back to `list(original.coords)`: every point
   shapely's buffer produced. The margin is now the fixed constant
   `_MAX_STRAY_MM = 0.02`.
2. Nothing collapsed coincident points. Douglas-Peucker keeps a vertex
   whenever it lies far from the chord, so at a cusp it retains two vertices
   microns apart. New `_thin_points()` merges anything closer than
   `_MIN_POINT_GAP_MM = 0.02`, and new `_thinned_fallback()` applies it to
   fronts that still cannot be simplified at all, re-checking against the
   same hole guard before accepting.

Geometry is unchanged — that is the whole point. On the fixture the wave path
is **513.5 mm before and after**, while moves drop 508 → 439 and sub-0.1 mm
moves drop 91 → 22. With `wall_snap=False`, 739 → 455. With arcs on, 329 →
260. Those four golden counts in `tests/test_wave_gcode.py` were updated, and
a new assertion fails if sub-0.1 mm moves exceed 8% of the total or if the
path length moves by more than 1 mm.

The owner's part should improve considerably more than the fixture, since it
has holes and the fixture barely does — but that is a projection, not a
measurement.

---

## 2026-10-02 — Wave 0.0.30, and a tidy-up of the whole repository

Two owner requests in one pass: *"make the speed whatever i set the bridge
speed (10) to in orca, same as any other bridge"*, and *"organize the repo,
archiving old files and info or test prints and gcode"*.

### Wave Overhangs 0.0.29 → 0.0.30

`print_speed` now **defaults** to `"orca"`. In 0.0.29 following the profile's
bridge speed was opt-in; now it is simply the behaviour, so waves print at the
same speed as any other bridge on the part. A number in `print_speed` still
overrides it, and a section with no readable feedrate still falls back to
2 mm/s.

With the owner's bridge speed of 10 mm/s their part is predicted to drop from
**103.9 min to 24.5 min**. (2 mm/s → 71.5, 5 → 36.2, 10 → 24.5, 20 → 18.6.)
Arithmetic on the G-code; not verified on hardware.

The in-Orca note for the setting was rewritten accordingly: it now explains
that this is the dial to turn *down* if an overhang droops, and why a wave can
need to be slower than a bridge at the same setting — a bridge is anchored at
both ends and held up by tension, a wave line is cantilevered into open air at
one end.

### Repository tidy-up

The repo root had accumulated 3.6 MB of loose `.gcode`, `.stl`, `.3mf` and log
files. They are now in **`archive/test-prints/`**, moved with `git mv` so the
history follows them, with a README explaining what each file is and what was
measured from it. Nothing was deleted.

The root is now only: the three `.bat` files, `plugins.json`, the four
Markdown documents, and the `plugins/ tools/ tests/ docs/ archive/
keyboard-lighting/` folders.

* `archive/README.md` gained an index of what is in the archive and a section
  on `test-prints/`.
* `README.md` gained a **"Where everything lives"** table covering every
  top-level path.
* The two places in `MEMORY.md` that said these files were "in the repo root"
  now point at `archive/test-prints/`.

No code moved and no test reads any of these files, so the suite is unaffected.
The fixture the tests *do* use, `tests/fixtures/Cube^2_3m53s.gcode`, has not
moved.

---

## 2026-10-02 — Wave Overhangs 0.0.29: follow Orca's own bridge speed

**`plugins/wave-overhangs/`:** 0.0.28 → 0.0.29. Owner request, straight after
0.0.28: *"Can we not make it take the speed used for bridges that I already
have in Orca"*.

`print_speed` now accepts `"orca"` (also `"bridge"` / `"auto"`) as well as a
number. The plugin reads the feedrate off the bridge moves it is replacing,
**per section**, so it follows whatever the user's profile says without
needing to know anything about their printer. This only became possible
because 0.0.28 added modal-feedrate tracking to the parser. Junk values and
sections with no feedrate fall back to the safe 2.0 default.

Measured on the owner's export, whose bridge speed is F1200 = 20 mm/s:

| setting | predicted total |
|---|---|
| 0.0.27 (with the feedrate bug) | 103.9 min |
| 0.0.28 (bug fixed, `print_speed` 2.0) | 71.5 min |
| `print_speed = 5` | 36.2 min |
| `print_speed = 8` | 27.4 min |
| `print_speed = "orca"` (20 mm/s) | 18.6 min |

**The default stays 2.0.** Orca's bridge speed is tuned for a strand anchored
at both ends, where tension holds it up while it cools; a wave line is
cantilevered into open air at one end only. A ten-times jump is a genuine
droop risk, so the setting's in-Orca note and the changelog both tell the user
to walk up through 5 and 8 first. These are predictions from move-by-move
arithmetic, not from a physical print.

`tests/test_wave_gcode.py` gains coverage that `"orca"` uses only feedrates
the export actually contains (the fixture has two: F1200 and F420), never
falls back to F120, that the default is still F120, and that an unparseable
value falls back rather than crashing.

---

## 2026-10-02 — Wave Overhangs 0.0.28: a third of the print time, recovered

**`plugins/wave-overhangs/`:** 0.0.27 → 0.0.28. Diagnosed from the owner's own
export, which they uploaded to this branch along with the part and the
OrcaSlicer debug log.

The owner reported that a print looked wrong in the OrcaSlicer preview legend:
1h46m total, of which **36m49s was "Travel"** — 34.6% of the print, at an
average travel speed of 5.8 mm/s when travel should run at 120 mm/s. They also
asked why the gram usage looked high.

**The travel figure was real, and it was our bug.** G-code feedrates are
modal: the last `F` stays in force until something changes it. The plugin
printed its waves at `print_speed` (2 mm/s by default, `F120`) and then ended
the block restoring the fan but **not the feedrate**, and the moves it writes
to replace covered bridge extrusions carried no `F` of their own. Those moves
therefore inherited 2 mm/s.

Measured by walking every move in the owner's `test print_19m50s.gcode`:

| | moves | distance | time | speed |
|---|---|---|---|---|
| Wave fill printing | 12,114 | 7.06 m | 58.8 min | 2.0 mm/s |
| **Stranded on the wave speed** | **373** | **3.95 m** | **32.9 min** | **2.0 mm/s** |
| Normal Orca moves | 7,588 | 22.91 m | 11.6 min | 33.1 mm/s |
| Travel inside wave blocks | 277 | 4.66 m | 0.6 min | 120 mm/s |

That totals 104.0 minutes, which matches the 1h46m the legend reported. The
stranded row is the defect: 3.95 m that should take 30 seconds took 32.9
minutes. All 342 replacement moves in that file carried no feedrate.

Every move the plugin writes now states its feedrate, and the original speed
is handed back before untouched moves resume. The toolpaths and the extrusion
are unchanged, so there is no quality trade-off.

**The grams were a false alarm**, and worth recording so it is not chased
again. Total extrusion in that file is 7.62 g, of which the wave blocks are
1.50 g — 20%. The wave fill is categorised as Bridge/Internal Bridge in the
legend, so those rows dominate the *time* column and read as if they dominate
material. They do not.

**`tests/test_wave_gcode.py`:** new regression guard that walks the modal
feedrate through a generated export and fails if any move outside a wave block
is running on a feedrate set inside one. Verified to fail on the old code
(`move after a Wave block inherited the Wave print speed (F120)`) and pass on
the new. Its existing "retained extrusion must be preceded by a travel" checks
now ignore feedrate-only `G1 F…` commands, which set a speed but move nothing.

Not verified on hardware: no physical print has been run with 0.0.28.

---

## 2026-10-02 — Launcher 1.0.1: the FINDSTR error on startup

**`Orca-Plugins.bat`:** 1.0.0 → 1.0.1. Reported from a real Windows run.

Every launch printed this before the menu:

```
Checking for a newer version of this launcher...
C:\Users\...\Temp\orca_frontdoor_31982.bat:set "FRONTDOOR_VERSION=1.0.0"
FINDSTR: Cannot open >nul
FINDSTR: Cannot open 2>nul
```

Harmless, but alarming, and it leaked an internal line onto the screen.

*The cause.* One of the three checks that verify a downloaded launcher is
really a launcher was written as:

```bat
findstr /b /c:"set \"FRONTDOOR_VERSION=" "%NEWBAT%" >nul 2>nul
```

`cmd.exe` decides what is a redirection by toggling a quoting flag on **every**
`"` it meets. It has no concept of `\"` as an escape — that is a C convention,
not a cmd one. The line therefore holds five quotes, so cmd reaches the end of
it still believing it is inside a quoted string, and `>nul` and `2>nul` are
passed to findstr as two more **filenames** instead of redirecting the output.
findstr cannot open them and says so, and because the output was never
redirected, the matched line is printed too. With more than one file argument
findstr also prefixes matches with the filename, which is the
`C:\...\orca_frontdoor_31982.bat:` part.

*The fix.* Search for `FRONTDOOR_VERSION=`, which needs no embedded quote and
is just as specific — it appears on exactly one line of the launcher. The line
now has four quotes and the redirections work.

The verification itself was never broken: findstr still matched, still returned
success, and a 404 page or a wifi login portal would still have failed all
three checks and been rejected. This was noise, not a hole.

*Why the version number moved.* A launcher already on disk decides whether to
hand over to a download by comparing the `rem FRONTDOOR_VERSION ... end`
marker. If the number does not change, every existing copy sees "same version"
and skips the hand-over — so a fix that does not bump the version reaches
nobody. `tests/test_installer.py` now parses both markers and fails if they
disagree, rather than hardcoding the number.

*The guard.* cmd.exe cannot run in this sandbox, so this could only ever have
been caught by reading. The installer test now walks every line of all three
.bat files the way cmd.exe does — toggling on each quote — and fails if a
redirection or pipe ends up inside an unclosed quote. Re-introducing the old
line makes it fail, which was checked. The only other odd-quote line in the
repo is `set "VAR=%VAR:"=%"`, the standard quote-stripping idiom, which has no
redirection after it and is correctly left alone.

---

## 2026-10-02 — Two plugins instead of three, and a wave with a shape

**Unlayered Infill:** 0.4.0. **Archived:** Wave Overhangs Geometry 0.1.4.
**Wave Overhangs:** unchanged at 0.0.27.

*Three plugins was one too many.* `Wave Overhangs Geometry` was always a
prototype: a second Wave plugin that edited Orca's geometry mid-slice, at
`posPrepareInfill`, so the waves would appear in the normal 3D preview rather
than only in a reopened export. It never completed a verified real-Orca slice,
never mind a physical print, and Orca's current bindings expose existing
extrusion paths read-only, so it could only hand Orca bridge-tagged fill
surfaces and hope. Meanwhile it cost a version bump, a test, a catalogue row, a
fallback row in the .bat and a paragraph of explanation in every document,
every single release.

It now lives in `archive/`, with its README, its changelog and its regression
test, which still passes from its new home. `archive/README.md` says why it was
archived, what was unfinished, and the exact steps to bring it back. The
catalogue, the launcher's hardcoded fallback plan, `tools/sync_changelog.py`
and the two plugin tests no longer mention it. Removing the fallback row meant
promoting the next row from `>>` to `>`, since the first row is the one that
creates the plan file — the kind of detail that only bites on a machine with no
network, which is exactly when the fallback is used.

*The wave got a shape.* Unlayered Infill rode `sin(frequency x)` and nothing
else, which quietly did half a job: a ripple that varies along X alone leaves
an infill line running along Y at one constant height for its whole length.
Lifted, not waved; it keys into nothing. On 0/90-degree infill that is half the
infill doing no work.

0.4.0 adds five controls. `pattern=cross` makes the wave an egg-crate that
ripples along both axes, so a line in any direction still rises and falls.
`wave_angle` aims the ripples across the infill direction. `shape` picks sine,
triangle (straight flanks, sharper keying) or square — a saturated sine with
flat crests, never a vertical Z step, because a printer cannot move Z
instantly. `layer_phase` advances the wave a little each layer so crests walk
sideways instead of stacking into a column. `max_lift_mm` is a hard ceiling in
millimetres on the Z movement, which matters because the amplitude default is a
share of layer height and will happily grow when you change layer height.

Every one of them defaults to the old behaviour, and
`tests/test_unlayered_waves.py` proves it by diffing real generated G-code
rather than by reading the defaults — an upgrade must not change anyone's
output. The same test pins the premise (the old wave really is flat along a
Y-running line), that every shape stays inside the requested amplitude, that
the square shape never steps hard enough to be unprintable, and that the export
is still idempotent with all five controls on at once.

*And the settings explain themselves.* Unlayered Infill now does what Wave
Overhangs got in 0.0.27: a plain-English note above every setting in the config
panel, and the full guide printed by **Unlayered Infill - Check setup**, with a
`settings_guide` toggle to silence it. One honest difference from Wave's
version — Orca only lets a capability read its own config, so the guide prints
the defaults and says plainly that it cannot see what you changed on the main
item, instead of printing a default and calling it your value.

Not verified: no real OrcaSlicer ran here, and no part was printed. The new
wave shapes are geometry that has been tested as G-code, not as plastic.

---

## 2026-10-01 — One launcher, and settings that explain themselves

**Wave Overhangs:** 0.0.27. **New:** `Orca-Plugins.bat`, the single entry point.

*One file instead of two.* The repo shipped a "chooser" and an "updater" and
the split was never explicable -- which one do you double-click, and why are
there two? There is now one file, `Orca-Plugins.bat`. It shows the remembered
build and OrcaSlicer folder, installs on Enter, and offers changing the
version or forgetting the remembered choices. It self-updates using the same
pattern the installer already used: fetch to temp, verify the download is
really the launcher, hand the run over, never rewrite the running file.

Both old filenames keep working, for different reasons.
`Choose-Orca-Plugin-Version.bat` becomes a thin forwarder so shortcuts
survive. `Update-Orca-Plugins.bat` keeps its exact name and URL because
copies already on disk poll that address for their own self-update; renaming
it would have silently stranded every one of them. It is the install engine
now, still usable standalone.

*Settings you can read where you use them.* The script capability is now
**Wave Overhangs - Settings guide & check**. After the usual diagnostics it
prints every setting with its explanation, the value actually in force, and a
marker on anything changed from the default -- wrapped to 72 columns because
Orca shows it in a plain message box that clips rather than reflows. A
`settings_guide` toggle silences it. This closes the gap behind the owner's
complaint that the only documentation was a README they had to go and find.

Renaming a capability changes its identity in Orca and can detach it from a
process preset that already selected it, so the test that pins those names
was updated deliberately rather than loosened, and the changelog says to
re-pick it.

Because cmd.exe cannot run in this environment, the .bat files also gained a
static check that every `goto` and `call` target actually exists -- a dead
label would otherwise only ever show up on the user's machine.

---

## 2026-10-01 — Updater remembers your choices; a GEOS crash fixed

**Wave Overhangs:** 0.0.26. **Chooser and updater:** remember last selection.

Three things the owner asked for, with one honest non-result.

*Arc moves for file size.* Measured rather than assumed, and the answer is to
do the opposite. Wave's own arcs now save **9.8 KB, 0.55% of the file**,
because 0.0.21-0.0.23 already shrank the wave blocks from 33.5% of the export
to 3.4%. Meanwhile OrcaSlicer's own arc fitting encodes **37% of the printed
path length** in that same file -- 12,546 arcs covering 30.3 m of 82.7 m.
Turning Orca's arc fitting off to avoid the crash would add roughly 86,000 G1
moves, about 2.6 MB, more than doubling the file. The right setting is
therefore Orca's arc fitting ON and Wave's `arc_fitting` off, which is exactly
the pair that avoids issue #7433.

*Remembering updater choices.* The chooser stored the last branch but still
made you select it; Enter on its own now reuses it. The updater now remembers
the OrcaSlicer data folder too, under
`%LOCALAPPDATA%\OrcaPluginUpdater\datadir.txt`, and offers it on Enter. Both
keep the full menu so switching is unchanged. Both .bat files stay CRLF
throughout.

*Blending the wave back together behind a hole.* Implemented as `wake_blend`,
and shipped **off by default because it is not good enough**. A morphological
closing of the reached region rounds the crease, and on a simple round hole it
replaces the sharp V with smooth curves. On the owner's real part it also
loses about 4% of wave coverage (1911 mm of path down to 1833 mm) and turns 8
tiny fragments into 40: healing makes consecutive fronts partly coincide, and
the "already reached" subtraction then cuts them into dashes. Two fixes were
tried -- propagating the raw region instead of the healed one, and measuring
the subtraction against the raw region -- and neither removed the dashes. The
remaining idea is in `docs/ROADMAP.md`. With `wake_blend` at 0 the output is
byte-identical to 0.0.25 on all five test shapes including the owner's export.

*Making the settings readable.* Orca presents a plugin's config as JSON, and
JSON has no comments, so the careful explanations in the plugin source never
reached the person actually editing the values -- they saw 33 bare keys. Every
setting now carries a plain-English note immediately above it, shipped as
`_`-prefixed keys. `_cfg()` already ignored unknown keys, so notes cannot
become settings, cannot be typo'd into one, and can be deleted with no effect;
a test asserts the panel round-trips to exactly `_DEFAULTS` both with and
without them, and that no setting is left unexplained.

This also fixes a documentation failure from the arc-size work: the owner went
looking for "arc fitting" in the plugin and could not find it, because Wave's
own `arc_fitting` was already false and the setting that actually mattered was
OrcaSlicer's, in Print Settings > Quality > Precision. The note says so.

Chasing that did find a real latent crash, now fixed: clipping one boundary
against another can leave a single-point line, and shapely's `linemerge`
raises `GEOSException` on it. `GEOSException` is not a `ValueError`, so the
existing handler could not catch it and the whole layer was lost. This is the
same fault that killed an earlier optimisation attempt. `wave_tracks` now
retries without the crumbs, only after the normal merge has already failed.
Covered by a test that injects the exception and is verified to fail when the
narrow handler is put back.

Not verified in real OrcaSlicer.

---

## 2026-10-01 — The export hang is an OrcaSlicer bug (#7433)

**Wave Overhangs:** 0.0.25.

The owner narrowed it down: the failed export only happens when **Arc fitting**
is on in OrcaSlicer. With the `auto` default that 0.0.21-0.0.23 shipped, that
is precisely when Wave wrote G2/G3 into the finished file.

That matches a known, unfixed OrcaSlicer bug:
[issue #7433](https://github.com/OrcaSlicer/OrcaSlicer/issues/7433), "Post
processing script results in corrupted gcode / crash when previewing model".
Opened November 2024 against Orca 2.2.0, reproduced by the reporter in nightly
in July 2025, and confirmed still present in 2.3.2 in January 2026. The
reporter's trigger was ArcWelder, a post-processor that does exactly what Wave
was doing: replacing straight moves with arcs. Orca crashes or renders corrupt
G-code when it re-reads post-processed output containing arcs.

So the 0.0.24 default (arcs off) is the fix, and it stays. **Check setup** now
prints the arc setting and, when arcs are on, warns with the issue number. The
plugin README carries the same warning.

The arcs Wave emits were audited and are not the problem: 118 arcs across every
fixture and the owner's export, radii 0.78-12.1 mm, sweeps 11-149 degrees, no
major arcs, no near-full circles, no chord longer than the diameter, no
reversed directions, all with positive extrusion.

Auditing that output did turn up a real defect, fixed here: the emitter decided
whether to write a move from the *unrounded* step length but wrote coordinates
to three decimals, so sub-micron steps became moves whose X/Y matched the
previous line exactly -- dead lines, usually `E0.00000` too. The owner's export
contained 542. The emitter now tracks the position it has actually written and
rolls any skipped extrusion into the next real move; Wave moves in that export
fall from 2,006 to 1,910 with no material lost. Arc I/J offsets are now
measured from the last written coordinate rather than the unrounded point,
improving worst-case arc radius consistency to 0.0013 mm.

Tests: a direct emitter test drives a front built from sub-micron steps and
asserts no written move repeats the previous coordinate, with the skipped
material accounted for. It was verified to fail when the fix is removed -- the
whole-file fixtures do not contain sub-micron steps, so a file-level assertion
alone would have been vacuous.

Not verified in real OrcaSlicer.

---

## 2026-10-01 — Wave can no longer hang an export

**Wave Overhangs:** 0.0.24.

The owner reported that OrcaSlicer would no longer export at all: it sat on
"exporting" and crashed about a minute later. Their last good print came out of
0.0.20, and 0.0.21, 0.0.22 and 0.0.23 had all shipped since, so the cause was
somewhere in those three.

**The crash could not be reproduced here, and this entry does not claim to have
found it.** What was measured: the arc fitter is linear and costs under 0.1 s
for an 800-point front; a synthetic part that overhangs on every layer costs
0.08-0.18 s per bridge layer with peak memory of 49 MB and no error; the
owner's own export reconstructed runs in 1.65 s. None of that explains a crash.
Every arc the plugin emits was also re-validated across all four test fixtures
and the owner's export -- 215 arcs, zero zero-radius, zero full-circle, zero
mismatched endpoints, every one carrying positive extrusion.

So rather than guess, two changes make the failure mode impossible:

A wall-clock ceiling, `time_budget`, defaulting to 30 seconds, now wraps the
whole G-code pass. It is checked before every layer and every bridge section.
When it fires the pass gives up and returns the file byte-for-byte as
OrcaSlicer wrote it, without the Wave stamp, so nothing is half-done and a
later run can try again; the stats record `timed_out` and how far it got, and
the message shown in Orca says what happened instead of reporting that nothing
was found. `0` disables the ceiling and a nonsense value falls back to 30.
Whatever the real cause turns out to be, the plugin can no longer be the reason
an export does not finish.

`arc_fitting` now defaults to `false` rather than `auto`. G2/G3 is the one
genuinely new kind of output introduced since the owner's last good print, and
OrcaSlicer re-parses the finished file for its preview and time estimate, which
makes it the best suspect available. The arcs are opt-in until a real export
confirms they are safe; `"auto"` restores the previous behaviour.

Tests: `tests/test_wave_gcode.py` gains a section covering the budget (fires,
changes nothing, does not stamp, `0`/junk handled) and now asserts the shipped
arc default is off even when the profile has arc fitting switched on.

Not verified in real OrcaSlicer.

---

## 2026-10-01 — Less work per export, and timings in the log

**Wave Overhangs:** 0.0.23.

The owner reported that exporting after a slice was taking far too long.
Measured on their own 1.75 MB export in the sandbox, the Wave pass takes about
two seconds, so the pass itself was never going to explain "forever" -- but it
was doing a great deal of pointless work, and the export it produced in 0.0.20
was doing a great deal to everything downstream.

Geometry is now built only for layers that have a Bridge section and the layer
that holds each one up: 7 layers out of 134 on their part, so 95% of the
shapely objects built were never used. Parsing dropped from 1.65 s to 0.2 s, an
export with no bridge at all now costs 0.01 s, footprints are buffered once per
line width instead of once per move, and cleanup builds its guard shapes once
per section rather than once per front.

The likely real cause of the slow export is upstream of all that: 0.0.20 wrote
**29,374 Wave moves, 0.89 MB, a third of the entire file**, because of the
simplification fault fixed in 0.0.21. The same input now yields 2,006 moves and
74 arcs in 0.07 MB, and the file drops from 2.66 MB to 1.82 MB. Orca re-reads
and re-estimates every move after post-processing, so that is where the waiting
was going.

The log now carries `seconds`, `parse_seconds`, `plan_seconds`,
`geometry_layers` and `layers_scanned` so the next slow export can be measured
rather than guessed at.

Also tried and rejected: simplifying the reachable region on each propagation
step to cap its vertex growth. It made GEOS throw on degenerate rings, the
plugin failed closed, and no waves were produced at all. Reverted.

---

## 2026-10-01 — The corner sliver, measured in the owner's own print

**Wave Overhangs:** 0.0.22.

The owner printed the part, photographed the first layer from below and
circled two things: a corner that was not filled, and a rounded wall whose
Wave edge was not smooth. They also uploaded the export, so both could be
measured rather than guessed at.

**The corner is real and is now fixed.** A wavefront is a contour of equal
distance from the supported edge, and those contours step outward one line
spacing at a time. Where the far boundary runs at an angle to that march --
the tip of a corner -- the last contour stops short. In their export that left
a 0.22 mm^2 void, 0.53 x 0.75 mm, in the corner of the plate. Wave now fills a
sliver like that with one short path down its middle, and only ever adds
material where there is none. Short fronts that touch a rung already on the
plate are also kept now rather than discarded as specks.

**The rounded wall is not what it looks like.** Every Wave end along that
curve sits 0.456 to 0.457 mm from the wall -- a spread of 0.001 mm across 21
ends -- so the ends are already landing on the wall exactly as intended. The
staircase in the preview is the flat end of each rung meeting a curve at
0.35 mm intervals. Smoothing it needs a rung laid along the wall with the
others trimmed back to make room. That was built, measured, found to make the
edge worse, and left out. It is written up in the roadmap instead of shipped.

Not verified on hardware beyond the owner's own photograph of the 0.0.20
print, which is what prompted this release.

---

## 2026-10-01 — Wave speaks arcs, and stops bloating files around holes

**Wave Overhangs:** 0.0.21.

The owner asked whether the waves could be arc moves, since they can turn Arc
fitting on in their print profile. They can now, and the request uncovered two
problems worth more than the arcs themselves.

Wave runs after Orca has written the G-code, so Orca's arc fitter never sees
Wave's moves — and, in the other direction, Wave could not *read* the arcs
Orca writes. With arc fitting on, a round hole's wall is exported as G2/G3, so
Wave would have gone blind to that wall and quietly lost the 0.0.20 perimeter
fix on the very parts that need it. Wave now reads both the `I J` and `R`
forms, and emits its own arcs when the export says the profile wants them.

The second problem was file size. Cleanup refused to simplify any wavefront
that touched a hole, and 0.0.20 had just made the ends finish *on* hole walls,
so nearly every front fell back to its raw rasterised form — a 9.9 mm front
written as 980 moves instead of 9. On the synthetic part with a hole, the Wave
G-code dropped from 316 KB to 9 KB once touching stopped being treated as
crossing.

Measured in the sandbox on the captured Cube^2 export: with arcs on, 44 arcs
replace 180 straight moves (28% fewer commands, 20% fewer bytes), extrusion is
conserved to 0.07%, and against the original dense wavefront the arcs are more
accurate than the straight moves they replace (mean error 0.034 mm versus
0.119 mm). Not verified on real hardware: no OrcaSlicer and no printer here.

---

## 2026-10-01 — Wave ends snap to the real wall and hole perimeters

**Wave Overhangs:** 0.0.20.

The owner reported that Wave ends would not snap to the overhang perimeter:
instead of marching from the supported perimeter all the way out to the
overhang perimeter and around holes, the fronts finished on a jagged edge that
the following outer perimeter then had to print against.

The cause was that Wave measured the overhang from the footprint of Orca's
exported bridge *lines*. The union of those line footprints has a castellated
edge — alternating in and out by about half a line width — that also stops
short of the wall, and fronts were being clipped to it. Wave now also reads the
layer's wall moves, squares the overhang area up against the real wall bead
(overlapping into it by 25% of the Wave width by default), and lets the fronts
reach that smooth boundary. `wall_snap=false` restores the 0.0.19 behaviour for
comparison.

A second, related bug was fixed: wall material was measured one G-code move at
a time, which left a hairline slit at every vertex of a curved wall, and a Wave
end could slip through one and finish on the visible surface of a hole.

Verified in the sandbox against the captured Cube^2 export (ends along each
wall now lie on one line within 0.02 mm, where 0.0.19 varied by 0.29 mm) and
against a new synthetic overhang-with-hole export (all ends around the hole on
one radius within 0.001 mm, nothing inside the hole, nothing outside the part).
Not verified on real hardware: no OrcaSlicer and no printer in the sandbox.

---

## 2026-10-01 — Wave endpoint taper no longer adds default micro-moves

**Wave Overhangs:** 0.0.19.

Changed the default endpoint taper so it lowers E on existing straight Wave moves
instead of inserting many tiny endpoint subdivision moves. Endpoint snapping now
extends along the Wave's own endpoint direction until it reaches the wall or
hole boundary, rather than jumping sideways to the nearest boundary point. This
keeps the snap-to-perimeter behavior from 0.0.18 while avoiding the rectangular
or grid-like endpoint texture seen in preview.

## 2026-10-01 — Wave endpoints snap to perimeter by default

**Wave Overhangs:** 0.0.18.

Changed the default edge cleanup after visual feedback showed that centerline
inset/clearance could leave visible gaps at the wall and hole perimeters. The
normal post-processing output now snaps Wave endpoints back onto nearby
non-support detail boundaries, then applies endpoint flow taper to reduce blobs
while still conforming to the same visible perimeters as Orca's default bridge
infill. `edge_clearance` remains available as an explicit comparison/debug
option but defaults to `0`.

## 2026-10-01 — Wave edge clearance before taper

**Wave Overhangs:** 0.0.17.

Added an edge-clearance stage to the original post-processing Wave plugin.
Emitted Wave centerlines are now clipped back from non-support detail boundaries
such as outer overhang walls, holes, and concave edges before endpoint taper is
applied. Bridge-removal coverage still uses the untrimmed cleaned Wave paths, so
old straight Bridge fragments do not reappear at the edge. `edge_clearance="auto"`
follows the exported bridge width; `edge_clearance=0` disables the inset for
comparison.

## 2026-10-01 — Wave endpoint flow taper

**Wave Overhangs:** 0.0.16.

Kept the original post-processing Wave path as the active direction and added
Arachne-like endpoint flow taper to the G-code emitter. Wave centerlines and
covered-bridge subtraction stay the same, but endpoints touching outer walls,
holes, or concave detail boundaries are split into short moves with reduced E
near the boundary. The support-side anchor boundary is excluded so the first
Wave rung still prints at full flow. The plugin-storage log/state behavior from
0.0.15 remains in place to avoid routine approval prompts.

## 2026-10-01 — Wave Geometry fill-surface bridge method

**Wave Overhangs Geometry:** 0.1.4.

Changed the preview-visible geometry experiment from `posSlice` slice-island
mutation to `posPrepareInfill` fill-surface mutation. Orca now keeps the
original perimeter it already generated, while the plugin replaces unsupported
prepared fill with `stBottomBridge` Wave ribbons handed to Orca from the
supported edge outward. This is meant to address the preview problem where Wave
ribbons appeared as many dark-blue overhang-wall islands. The same Python API
limit remains: existing generated `ExtrusionPath` objects are read-only, so the
plugin still cannot promise final bridge/perimeter G-code ordering by itself.

## 2026-10-01 — Wave Overhangs Geometry preview prototype

**Wave Overhangs Geometry:** 0.1.3.

Added a separate `posSlice` geometry-stage prototype. It replaces reachable
unsupported slice area with obstacle-aware Wave ribbon geometry before Orca
creates perimeters and infill, so the intended result can appear in the normal
preview. The current Python bindings expose existing extrusion paths as
read-only, so this first version does not inject raw `ExtrusionPath` objects;
it documents that limitation and fails closed when live geometry mutation is
unavailable. Version 0.1.3 uses Orca's plugin storage for routine logs/state so
normal slicing should not ask for log-write approval. It also separates preview
roles: one non-bridge outer overhang-wall shell contains bridge-classified Wave
ribbons that are clipped inside that perimeter and handed to Orca from the
supported side outward.

**All plugin wrappers:** routine logs and setup-state JSON now default to Orca's
plugin storage folder instead of `Downloads`, avoiding normal audit prompts.
`ORCA_PLUGIN_LOG_DIR` remains only as an explicit debug override.

## 2026-10-01 — Wave controls and cleaner component order

**Wave Overhangs:** 0.0.15. **Updater:** 1.4.0.

Wave now exposes documented controls for propagation, order, cleanup, speed,
fan, and safety. The optional nearest component order reduces long same-distance
travel moves around holes without allowing farther fronts to print before nearer
ones. Safe defaults remain `auto` propagation, support-first components, and the
smart pattern. Endpoint cleanup now preserves interior curve points while
removing only short end stubs; the captured Cube fixture emits 399 smoothed
Wave moves.

## 2026-10-01 — Wave topology-safe diffraction

**Wave Overhangs:** 0.0.14. **Updater:** 1.3.0.

Wave propagation now treats holes entirely inside the unsupported plane as
obstacles, so fronts continue around both sides instead of only reacting to
holes on the supported boundary. Front simplification is also checked against
the unsupported-region boundary; curved fronts keep their original points when
a simplified chord would cross a hole or concave void. Internal-hole and
circular-hole regressions protect against the stray diagonal visible in the
comparison preview.

## 2026-10-01 — Wave handoff, cleanup, and pattern controls

**Wave Overhangs:** 0.0.13. **Updater:** 1.2.9.

Wave now makes an explicit non-extruding travel to each replaced bridge
segment's original endpoint before any retained original extrusion resumes.
This prevents a sharp handoff line from a Wave endpoint. Cleanup now removes
short complete Wave fronts and short endpoint stubs, while retaining substantial
uncovered bridge material. The captured fixture emitted 386 cleaned Wave moves.
New `smart`, `monotonic`, and `zigzag` patterns plus deterministic endpoint
policies are available; monotonic keeps a consistent front direction without
extruding between separate fronts. Uniform absolute-E bridge sections restore
`M82` and the prior command value safely; mixed E-mode sections fail closed.

## 2026-10-01 — repository organization and documentation cleanup

* Grouped the supplied OrcaSlicer reference PDFs under `docs/reference/` and
  the `Cube^2.STL` model plus captured G-code under `tests/fixtures/`.
* Removed the captured `orca-plugins.log` from version control and ignored
  runtime logs so a user's machine output cannot be mistaken for source.
* Reconciled the README, plugin guides, roadmap, facts, and handoff with the
  current permanent names, Wave 0.0.12 behavior, Unlayered Infill 0.3.4, and
  the tests that actually run in this repository.
* No plugin algorithm or installer behavior changed in this cleanup.

## 2026-10-01 — align and clean real Wave paths

**Wave Overhangs:** 0.0.12. **Updater:** 1.2.8.

The owner's visible Wave result exposed a 0.25 mm downward shift: Orca's layer
comment omitted the profile's `z_offset`, while the real bridge moves included
it. Wave now uses the bridge move's actual modal Z. Wavefronts are simplified,
sub-0.15 mm chatter is merged, line width follows Orca's real bridge width, and
isolated remnants shorter than half a line width are removed. The captured
cube drops from roughly 1,850 tiny Wave extrusion moves to 436 while preserving
25 substantial uncovered bridge fragments.

## 2026-10-01 — rebuild Wave as one transactional G-code pass

**Wave Overhangs:** 0.0.11. **Updater:** 1.2.7.

Research into the production WaveOverhangs forks confirmed that wave paths
intercept bridge residuals. The plugin now performs that interception directly
on Orca's exported `Bridge` and `Internal Bridge` toolpaths instead of passing
plans between two unreliable Orca callbacks. On the owner's real 0.30 mm cube
export, the offline regression creates three wave layers, removes only covered
bridge extrusion, retains 158 uncovered fragments, restores fan state, fails
closed, and is idempotent. Real-Orca export and physical-print verification are
still outstanding.

## 2026-10-01 — fix Wave's measured Orca integration failures

**Wave Overhangs:** 0.0.10. **Updater:** 1.2.6.

The owner's 0.30 mm `Cube^2.STL` export proved that Wave 0.0.9 planned four
layers but inserted zero wave blocks. Orca's internal `slice_z` values did not
match exported layer Z values, and its released Polygon binding rejected the
plugin's list constructor. Wave now keys plans by `print_z` and builds host
polygons through the supported empty constructor plus appended Points. Failure
still retains Orca's original bridge; replacement is not yet declared proven.

## 2026-10-01 — chooser uses the selected branch's updater

The branch chooser now downloads and validates `Update-Orca-Plugins.bat` from
exactly the selected branch and runs it from the Windows temporary folder. This
ensures a branch test includes its updater changes, not merely its plugin files.
A missing or invalid branch updater stops safely and never borrows `main`.
Neither downloaded batch file is overwritten while running.

## 2026-09-30 — final version-free plugin names

**Wave Overhangs:** 0.0.9. **Unlayered Infill:** 0.3.4.
**Updater:** 1.2.5.

* Both permanent package names are now simple and version-free: `Wave
  Overhangs` and `Unlayered Infill`. Capability names match and also remain
  version-free. Release numbers appear only in explicit version displays.
* Removed every compatibility-name special case from the catalogue, updater
  sidecars, tests and instructions.
* Pinned the complete Unlayered 0.3.0 control set as defaults so percentage,
  nozzle-grid, blending, frequency, segment length and full-strength controls
  do not depend on an old version-named configuration slot.
* This is the final identity migration. Reselect each capability once after
  installing; future releases will not rename either package.

## 2026-09-30 — restore Wave Overhangs' saved identity

**Wave Overhangs:** 0.0.8. **Updater:** 1.2.4.

* Restored `Wave Overhangs v0.0.6` as its permanent compatibility identity so
  Orca can reconnect to the pipeline selection/configuration saved before the
  unsuccessful 0.0.7 rename. The actual version remains visible separately.
* Updater output now prints both compatibility names explicitly: select
  `Wave Overhangs v0.0.6` and `Unlayered Infill v0.3.0`; check the Version
  column for the actual 0.0.8 / 0.3.3 releases.
* Replacement remains the chosen Wave behavior. Same-export, geometry-bounded
  bridge removal is still under development and is not claimed working here.

## 2026-09-30 — restore Unlayered Infill's working configuration identity

**Unlayered Infill:** 0.3.3. **Updater:** 1.2.3.

* Restored the permanent compatibility name `Unlayered Infill v0.3.0` so Orca
  reconnects to the configuration slot containing the owner's percentage
  amplitude, nozzle grid, blending, frequency and full-strength settings.
* Kept every current import-safety, logging and engine fix; this is not a code
  rollback to 0.3.0. The real version remains visible everywhere except the
  compatibility name.
* Wave Overhangs 0.0.7 was tested on real Orca and still did not replace the
  normal bridge in reopened exported G-code. The stable-name diagnosis is
  therefore disproven; Wave remains unresolved.

## 2026-09-30 — stable plugin identities, audited against Orca's PDF

**Plugins:** Wave Overhangs **0.0.7**, Unlayered Infill **0.3.2**.
**Updater:** 1.2.2.

* Both plugin names are now permanent and version-free. Orca's Plugin
  Development PDF says a preset's full capability reference contains the
  plugin name. Renaming the plugin every release could leave a preset pointing
  at yesterday's identity even while the new plugin appeared installed and
  activated. Versions remain visible in Orca's Version column, Check setup,
  logs, G-code stamps, standalone tool, and updater output.
* The updater sidecar now writes the same stable PEP 723 name and keeps the
  version only in `installed_version`.
* Added PDF-backed contract checks for PEP 723 dependency placement, one
  package class, typed capability bases, execute signatures, and capability
  registration. Wave Overhangs continues to declare numpy and shapely for
  Orca's bundled `uv` installer; it does not run `pip` itself.
* Recent changes continue to appear in the Plugins menu's Description tab and
  through Check setup. The dedicated Changelog tab cannot be populated by a
  side-loaded `.py`; Orca fills it from a cloud listing.
* After installing, reselect each pipeline capability once so Orca stores its
  corrected stable reference.

## 2026-09-30 — first Windows run fixes

**Updater:** 1.2.1. **Plugin versions unchanged:** Unlayered Infill 0.3.1,
Wave Overhangs 0.0.6.

The owner's first real Windows run found two failures that Linux static checks
could not expose:

* Windows PowerShell 5.1 kept GitHub's branch response as one nested
  `System.Object[]`. The chooser now enumerates the response directly and
  converts each commit URL to one string before requesting it.
* The 747-byte `plugins.json` catalogue was rejected by the 2,000-byte safety
  floor intended for large plugin files. Catalogue downloads now use a
  separate 100-byte floor and still have to parse as valid JSON before use.

The strict safety behavior itself worked: the updater reported the selected
branch, refused to borrow from `main`, and changed no Orca plugin files.
Regression checks pin both fixes. The corrected menu and successful install
still need a second Windows run.

## 2026-09-30 — choose and test any branch by double-clicking

**Updater:** 1.2.0. **Plugin versions unchanged:** Unlayered Infill 0.3.1,
Wave Overhangs 0.0.6.

* Added `Choose-Orca-Plugin-Version.bat`: a numbered menu of live GitHub
  branches, with released `main`, the five newest test branches, all branches,
  and manual entry. It remembers the chooser's last selection and has an
  obvious return to released `main`. No Command Prompt or token is needed.
* Test branches are now strict and all-or-nothing. The updater downloads and
  validates the catalogue and every plugin before changing Orca's folders. A
  missing test-branch file stops the install; it never silently borrows the
  released copy from `main`.
* Large start and finish banners show the selected branch and plugin versions.
  A plain double-click of `Update-Orca-Plugins.bat` still uses `main`.
* Extended `tests/test_installer.py` with static checks and a branch-preflight
  replay. **Not verified on Windows:** neither batch file can run in this
  sandbox; the first real double-click is still the real test.

## 2026-09-30 — released to `main`

Everything below this line was merged into `main` (PR #2), so
`Update-Orca-Plugins.bat` now installs **Unlayered Infill 0.3.1** and **Wave
Overhangs 0.0.6**. Until this merge the updater was still handing out
wave-overhangs 0.0.3 and unlayered-infill 0.2.0, which is why version numbers
appeared not to change.

Updaters older than **v1.1.0** cannot upgrade themselves. If you have one of
those, download `Update-Orca-Plugins.bat` once more; after that it keeps
itself current.

## 2026-09-30 — install fixes

**Plugins:** Unlayered Infill **0.3.1**, Wave Overhangs **0.0.6**.
**Updater:** 1.1.0.

Reported by the owner: the new versions would not install, and the updater
appeared to be missing.

* **Fixed a bug that can stop a plugin loading.** Both plugins wrote their
  first log line while OrcaSlicer was still importing them. Orca installs a
  file-activity watcher before it imports any plugin, so a write at that
  moment has no plugin identity attached to it; it can prompt the owner
  mid-install or fail the load. Logging now happens on first use instead.
  Guarded by a new test that imports each plugin under a watcher that refuses
  every write.
* **Fixed the updater staging a non-plugin file beside the plugin copies.**
  Orca requires exactly one `.py` per plugin folder. Standalone tools now go
  to `Downloads\OrcaPlugins\tools\`.
* **Ruled out** `requires-python = ">=3.12"` as a cause — OrcaSlicer reads
  that field but does not enforce it.
* **Documentation:** README gained "If a plugin will not install" (read the
  Diagnostics tab first) and "Installing a test build" (`PLUGIN_BRANCH`).
  `AGENTS.md` gained hard rules 13 and 14. `docs/ORCA-PLUGIN-FACTS.md` gained
  the corrected audit-hook timing, the on-disk layout, and where a load
  failure is reported.
* **Not verified:** whether either fix is what the owner actually hit. The
  decisive evidence is the Plugins dialog's Diagnostics tab.

## 2026-09-30 — tuning that matches the owner's own build

**Plugins:** Unlayered Infill **0.3.0**, Wave Overhangs **0.0.5**.

* Unlayered Infill amplitude became a **percentage of layer height** (default
  `"200%"`) and the grid became **one nozzle wide** (`"auto"`), matching the
  behaviour the owner had in their own version.
* Both plugins now write a plain-English log to
  `<Downloads>\orca-plugins.log`, with a documented ladder for reading it.
* `tools/sync_engine.py` added: the shared engine exists twice (plain source
  in the standalone tool, an escaped string inside the plugin) and this keeps
  them byte-identical.
* `tests/fake_orca.py` and `tests/test_plugin_runtime.py` added: the plugin
  itself now runs in the test suite, not just the engine.
* **Not verified:** the fake Orca harness is reconstructed from the wiki, so a
  green run proves our logic is self-consistent, not that Orca drives us that
  way.

## 2026-09-30 — a standalone tool, and a self-updating updater

**Updater:** 1.1.0.

* Shipped `plugins/unlayered-infill/unlayered_infill_post.py`: the same engine
  as a double-clickable tool with a small window, so it works even when the
  plugin system does not. Decision C1 was to keep **both** forms.
* The updater now updates itself: it fetches a newer copy to `%TEMP%`,
  verifies it and hands over, never overwriting the file it is running from.
* **Not verified:** the `.bat` has never been executed on Windows from this
  sandbox.

## 2026-09-30 — the version is visible from inside OrcaSlicer

**Plugins:** Unlayered Infill **0.2.1**, Wave Overhangs **0.0.4**.

* Each plugin's display name now ends in `v<version>`, so the Plugins dialog
  shows which build is installed.
* Check setup prints the running version; exported G-code carries a stamp.
* **Capability names deliberately left alone** — a process preset stores the
  capability name, so renaming them would orphan presets and make OrcaSlicer
  refuse to slice.

## 2026-09-30 — the handoff file

* Added [`MEMORY.md`](MEMORY.md), wired into `AGENTS.md` and `README.md`, so a
  new chat can pick up mid-stride after a merge.

## 2026-09-30 — the updater, rebuilt

**Plugins:** Unlayered Infill **0.2.0**, Wave Overhangs **0.0.3**.

* Recreated `Update-Orca-Plugins.bat` after the old download sources were
  deleted, and made this repository the source of truth.
* Reorganised into `plugins/`, `tools/`, `tests/`, `docs/`.
* Added `AGENTS.md` (the rulebook), `docs/ROADMAP.md` and
  `docs/ORCA-PLUGIN-FACTS.md`.
