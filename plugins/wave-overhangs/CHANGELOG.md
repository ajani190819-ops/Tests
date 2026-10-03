# Changelog — Wave Overhangs

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog in its separate Version column, and running
**Wave Overhangs - Check setup** prints it with a short version history.

**This plugin is still experimental and has not completed a verified physical
print.** The owner confirmed that 0.0.11 produced visible, perimeter-conforming
waves in a reopened real Orca export; 0.0.28 still needs a fresh export and
physical print. Treat every version here as a work in progress.

Dates are the day the change was made, not a release date.

**2026-10-02 — there are now two plugins, not three.** The experimental
`Wave Overhangs Geometry` plugin was archived to `archive/` and is no longer
installed, offered by the launcher, or listed in the catalogue. It was a
separate prototype that tried to change Orca's geometry mid-slice so waves
would show in the normal preview; it never completed a verified real slice or
print. **This plugin — Wave Overhangs — is unchanged and is still the one to
use**, and that archival did not change it (it was 0.0.27 at the time).
If the launcher previously installed
Geometry for you, it will simply stop offering it; remove it from your process
preset if you had selected it.

## 0.0.50 — 2026-10-02

* Settings now come back by themselves after an update. If the Config
  panel reappears at factory defaults and this plugin remembers values you
  had set under an earlier version, they are put back on the next slice
  and the log says what was restored. New `auto_restore_settings` (true).

**Why it needed more than the manual switch added in 0.4.5/0.0.37.** That
switch worked, but only for someone who knew it existed -- which is no use
when the symptom is "my settings are gone".

The rule is deliberately narrow, so it can never fight the Config tab's
own **Restore defaults** button:

* the saved config must be pristine -- every value at this build's
  default, which is what a wipe looks like;
* the newest remembered snapshot holding non-default values must come from
  a DIFFERENT build than the one running.

Press Restore defaults without updating and the newest snapshot is from
the running build, so nothing happens and the button means what it says.
Update, and the snapshot is from the older build, so your values return.
Tested in `tests/test_plugin_runtime.py` as all three cases: restore after
a version change, Restore defaults sticking inside one version, and the
switch turning it off.

**Also, on the curved perimeters.** A clean export of that model finally
arrived, and the answer is that the waves already reach the wall:

| | |
| --- | --- |
| median distance, wall to nearest wave | 0.157 mm (= `wall_overlap` x line width) |
| wall more than one line width away | 5% of its length |
| those stretches that are actually overhang | **0 of 22** |

Every place the waves fall short of that wall is somewhere the overhang
does not reach -- Orca prints those itself. So `contour_finish` correctly
finds nothing to add and stays off.

One real bug came out of looking: `contour_finish` could never have done
anything, because it called `linemerge()` on the wall geometry and that
raises outright when the walls merge to a single LineString, which the
surrounding `except` then swallowed. Fixed, and it now does add a bead
along a boundary the fronts never reached -- there is a test for that.

**The visual evidence predates the fixes.** The waved export in
`test-prints/t2-curved-perimeters/` is Wave 0.0.39 output: before the
point-density fix (0.0.40), before internal bridges stopped being waved
(0.0.44), and before crease rounding (0.0.45), which took the
90th-percentile turn from 90 degrees to 22. Worth re-slicing that part
before chasing it further.

## 0.0.49 — 2026-10-02

* New, EXPERIMENTAL and off by default: `contour_finish`. Adds one pass
  along the far boundary after the fronts, half a line width inside it, so
  the waved area ends ON a curved wall instead of wherever the outermost
  front happened to be pointing. Only the stretches no front already
  covers are added.

**Why it is off.** The owner reported that on rounded perimeters the waves
"curve back inwards into area that is already printed instead of following
the contour". The diagnosis is sound in principle -- wavefronts are
contours of distance from the SUPPORTED edge, and near a curved wall that
is not the same shape as the wall, so the last front is not parallel to it.

But it could not be reproduced on any export available here. On t3:

| | |
| --- | --- |
| wave ends within 0.3 mm of the wall | 99% |
| median end distance | 0.157 mm (= wall_overlap x line width) |
| median gap from wall to wave material | 0.000 mm |
| paths `contour_finish` finds to add | 0 |

So on that part the fronts already reach the wall and the pass is a no-op.
Turning it on by default would be shipping a change whose benefit cannot be
demonstrated, so it ships as a switch to try on the part that actually
shows the problem.

The curved-perimeter export in `test-prints/t2-curved-perimeters/` is
Wave 0.0.39 output, so it cannot be re-run: the original bridge moves are
already gone. **A clean export of that model -- same part, plugin switched
off -- is what is needed to finish this.**

## 0.0.48 — 2026-10-02

* A relocated overhang wall now takes its travel-in, unretract, retract and
  WIPE block with it, instead of leaving them stranded at the old position.
  A wall run that cannot take that block with it is no longer relocated at
  all.

**The owner asked whether 0.0.47 really dealt with what was in the tail of
the waved layer. It had not.** Relocating only the EXTRUDING moves left each
wall's plumbing behind, and on t3 six of them ended up chained together:

```
G0 F7200 X100.440 Y91.979
G1 E-1.75 F1800          <- retract
;WIPE_START ... ;WIPE_END
G1 X119.932 Y120.252 F7200
G1 E1.75 F3600           <- unretract
; wave-overhangs moved this overhanging wall after the waves
```

Travel, retract, wipe, travel, unretract, repeat -- with nothing printed.
That is the "goes back through the layer stopping at random points", and it
survived the previous two attempts because both were measuring travel
distance, which this barely changes, rather than reading the output.

Counting retract/wipe cycles that print nothing, on t3:

| | cycles |
| --- | --- |
| unprocessed export (Orca's own) | 32 |
| waves, no wall relocation | 37 |
| 0.0.47 | 41 |
| **0.0.48** | **38** |

Absorbing the plumbing is only safe when the extrusion inside it nets to
zero -- an unretract matched by its retract. Where it does not, the span is
refused and the run stays where it is, because taking half of a retract pair
would shift every E value after it. Total extrusion is identical with
relocation on or off, and there is a test for that.

**Still outstanding, and measured rather than guessed**: 5 of those cycles
come from the wave replacement itself, not the wall -- removing a covered
bridge move can leave the wipe that belonged to it. `wall_last: false` takes
the count to 37, which isolates the two.

## 0.0.47 — 2026-10-02

* An overhanging wall loop now moves after the waves whole or not at all.
  It was being lifted out move by move, so a loop that was partly over air
  came apart: the hanging pieces printed after the waves, the supported
  pieces stayed where they were, and the nozzle crossed the layer between
  them. That is the "goes back through the layer stopping at random
  points" the owner was seeing, and it was this plugin's own doing.
* The relocated pieces are ordered nearest-neighbour from where the waves
  ended instead of in the order the slicer wrote them.
* New `keep_uncovered_bridge` (true). Set it false to skip the trips back
  for leftover bits of original bridge entirely.

Travel after the waves on the owner's t3 export, same file each time:

| | travels | distance |
| --- | --- | --- |
| 0.0.45 | 59 | 729 mm |
| 0.0.46 | 28 | 279 mm |
| 0.0.47 | 27 | 240 mm |

What is left is genuine: the part is 40 mm across and the wall pieces are
on opposite sides of it.

**Worth being straight about what was NOT the cause.** Counting
travel-then-short-extrusion pairs across the whole file finds 199 of them,
and only one belongs to this plugin -- the rest are OrcaSlicer's own
infill ends and wipe sequences, present in the unprocessed export too. If
movement remains after this, `wall_last: false` turns the relocation off
and is the quickest way to tell the two apart.

## 0.0.46 — 2026-10-02

* Fixes the nozzle still jumping around after the waves finish. On the
  owner's t3 export that was 59 travels covering 729 mm to print 250 mm;
  it is now 28 travels and 279 mm, and the ones left are real
  repositioning between separate pieces of geometry.
* The per-move "replaced covered bridge move" comments fold into one line
  naming the range -- 390 lines became a handful on one layer.

**Why 0.0.41 did not already fix this.** That release collapsed runs of
redundant travels and was verified on the synthetic cases, where it
worked. On a real export it barely fired: OrcaSlicer sprinkles `M73`
progress lines through the G-code, and any line that was not a comment
ended a run. So the pattern on a real file was travel, M73, travel, M73 --
and every one of them survived.

M-codes that change state without moving the nozzle (`M73`, `M117`,
`M204`, `M205`, `M106`, `M107`, `M900`) now sit inside a run without
ending it, and one of this plugin's own comments can open a run as well as
continue one. The collapse is still deliberately narrow: a run is only
touched if it contains a `; wave-overhangs` comment, so G-code Orca wrote
is never rewritten.

**A lesson worth keeping**: the synthetic cases in the test suite had no
M73 lines, so they could not have caught this. The fix is verified against
the captured export as well now.

## 0.0.45 — 2026-10-02

* Rounds off the hard V kinks in a wavefront, which is what made waves on a
  curved perimeter look jagged. On the owner's t3 export the 90th-percentile
  turn at a vertex goes from 90 degrees to 22, and vertices turning more than
  20 degrees from 38% to 13%. New setting `smooth_creases` (true).

**It was not faceting.** That is what it looks like, and the obvious
suspects -- simplification tolerance, arc resolution, the support footprint
being scalloped -- were all measured and all wrong. Closing the support
polygon with radii from 0.3 to 1.5 mm moved the median turn angle by less
than a degree.

What the numbers actually said: the MEDIAN turn at a vertex was 15 degrees,
which is a smooth curve, but the 90th percentile was 90 and the maximum 179.
Those are hairpins. A front flows around an obstacle, rejoins behind it, and
meets itself in a V; every later front inherits the kink, and the line of
kinks reads as a chevron seam across the field. Curved walls show it worst
because the fronts are already turning.

**The fix** replaces each sharp vertex with a three-point chamfer, applied
three times at a shrinking setback, so a 104-degree hairpin becomes a
readable curve. Every replacement is checked against the same region guard
as everything else, so rounding cannot push a front into a hole. Turns
gentler than 30 degrees are left exactly alone -- real curvature is not
touched.

**The cost is points**: about twice as many on a crease-heavy layer (1770 to
3456 on t3). Thinning them back was tried and dropped -- re-simplifying at a
quarter of the tolerance took the 90th percentile from 22 back to 36 degrees
and saved only 265 points. The points a chamfer adds ARE the roundness. Set
`smooth_creases: false` for the old output.

## 0.0.44 — 2026-10-02

* Waves are now only used where a straight bridge cannot do the job. Two
  things Orca labels "bridge" are excluded: `Internal Bridge` (the solid
  layer over sparse infill, anchored every few mm by the infill under it),
  and any unsupported patch a plain bridge can cross -- `straight_bridge_span`,
  10 mm by default.
* New settings: `wave_internal_bridges` (false) and `straight_bridge_span`
  ("auto" = 10 mm). Setting the span to 0 and wave_internal_bridges to true
  restores the old behaviour exactly.

**Why.** Waves are for extrusion with nothing under it and nothing to span
between. They are slower than a straight bridge and they look different, so
using them where a bridge would do is a cost with no return. Measured on the
owner's t3 export, where "reach" is the distance from solid material to the
furthest point of the unsupported patch -- a straight bridge has to cross
twice that:

| Z | type | area | reach | now |
| --- | --- | --- | --- | --- |
| 5.4 | Bridge | 1035 mm2 | 37.5 mm | **waved** -- genuine thin air |
| 7.8 | Bridge | 18-54 mm2 | 2.4-3.7 mm | left alone -- the "divots" |
| 8.1 | Bridge | 1.3 mm2 | 0.2-0.4 mm | left alone -- specks |
| 9.3 | Internal Bridge | 1090 mm2 | 5.5 mm | left alone -- solid over infill |

That file goes from 7 waved sections to 1, and the pass drops from 21.7 s to
13.6 s because the work it was doing was work it should not have been doing.

Note the 9.3 case: 1090 mm2 is a large area, so area thresholds never caught
it. What makes it a straight-bridge job is that the infill below is never
more than 5.5 mm away, and the type label says so outright.

**On the Cube test export** two of the three waved sections turn out to have
been Internal Bridge as well. The remaining one -- the real 4.7 mm overhang
ring -- is still waved, because its corners sit 6.6 mm out diagonally, past
the 5 mm of reach a 10 mm bridge has.

## 0.0.43 — 2026-10-02

* Check setup now explains the most likely reason Wave "did nothing": the
  Slicing Pipeline Plugin field is ONE selection and both plugins in this
  repo want it. If Unlayered Infill is selected there, Wave is never
  called, and the export carries the Unlayered stamp and no wave blocks.

No behaviour change. This is the message that would have saved a round
trip: the owner reported Wave doing nothing on a complex part, the earlier
guess was the time budget, and the part itself disproved it -- 0.0.41
waves that same file in 19.9 s, inside its old 30 s limit. The file's first
line says `; unlayered-infill v0.4.8` and no wave stamp appears anywhere,
which is what "the other plugin had the pipeline slot" looks like.

Measured on the owner's t3 export (1.9 MB, 0.2 mm layers): 3 wave layers, 7
sections, 688 bridge moves replaced, 259 fragments retained, 104 overhang
wall moves reordered, 20.0 s. See test-prints/t3-multi-overhang/notes.md.

## 0.0.42 — 2026-10-02

* The time budget is now "auto" and scales with the size of the export --
  30 s plus 45 s per megabyte, capped at 300 s -- instead of a flat 30 s.
  A flat 30 s was set against a test cube; on a real part the pass ran out,
  handed the file back exactly as Orca wrote it, and looked for all the
  world like the plugin had not run at all.
* Roughly 1.8x faster on parts with many holes. _interior_voids() was being
  recomputed once per ENDPOINT -- 696 times on a 36-hole stress case, each
  doing a buffer and a union, 40% of the whole pass. It is a property of
  the region, so it is now computed once and cached.
* A budget that does run out is impossible to miss: Check setup leads with
  "*** THE LAST EXPORT RAN OUT OF TIME ***" and says no waves were added.

**Context: the owner reported a complex part coming back completely
unprocessed**, with the theory that it was about hole count or multiple
overhangs. That is right in effect. Measured on a synthetic stress case
(40 mm block, round holes, three overhang layers):

| holes | before | after |
| --- | --- | --- |
| 1 | 0.6 s | 0.7 s |
| 16 | 2.1 s | 1.6 s |
| 36 | 7.4 s | 4.0 s |

The cost grows with the geometry, as it must, but it was growing faster
than it needed to and the ceiling it was growing into was too low. A 4 MB
export now gets 210 s instead of 30 s.

**Why not just remove the limit.** An export that never finishes is a
broken printer. The budget stays, it is now proportional to the work, and
`time_budget: 0` still disables it for anyone who would rather wait.

## 0.0.41 — 2026-10-02

* Fixes the nozzle appearing to "scan its way across the print" after the
  waves are finished. Every original bridge move the waves covered was being
  replaced by a comment AND a travel to where that move used to start, so
  after the wave block the nozzle re-traced the entire original bridge
  raster in mid-air: 63 travels for 11 extrusions on the Cube export.
  Consecutive travels are now collapsed to the one that matters.

G0 states absolute X and Y, so only the last travel in a run has any effect;
the rest were pure wasted motion, wasted time, and an alarming preview. The
collapse is deliberately narrow -- a run of travels is only touched when it
contains one of this plugin's own comments, so G-code OrcaSlicer wrote
(wipes, retract sequences, anything with its own meaning) is never
rewritten. Measured on the Cube export: 206 travels to 151, extrusion
identical to the digit at 320.55580 mm, same 2270 extruding moves.

The "replacing bridges" comments you saw are correct and stay: they mark
where an original move was removed because a wave now covers that ground.
What was wrong was the travel next to each one.

## 0.0.40 — 2026-10-02

* Fixes curves being drawn with far more moves than they need. A front that
  ran 28 mm along a gentle curve and then squeezed past a hole used to come
  out as 132 moves; it is now 3, over the same path. The rounded-corner test
  case drops from 1048 wave moves to 606 with its path length unchanged to
  0.02%.

**Why it happened.** Simplification was accepted or rejected for a WHOLE
front at a time. If the one coarse chord near a hole would have cut the
corner, the entire front was re-simplified at a tighter tolerance -- and if
even the tightest rung failed, every raster point the buffer produced was
kept. So one difficult centimetre made the other twenty-seven expensive. The
owner described it exactly: "hundreds of lines when a couple dozen should
have sufficed."

**The fix.** When the whole-front ladder fails, the front is now refined
per-chord instead: each straight move is checked on its own against the same
hole/boundary guard, and only the chords that fail are split, at their worst
point. The result is valid by construction rather than valid-or-discarded,
so points are spent where the geometry is actually difficult and nowhere
else.

**Where it is deliberately NOT used.** Inside the normal simplification pass.
That was tried first and was worse: against a castellated bridge footprint
(`wall_snap: false`) almost every chord leaves the region, the recursion
splits down to the raster, and the Cube export went from 455 wave moves to
2350. Per-chord refinement is the right tool only once the ordinary pass has
given up.

Measured on the Cube export: 439 -> 432 wave moves with the path length
unchanged at 513.4 mm, 455 -> 425 with `wall_snap: false`, and 260 -> 256
with arc fitting on.

## 0.0.39 — 2026-10-02

* Fixes "The preset stores invalid plugin capability configuration JSON."
  Nothing this plugin writes can corrupt a preset any more: every note is
  stripped of semicolons, double quotes, newlines and tabs, and the config
  is written on a single line instead of pretty-printed over 37.

**What went wrong.** The settings can live in two places. The global store
(`data_dir()/orca_plugins/config.json`) is a real JSON file and tolerates
anything. A **preset override** is not -- a preset is a flat key=value
record, and `;` is its reference separator. That is already documented here
as the reason a capability name may not contain one; what was missed is that
it applies to the configuration VALUE as well. Several notes rewritten for
the readable panel contained a semicolon ("master switch; false leaves your
G-code untouched"), and the whole blob was written pretty-printed with
newlines. Stored in a preset, that comes back mangled, and OrcaSlicer
reports what it then sees.

**Three things now guarantee it cannot recur**: a `preset_safe()` filter
every written string passes through, single-line output from
`dump_config()`, and a test that fails if any key or value in either panel
contains `;`, `"`, a newline or a tab.

**If you are already seeing the error**, the broken value is in the preset,
so updating the plugin does not clear it by itself -- see the release notes
in CHANGELOG.md for how to clear the preset override.

## 0.0.38 — 2026-10-02

* fan now defaults to "auto" and follows your profile's Bridges fan speed,
  so cooling matches the rest of your print instead of being forced to 100%
  by the plugin. If your profile does not state a bridge fan it stays at
  100%; set a number to override.

Everything else that can be derived was already on auto as of 0.0.35
(line_spacing, perimeter_overlap, min_overhang_area, min_wave_length,
min_wave_segment, simplify_tolerance, edge_taper_distance, travel_speed,
max_iterations, wall_reach, edge_snap_distance, arc_tolerance, print_speed).

**One deliberate narrowing while doing this.** "auto" reads
`bridge_fan_speed` and nothing else. It used to fall back to
`overhang_fan_speed`, which is a different setting about sloped walls and is
commonly set much lower -- on the captured test profile it is 50%, so the
fallback would have quietly halved the cooling on unsupported extrusion
hanging in open air. No bridge fan stated now means 100%, not a substitute.

**What stays a fixed number, and why.** `flow_ratio`, `wall_overlap`,
`edge_taper_min_flow`, `overhang_tol`, `min_bridge_fragment`,
`gap_fill_min_area`, `time_budget`, and every on/off switch. These have no
Orca equivalent to inherit and nothing in the G-code implies them -- they are
judgements about how you want the part to come out, and a plugin guessing at
those is not automation, it is just a different arbitrary number.

## 0.0.37 — 2026-10-02

* Your settings are now backed up by the plugin itself, so they survive the
  Config panel being wiped. Every run saves a copy of the values in force;
  if the panel ever comes back reset, set restore_backup to true and slice
  once to put them back, and the flag turns itself off.
* The backup is a short history, not one slot, because the wipe is followed
  by a run that would otherwise overwrite the only copy with the defaults
  that just replaced your settings.
* A value typed into the Config panel is captured even if you never slice
  afterwards: the snapshot is taken by the config lifecycle hook as well as
  by every run, and it is not rate-limited by the once-per-session migration,
  so a second and third edit in the same session are captured too.

**Why this was needed.** OrcaSlicer owns the settings file and keeps it in
one global place, so a plugin cannot stop it being reset -- "Restore
defaults", a reinstall, a data-directory or profile change, an Orca upgrade.
Ordinary version-to-version migration already preserved everything (it merges
this build's new keys into your saved copy and never touches a value you
set), but there was no protection against the file simply going away.

**Nothing is restored automatically**, deliberately. Silently putting old
settings back would make "Restore defaults" impossible, and a plugin that
overrules an explicit action is worse than one that loses a value. So the
backup sits there, Check setup prints exactly what it holds, and
`restore_backup` is a one-shot undo you ask for.

**The one case where a value genuinely cannot carry over** is a setting this
build no longer has. Those are dropped on the way back in rather than
resurrected as dead keys.

## 0.0.36 — 2026-10-02

* The overhanging part of the wall is now printed AFTER the waves instead
  of before. Orca emits a layer walls-first, which lays that wall into open
  air with nothing underneath it, so it droops before the waves that were
  supposed to carry it even exist. Set wall_last: false for the old order.
* New, experimental and off by default: adaptive_flow, the Arachne idea
  applied to wave spacing. Fronts step out a fixed spacing, so the strip
  left against the far boundary is rarely a whole bead wide; this widens
  the neighbouring front's flow to absorb it. Paths never move.

**The overhanging wall is now printed after the waves, not before.** Orca
emits a layer walls-first. On an overhanging layer that is exactly backwards:
the wall is laid into open air with nothing underneath it, so it droops
before the waves that were supposed to carry it even exist. The waves bridge
their way outward from supported material and support each other as they go,
so they have to come first; the wall then lands on something.

On any layer where Wave did something, the overhanging part of the wall is
lifted out and re-emitted immediately after the wave block. On the captured
Cube export that is 11 moves on one layer; on the synthetic overhang-with-a-
hole case, 73.

Three rules keep it safe, all covered by tests:

* **Relative E only.** In absolute E the numbers are positions, so moving a
  run of moves would make the extruder jump. Absolute-E exports are left
  alone entirely.
* **Position continuity.** Where a run is cut out, a travel to its end point
  is left behind, so every move that followed still starts where it expected
  to; and the relocated block ends by travelling back to where the wave
  output left the nozzle, so wipes and retracts downstream are unaffected.
* **The wall is re-emitted verbatim** — same coordinates, same E, same width,
  and its `;TYPE:` markers and `M204`/`M205` acceleration and jerk settings
  travel with it. Its original feedrate is restored first, because the wave
  block leaves a different F in force. Total extrusion is identical to the
  digit; only the order changed.

Set `wall_last: false` to go back to Orca's order.

**New, experimental: `adaptive_flow` (off by default), the Arachne idea
applied to wave spacing.** Arachne varies bead *width* so a shape is filled
exactly rather than tiled with fixed-width lines and left with slivers. Wave
has the same problem in one dimension: fronts step outward a fixed
`line_spacing`, so the strip left against the far boundary is rarely a whole
bead wide. With `adaptive_flow` on, each uncovered patch is assigned to the
front it sits against, and that front is asked to extrude the material the
patch needs, spread along its own length:

    extra width = uncovered area assigned to this front / its length
    scale       = (line width + extra width) / line width

capped by `adaptive_flow_max` (1.5 by default). **Paths do not move — only
flow changes**, which is the half of Arachne that can be done safely to an
already-sliced file.

Honest numbers from the Cube export: +0.03% extrusion, because `gap_fill`
already puts a path down each sliver. With `gap_fill: false` it is +0.12%.
So on a part like this it is a refinement, not a transformation — it matters
most on parts whose overhang boundary runs at a shallow angle to the march of
the fronts, where the leftover strip is long. It is off by default until it
has been printed.

## 0.0.35 — 2026-10-02

**A settings panel you can read, and settings that follow your own Orca
profile.** On a stock 0.4 mm profile the output is unchanged, to the digit —
that is enforced by a test.

**Thirty-three settings are now nine numbered groups.** BASICS, WHAT COUNTS
AS AN OVERHANG, THE WAVE ITSELF, PRINT ORDER, CLEANUP, MEETING THE WALL, LINE
ENDS AND FLOW, SPEED AND COOLING, ARC MOVES. Each note is one short sentence;
the detail moved to the README. Panel order and guide order come from the
same list.

**Nine settings now default to `"auto"` and are derived from the print.** The
point: almost none of these were really numbers. `line_spacing: 0.35` always
meant "seven eighths of a 0.4 mm line" — written as a constant it is correct
for one profile and quietly wrong for every other, which is exactly what
happened on the 0.6 mm test fixture, where waves were being spaced for a
printer nobody was using.

| setting | auto means | at a 0.40 line | at a 0.57 line |
| --- | --- | --- | --- |
| `line_spacing` | 0.875 x Wave line width | 0.350 mm | 0.502 mm |
| `perimeter_overlap` | 0.25 x line width | 0.100 mm | 0.143 mm |
| `min_wave_length` | 2.5 x line width | 1.000 mm | 1.433 mm |
| `min_wave_segment` | 0.75 x line width | 0.300 mm | 0.430 mm |
| `edge_taper_distance` | 1.5 x line width | 0.600 mm | 0.860 mm |
| `min_overhang_area` | ~3 line widths squared | 0.500 mm2 | 1.027 mm2 |
| `simplify_tolerance` | 0.125 x line width, floored at your Resolution | 0.050 mm | 0.072 mm |
| `travel_speed` | your profile's travel speed | — | — |
| `max_iterations` | enough fronts to cross the region, plus headroom | — | — |

Every factor is chosen so that auto on a stock 0.4 mm / 0.0125 mm-resolution
profile reproduces the constant this plugin shipped with, exactly. Upgrading
does not change a 0.4 mm print.

**The iteration cap adapts.** It was a flat 400, which is both too small for
a large overhang — it stopped half way — and meaningless for a small one.
`"auto"` measures the region and asks for as many fronts as it takes to cross
it, plus headroom, clamped to 64..20000. It is a runaway guard, not a quality
dial: it never adds a front the geometry did not ask for, so it cannot bloat
the file. On the test fixture it settles at the 64 floor; the file got
*smaller*, 2,931 wave moves against 3,087 for the old 0.4-tuned config.

**`fan` also accepts `"auto"`** (your profile's bridge fan), but stays at 1.0
by default: full cooling on an overhang is a recommendation worth keeping,
not something to inherit silently.

**What stays constant, deliberately.** `pattern`, `start_policy`,
`component_order`, `flow_ratio`, `wall_overlap`, `edge_taper_min_flow` and
the on/off switches. No amount of reading the G-code tells you what someone
wants a part to look like.

**Check setup and the log now print what each auto resolved to**, with the
reasoning, instead of echoing "auto" back at you.

## 0.0.34 — 2026-10-02

**Preventive, no change to the waves.** Unlayered Infill 0.4.3 fixed two
import-time hazards; both patterns existed here too, and are now closed
before they can bite.

* `wave_core` registered an empty module in `sys.modules` before the engine
  was executed into it, and removed it on failure. Pressing Refresh in the
  Plugins dialog re-imports the plugin in the same interpreter, so a second
  pass could have replaced a working engine with nothing. The replacement is
  now built aside and published only after a clean exec.
* `_pt()` did `from shapely.geometry import Point` on first use — inside a
  capability call, where Orca's audit hook watches every file open. `Point`
  now comes from the module-level shapely import that was already there.

## 0.0.33 — 2026-10-02

**A settings panel left over from an older build now repairs itself.**

OrcaSlicer keeps each capability's settings in one global file,
`orca_plugins/config.json`, and the Config tab shows you that saved copy. A
copy written by an older Wave build does not contain the settings added since,
so they never appear in the panel — and Wave Overhangs has gained a great many
settings since 0.0.11.

Orca documents a migration hook for exactly this, and the plugin now uses it.
When it finds a configuration written by an older build it merges this build's
settings into it and saves it back: every value you set is kept, new settings
arrive at their defaults, the notes are refreshed, and unrecognised keys are
left alone rather than deleted. This runs when the plugin loads, and again on
the first slice or **Settings guide & check** run of a session.

Check setup's "not seeing all the settings?" section was rewritten to match:
the manual fallback is **Restore defaults** in the Config tab, not the
preset-reselect recipe from 0.0.32, which was based on a wrong idea of where
the settings live.

No slicing behaviour changed; exports are identical to 0.0.32's.

## 0.0.32 — 2026-10-02

> **Superseded by 0.0.33.** The explanation below — that Orca copies the
> settings into your *process preset* — was wrong, and so was the
> None-and-back recipe. The settings live in `orca_plugins/config.json`, and a
> stale copy *can* be repaired in code, which is what 0.0.33 does.


Same stale-settings explanation as Unlayered Infill 0.4.1.

Wave Overhangs has gained a lot of settings since 0.0.11, so it is wide open
to the same trap: OrcaSlicer saves a copy of a plugin's settings into your
process preset the first time you select it, and keeps showing that saved copy
afterwards. Settings added by later versions are missing from it, which looks
like the update did not install.

**Check setup** now prints how many settings the installed build has, plus the
three steps that refresh the saved copy (set the Slicing Pipeline Plugin to
None, back to Wave Overhangs, save the preset). No behaviour change: not one
line of exported G-code differs from 0.0.31.

## 0.0.31 — 2026-10-02

The thousands of pointless micro-moves around holes are gone.

* **Fronts that wrap a hole no longer keep every raster point.** The owner
  spotted this: *"randomly you have an absurd number of lines just to do a
  tiny chunk of curve next to the hole"*. They were right, and it was bad —
  on their part **60.7% of every wave move was under 0.1 mm long, and all of
  those together carried 0.9% of the distance printed.** The median wave move
  was 0.015 mm. Fifteen microns. Thousands of G-code lines doing nothing.

  Two separate causes, both fixed:

  1. **A self-defeating safety guard.** Before simplifying a wave path the
     plugin checks it has not cut a corner into a hole, allowing it to stray
     by a small margin. That margin was computed as `tolerance * 0.4`. When
     the first simplification attempt was rejected the code retried with a
     *smaller* tolerance to be gentler — but that shrank the margin by the
     same factor, so the guard got stricter at exactly the moment it needed
     to relax. A front hugging a hole failed all three attempts and fell back
     to keeping every single point. The margin is now a fixed allowance of
     0.02 mm — a twentieth of a line width — and no longer moves with the
     tolerance.
  2. **Nothing removed points piled on top of each other.** The simplifier
     keeps a point whenever it sits far from the straight line between its
     neighbours, which at a sharp cusp — precisely what a wavefront forms
     where it wraps a hole — means keeping two points microns apart. Points
     closer together than 0.02 mm are now merged, and a front that still
     cannot be simplified at all is thinned this way instead of being left
     raw.

* **Nothing about the shape changed.** This only removes points that were
  not describing anything. On the test export the wave path is **513.5 mm
  before and 513.5 mm after**, while the move count drops from 508 to 439 and
  sub-0.1 mm moves fall from 91 to 22. With `wall_snap` off the drop is
  larger: 739 moves to 455. Every existing check that waves stay out of holes
  still passes, and the thinning is re-tested against that same hole guard
  before it is accepted.

* **What you should notice.** A much smaller G-code file, and less chance of
  your printer stuttering. Thousands of micro-moves can arrive faster than a
  printer's motion planner can process them, which makes it pause and jerk
  through a curve regardless of the speed you set. The distance printed is
  identical, so this does not change the time estimate by itself.

  The owner's part had 60.7% tiny moves against the test fixture's 17.9%,
  because it has holes and the fixture barely does, so the improvement there
  should be considerably bigger than the fixture numbers above. That part is
  a projection — the measured figures are the fixture ones.

## 0.0.30 — 2026-10-02

Waves now print at your bridge speed by default, like any other bridge.

* **`print_speed` now defaults to `"orca"`.** In 0.0.29 this was something you
  had to opt into; the owner asked for it to simply be the behaviour. Set your
  bridge speed in OrcaSlicer (Print Settings > Speed) and the waves use it,
  the same as every other bridge on the part. Change it later and the waves
  follow — the plugin reads it out of the exported G-code each time, section
  by section, so nothing is baked in and nothing needs copying across.

  Put a number in `print_speed` to override it, in mm/s. If a section has no
  readable feedrate the plugin still falls back to 2 mm/s rather than guessing.

  With the owner's bridge speed of 10 mm/s, their part is predicted to go from
  **1h44m to about 24 minutes**:

  | bridge speed | predicted total |
  |---|---|
  | 0.0.27, as actually printed | 103.9 min |
  | 2 mm/s (the old default) | 71.5 min |
  | 5 mm/s | 36.2 min |
  | **10 mm/s (the owner's setting)** | **24.5 min** |
  | 20 mm/s | 18.6 min |

* **If your overhang droops, slow the bridge speed down.** Now that waves
  follow your profile, this is the dial, and it is worth knowing why a wave
  may need to be slower than a normal bridge: a bridge is anchored at *both*
  ends, so tension holds the strand up while it cools, but a wave line is
  cantilevered into open air and held at one end only. Nothing stops it
  sagging except cooling fast enough to hold its own shape. If you see droop
  or stringing, put a number in `print_speed` — try 5, or 2 for a bad
  overhang — rather than slowing your whole profile down.

  These timings are arithmetic on the G-code. No physical print has been run.

## 0.0.29 — 2026-10-02

`print_speed` can now follow the bridge speed in your own Orca profile.

* **Set `print_speed` to `"orca"` and wave lines use your bridge speed.**
  Requested by the owner: 2 mm/s is the single biggest cost in a wave print,
  and your Orca profile already states a bridge speed, so having to copy the
  number across by hand was silly. With `"orca"` the plugin reads the feedrate
  off the very bridge moves it is replacing, section by section, straight out
  of the exported G-code. It therefore follows whatever your profile says
  without the plugin needing to know anything about your printer. `"bridge"`
  and `"auto"` do the same thing. Anything that is not a number and not one of
  those words falls back to the safe 2 mm/s default rather than failing.

  Resolution is **per bridge section**, not one value for the file. A part can
  genuinely have Bridge and Internal Bridge at different speeds, and the test
  fixture does: its sections come out at `F1200` and `F420` and each wave
  block follows the section it replaced.

* **Read this before using it — it is a big jump, and it can ruin a print.**
  On the owner's part the measured effect is:

  | setting | predicted total |
  |---|---|
  | 0.0.27 (with the feedrate bug) | 103.9 min |
  | 0.0.28 (bug fixed, `print_speed` 2.0) | 71.5 min |
  | `print_speed = 5` | 36.2 min |
  | `print_speed = 8` | 27.4 min |
  | `print_speed = "orca"` (their bridge speed, 20 mm/s) | 18.6 min |

  That is a **ten times** speed increase over the default. The catch is that
  Orca's bridge speed is tuned for a *bridge*, which is anchored at both ends
  so tension holds the strand up while it cools. A wave line is **cantilevered
  into open air, held at one end only** — nothing stops it drooping except
  cooling fast enough to hold its own shape. The two are not the same job, so
  your bridge speed is not automatically a safe wave speed.

  The default is unchanged at 2.0 and will stay that way. If you want the time
  back, the honest advice is to walk up: try 5, look at the overhang, then 8,
  then try `"orca"`. Those numbers are predictions from move-by-move
  arithmetic on the G-code, not from a physical print.

## 0.0.28 — 2026-10-02

A third of the print time was being wasted. This fixes it, and costs nothing
in print quality.

* **Moves after a wave block were crawling at the wave speed.** This is the
  big one. In G-code a speed is "modal": once you set one it stays in force
  until something changes it. The plugin printed its waves at `print_speed`,
  which is deliberately very slow (2 mm/s by default, so `F120`), and then
  handed control back **without putting the speed back**. It also wrote the
  moves that replace covered bridge extrusions with no speed of their own.
  So those moves inherited 2 mm/s and took minutes instead of seconds.

  Measured on the owner's own export (`test print_19m50s.gcode`): **373
  moves covering 3.95 m that should have taken 30 seconds took 32.9
  minutes.** That is **32 minutes of a 106 minute print — 31% of it** — on
  travel moves. OrcaSlicer reported it in the preview legend as 36m49s of
  "Travel" at an average of 5.8 mm/s, which is what first looked wrong.

  Every move the plugin writes now states its own feedrate, and the original
  speed is handed back before your untouched moves resume. Nothing about the
  wave toolpaths themselves changed — the plastic that comes out is
  identical, it just stops wasting time getting there. For the same reason
  this is a pure win: there is no quality trade-off to weigh up.

* **A note on what this does *not* fix.** The wave lines themselves are still
  printed at `print_speed`, and on the owner's part that is 58.8 minutes, the
  majority of the remaining time. That slowness is deliberate — it is what
  lets each line cool and hold its shape in mid-air, and raising it is the
  most likely cause of droop. If you want to spend that time, raise
  `print_speed` gradually and test; the plugin will not do it for you.

## 0.0.27 — 2026-10-01

One file to run, and the settings explain themselves inside OrcaSlicer.

* **There is now one file: `Orca-Plugins.bat`.** Previously there were two
  and the difference between them was never clear -- a "chooser" and an
  "updater". Now you download and double-click one thing. It shows which
  build and which OrcaSlicer folder it remembers, and pressing Enter installs
  with those. The menu also offers choosing a different version, and
  forgetting your remembered choices to start fresh.

  It updates itself, the same careful way the installer already did: it
  fetches the newest copy, checks it really is the launcher and not a 404
  page or a wifi login portal, and runs that for this run. It never
  overwrites itself while running, because Windows reads a .bat by byte
  position as it executes and a file that rewrites itself mid-run can jump
  into garbage. So a bad download can never leave you without a working
  launcher.

* **Your old files keep working.** `Choose-Orca-Plugin-Version.bat` is now a
  short forwarder that hands over to `Orca-Plugins.bat`, so existing
  shortcuts do not break. `Update-Orca-Plugins.bat` deliberately keeps its
  name and its download URL, because every copy already on someone's disk
  checks that exact address for its own updates -- renaming it would have
  stranded those copies on an old version with no warning. It is now the
  install engine underneath, and still works on its own.

* **The plugin explains its settings inside Orca.** The menu item is now
  **Wave Overhangs - Settings guide & check**. It still reports whether the
  plugin is working, and then lists every setting with a plain-English
  explanation, showing the value you actually have in force and marking
  anything you have changed away from the default. No more opening a README
  on GitHub to find out what `perimeter_overlap` does. Set `settings_guide`
  to false in that item's own settings once you know them and you get just
  the short status report.

  **Note:** renaming that menu item changes its identity in OrcaSlicer. If
  you had it selected in a process preset you may need to pick it again from
  the list once.

## 0.0.26 — 2026-10-01

Updater convenience, a latent crash fixed, and an honest non-result on wave
blending.

* **The chooser and the updater now remember what you picked.** The branch
  chooser already stored your last branch but still made you pick it from the
  menu; pressing Enter on its own now just reuses it. The updater now
  remembers the OrcaSlicer data folder it installed into and offers it the
  same way, so a repeat update is Enter, Enter. Both still show the full menu,
  so switching is exactly as easy as it was. The remembered values live in
  `%LOCALAPPDATA%\OrcaPluginUpdater\` (`branch.txt` and `datadir.txt`);
  delete them to be asked from scratch.
* **The settings panel now explains itself.** OrcaSlicer shows the plugin
  config as JSON, and JSON cannot hold comments, so every explanation written
  in the source was invisible to you -- the panel was 33 bare names with no
  hint what any of them did. Each setting now has a plain-English note
  directly above it saying what it does, what the units are, and what happens
  if you change it. Notes are the keys beginning with `_`; the plugin ignores
  them, so you can edit or delete them freely and nothing breaks.

  The note on `arc_fitting` in particular now spells out that it controls
  *Wave's own* arcs and that OrcaSlicer's arc fitting is a separate setting in
  **Print Settings > Quality > Precision > Arc fitting**, which this plugin
  does not touch.

* **A real crash in the wave propagation, fixed.** Clipping one boundary
  against another can leave a single-point line behind, and shapely's
  `linemerge` raises `GEOSException` on those. `GEOSException` is not a
  `ValueError`, so the handler that was there could not catch it, and the
  whole layer was lost — the plugin failed closed and produced no waves at
  all. This is the same fault that killed an earlier optimisation attempt.
  `wave_tracks` now retries without the crumbs, and only when the normal
  merge has already failed, so ordinary fronts are untouched.
* **`wake_blend`: new, experimental, and off by default.** When the field
  flows around a hole and rejoins behind it, the two arriving sides meet in a
  sharp V and every later front inherits the kink, leaving a hard seam
  downstream. `wake_blend` rounds that crease off, in multiples of
  `line_spacing`.

  It works on a simple round hole — the V is replaced by smooth curves. It is
  **off by default because it is not good enough yet**: on the owner's real
  part it also loses about 4% of the wave coverage (1911 mm of path down to
  1833 mm) and turns 8 tiny fragments into 40. Healing makes consecutive
  fronts partly coincide, and the "already reached" subtraction then cuts
  them into dashes. Two different fixes for that were tried and neither
  worked; the remaining idea is written up in `docs/ROADMAP.md`. Set
  `wake_blend` to 1.0 to try it; values over 1.5 are clamped because beyond
  that the closing swallows whole fronts.

  With `wake_blend` at its default of 0 the output is byte-for-byte identical
  to 0.0.25 on all five test shapes, including the owner's own export.

## 0.0.25 — 2026-10-01

The export hang is an OrcaSlicer bug, and this version stops Wave from
triggering it. The owner confirmed the failure only happens when **Arc
fitting** is switched on in OrcaSlicer — which, with 0.0.21–0.0.23's `auto`
default, is exactly when Wave wrote G2/G3 arcs into the finished file.

* **This is OrcaSlicer issue #7433**, "Post processing script results in
  corrupted gcode / crash when previewing model", opened in November 2024
  against Orca 2.2.0 and still reproducing in 2.3.2 nightly as of January
  2026. The reporter triggered it with ArcWelder, a post-processor that does
  the same thing Wave was doing: replacing straight moves with arcs. Orca
  crashes or shows corrupt G-code when it re-reads post-processed output
  containing arcs. Nothing in the plugin can fix that, so the plugin stops
  provoking it: `arc_fitting` shipped as `false` from 0.0.24 and stays that
  way. **Check setup** now prints the arc setting and warns, with the issue
  number, if arcs have been switched back on.
* The arcs themselves were audited and are not malformed. Across all test
  fixtures and the owner's own export: 118 arcs, radii 0.78–12.1 mm, sweeps
  11–149 degrees, no major arcs, no near-full circles, no impossible chords,
  no reversed directions, every one carrying positive extrusion. The problem
  is on Orca's side of the handover, not in the geometry.

Separately, a real defect found while auditing that output:

* **542 dead moves removed from the owner's export.** Coordinates are written
  to three decimals, but the emitter decided whether to write a move using
  the unrounded step length. Steps shorter than a micron were therefore
  written out as moves whose X/Y rounded to the same values as the line
  before — literal no-ops, most of them `E0.00000` as well. Their export
  carried 542 of them. The emitter now tracks the position it has actually
  written and rolls any skipped step's extrusion into the next real move, so
  the dead lines disappear without losing material. Wave moves in that export
  drop from 2,006 to 1,910 (1,464 plus 74 arcs with arcs on).
* Arc `I`/`J` offsets are now measured from the last coordinate actually
  written rather than from the unrounded geometric point, which could sit
  half a micron away. Worst-case arc radius inconsistency in the owner's
  export improves to 0.0013 mm.

## 0.0.24 — 2026-10-01

Fixes an export that never finishes. The owner reported that after updating
past 0.0.20 OrcaSlicer sat on "exporting" and then crashed about a minute
later. Two changes, both aimed at making that impossible rather than at any
one suspected cause.

* **Wave can no longer hang an export.** The whole G-code pass now runs
  against a wall-clock ceiling, `time_budget`, set to 30 seconds. If the
  pass is still going when the clock runs out it stops and hands back the
  file exactly as OrcaSlicer wrote it — not a byte changed and no Wave
  stamp, so a later run will happily try again. The result message says so
  plainly instead of quietly reporting that it found nothing. Set
  `time_budget` higher if you have a big model and the time to wait, or to
  `0` to remove the ceiling. A slow Wave is a nuisance; an export that
  never finishes is a broken printer, so this gives the feature up rather
  than ever blocking a slice.
* **Arc moves are off by default again.** `arc_fitting` now ships as
  `false` instead of `auto`. G2/G3 is the one genuinely new *kind* of
  output Wave started writing in 0.0.21, and OrcaSlicer re-parses the
  finished file for its preview and time estimate, which makes it the most
  likely suspect for an export that stalls after the plugin has run. The
  arcs themselves were re-checked and are well formed — across every test
  fixture and the owner's own export there is not a single zero-radius,
  full-circle or mismatched-endpoint arc, and they track the real wavefront
  about 3.5x more accurately than the straight moves they replace — but
  "off until proven on real hardware" is the right default for something
  that could stop a print being made at all. Set `arc_fitting` to `"auto"`
  to get the old behaviour back, where arcs follow the Arc fitting setting
  in your print profile.

Honest note: the crash could not be reproduced here. The arc fitter was
measured and is linear (under 0.1 s for an 800-point front), and a part
that overhangs on every single layer costs 0.08–0.18 s per bridge layer
with no memory growth, so neither explains a crash. The time budget is a
backstop that works whatever the real cause turns out to be. If it fires,
the log file — its path is printed by **Wave Overhangs - Check setup** —
will record `timed_out`, how far the pass got and how long it took.

## 0.0.23 — 2026-10-01

* Much less work per export. Geometry is now built only for layers that have
  a Bridge section and the layer that holds each one up. On the owner's own
  export that is 7 layers out of 134, so 95% of the shapely objects that used
  to be created were never looked at: parsing went from 1.65 s to 0.2 s. An
  export with no Bridge section anywhere now returns in about 0.01 s without
  building a single piece of geometry.
* Extrusion footprints are buffered once per line width instead of once per
  move, which on a layer with twenty thousand moves is one GEOS call instead
  of twenty thousand.
* Cleanup no longer rebuilds its guard shapes for every front. They depend
  only on the region and the tolerance, so they are built once per section.
* The log now records `seconds`, `parse_seconds`, `plan_seconds`,
  `geometry_layers` and `layers_scanned`, so a slow export can be diagnosed
  from the log instead of guessed at.
* Note on what was actually making exports slow: 0.0.20 wrote 29,374 Wave
  moves into the owner's export -- 0.89 MB, a third of the whole file --
  because of the simplification fault fixed in 0.0.21. The same input now
  produces 2,006 moves and 74 arcs in 0.07 MB, and the finished file drops
  from 2.66 MB to 1.82 MB. Everything downstream that walks those moves
  (preview, time estimate, transfer to the printer) gets that back.

## 0.0.22 — 2026-10-01

* Fixed the unfilled corner the owner photographed. A wavefront is a contour
  of equal distance from the supported edge, and the contours step outward one
  line spacing at a time, so where the far boundary runs at an angle to that
  march -- the tip of a corner -- the last contour stops short and leaves a
  small sliver with nothing in it. Measured in the owner's own export: a
  0.22 mm^2 void, 0.53 x 0.75 mm, in the corner of the plate. Wave now fills
  such a sliver with one short path down its middle. `gap_fill` (on) and
  `gap_fill_min_area` (0.05 mm^2) control it. It only ever adds material where
  there is none: a part with no sliver comes out byte for byte identical.
* A short wavefront that touches a full-length rung is now kept instead of
  discarded. Short fronts were dropped to avoid specks printed into thin air,
  but one that touches a rung already on the plate is anchored, and dropping
  it was leaving holes in exactly the places this release is about. Whether a
  front survives is measured against every rung, not the ones kept so far, so
  print order cannot change which bridge extrusion ends up covered.
* Investigated the second thing the owner circled -- a rounded wall whose Wave
  edge looks like a staircase. Measured in their export, every Wave end along
  that curve sits 0.456 to 0.457 mm from the wall: a spread of 0.001 mm, so the
  ends are already on the wall. What the preview shows is the flat end of each
  rung meeting a curve at 0.35 mm intervals. Smoothing that needs a rung laid
  *along* the wall with the others trimmed back to make room; the first attempt
  made the edge worse and was not shipped.

## 0.0.21 — 2026-10-01

* Wave can now emit **G2/G3 arc moves**. Wave runs after Orca has written the
  file, so Orca's own arc fitter never sees these toolpaths; Wave fits its own
  arcs instead. `arc_fitting` is `auto` by default, which follows the export's
  own `enable_arc_fitting` setting: switch Arc fitting on in the print profile
  and the Waves become arcs too, leave it off and nothing changes. An arc is
  only used where it is measurably closer to the real wavefront than the
  straight moves it replaces, and it is re-checked against the overhang region
  so a bowed arc cannot push into a wall or a hole.
* Wave now **reads** G2/G3 moves out of the export. With arc fitting switched
  on, a round hole's wall is exported as arcs; before this, Wave could not see
  that wall at all, which would have silently undone the 0.0.20 perimeter fix
  on exactly the parts that need it most. Both the `I J` and `R` forms are
  understood.
* Fixed a large file-size problem on parts with holes. Cleanup refused to
  simplify any front that *touched* a hole, and since 0.0.20 ends deliberately
  finish on hole walls, nearly every front fell back to its raw form: a 9.9 mm
  front was being written as 980 moves instead of 9. Touching a hole is no
  longer treated as cutting across one, and when a shortcut really would cut a
  corner, Wave retries with a tighter tolerance before giving up. On the test
  part with a hole this cut the Wave G-code from 316 KB to 9 KB.
* Added `arc_tolerance` (`auto` follows the profile's own `resolution`, capped
  at 0.05 mm).

## 0.0.20 — 2026-10-01

* Fixed the frayed Wave edges. Orca exports bridge infill as separate lines, so
  the area those lines cover has a castellated edge that also stops short of
  the perimeter. Wave was clipping its fronts to that edge, which is what made
  the ends look jagged. Wave now reads the wall moves the layer actually
  printed and squares the overhang area up against them, so fronts run from the
  supported perimeter all the way to the overhang perimeter and to any hole.
* Wave ends now finish inside the wall bead, overlapping it by 25% of the Wave
  line width by default (`wall_overlap`), so the following perimeter has a
  straight, fully bonded edge to print against instead of a sawtooth.
* Added `wall_snap` (on by default; set it to `false` to get the 0.0.19 edges
  back for comparison), `wall_reach` (how far the area may be stretched to
  reach a wall, `auto` = 1.5 line widths) and `wall_overlap`.
* Fixed wall material being measured one G-code move at a time, which left a
  hairline slit at every vertex of a curved wall. A Wave end could slip through
  one of those slits and finish on the visible surface of a hole.
* Stretching the area to the wall can never create a Wave where there was not
  one: a region still has to come from bridge extrusion Orca exported over
  unsupported space, and nothing may be placed outside the part.

## 0.0.19 — 2026-10-01

* Stopped creating default tiny endpoint subdivision moves. Endpoint taper now
  changes E on the existing straight Wave moves unless `edge_taper_segment` is
  explicitly set above zero, avoiding the rectangular/grid texture seen near
  some walls.
* Changed endpoint snapping to extend along the Wave's own endpoint direction
  until it reaches the wall or hole boundary, rather than jumping sideways to
  the nearest boundary point.
* Kept snap-to-boundary and endpoint taper as the default clean-edge behavior,
  with optional `edge_clearance` still off by default.

## 0.0.18 — 2026-10-01

* Changed the default edge cleanup from clearance/inset to snap-to-boundary.
  Wave endpoints are projected back onto nearby outer walls, holes, and concave
  detail boundaries so they conform to the perimeter instead of leaving gaps.
* Set `edge_clearance` off by default. It remains available as an explicit
  comparison/debug option, but the normal output now keeps Wave endpoints on the
  visible perimeter and uses taper to reduce endpoint blobs.
* Added `edge_snap_distance` (`auto` by default) and regressions proving that
  wall and hole endpoints snap to their perimeter while support-side anchors are
  not moved away from support.

## 0.0.17 — 2026-10-01

* Added `edge_clearance` for cleaner Wave terminations near overhang walls,
  holes, and concave detail boundaries. The emitted Wave centerline is clipped
  back from those non-support boundaries so the bead should not bleed into the
  overhang perimeter.
* Kept bridge replacement coverage based on the untrimmed cleaned Wave paths, so
  old straight Bridge fragments are still removed at the boundary instead of
  reappearing where the visible Wave bead was inset.
* `edge_clearance="auto"` follows the exported bridge width; set
  `edge_clearance=0` to compare against the previous full-length endpoint
  behavior. Endpoint flow taper remains active after the inset unless disabled
  separately.

## 0.0.16 — 2026-10-01

* Added Arachne-like endpoint flow taper for the post-processing Wave emitter.
  Wave endpoints that touch outer walls, holes, or concave detail boundaries are
  split into short moves and extruded with less E near the boundary, so they can
  finish cleaner instead of leaving full-width jagged blobs.
* Excludes the support-side anchor boundary from tapering so the first Wave rung
  keeps full flow where it needs to bite into supported material.
* Keeps the original one-pass G-code replacement, plugin-storage logging, fan
  restoration, idempotence, retained-fragment behavior, and fail-closed safety.
  Setting `edge_taper_distance=0` restores the previous 399-move no-taper Cube
  output for comparison.

## 0.0.15 — 2026-10-01

* Added fully documented configuration controls for propagation mode, front
  pattern, endpoint policy, component order, spacing, cleanup, extrusion, fan,
  speed, and iteration safety.
* Added `component_order="nearest"` for shorter same-distance travel moves
  around holes while preserving near-to-far mechanical anchoring.
* Kept `auto` propagation and `support` component order as safe defaults.
* Refined front cleanup so only short endpoint stubs are removed after curve
  simplification; interior points are retained for smooth wall and hole
  termination. Connected obstacle fronts are merged before cleanup. The
  captured Cube fixture now emits 399 smoothed Wave moves; a fresh 0.0.15
  Orca export and physical print are still required.

## 0.0.14 — 2026-10-01

* Added obstacle-aware propagation for holes entirely inside the unsupported
  plane. Fronts now continue around both sides instead of only reacting when a
  hole touches the supported boundary.
* Prevented curved Wave fronts from being simplified into straight chords
  through holes or concave voids. Topology-safe cleanup keeps the original
  boundary when a shortcut would leave the unsupported region or enter an
  interior hole.
* Added internal-hole and circular-hole regressions alongside the concave-front
  test. Disconnected components remain separate and their travel remains
  non-extruding.
* The captured Cube export keeps its 3-layer, 386-Wave-move result and the
  existing cleanup/bridge-retention measurements. A fresh 0.0.14 Orca export
  and physical print are still required.

## 0.0.13 — 2026-10-01

* Fixed the Wave-to-original-toolpath handoff. Every replaced bridge segment
  now ends with an explicit non-extruding `G0` travel to its original endpoint,
  so the next retained `G1` move cannot draw a sharp line from a Wave endpoint.
* Refined cleanup with configurable minimum front length, minimum Wave segment
  length, simplification tolerance, and retained-bridge fragment threshold.
  The captured fixture emitted 386 cleaned Wave extrusion moves and removed
  30 isolated short fronts.
* Added real pattern and endpoint settings: `smart`, `monotonic`, and `zigzag`,
  plus `supported`, `consistent`, and min/max X/Y endpoint policies. Monotonic
  keeps one direction across fronts while all inter-front travel remains
  non-extruding.
* Added safe uniform absolute-E support: the temporary relative block restores
  `M82` and the prior command value with `G92`; mixed E-mode sections fail
  closed. The fixture retains 31 substantial uncovered fragments and remains
  idempotent. A fresh 0.0.13 Orca export and physical print are still required.

## 0.0.12 — 2026-10-01

* Fixed the real-print Z alignment: Orca's `;Z:` comment is nominal and the
  owner's profile adds `z_offset = 0.25`. Waves now use the bridge extrusion's
  actual modal nozzle Z, changing the test layers from 5.4/9.6/14.4 mm to the
  correct 5.65/9.85/14.65 mm.
* Cleaned up edge dots by simplifying each conforming wavefront, merging
  sub-0.15 mm chatter, deriving Wave width from Orca's real bridge width
  (0.573 mm in the captured export), and dropping isolated uncovered remnants
  shorter than half a line width.
* On the captured 0.30 mm cube export, wave extrusion moves fall from roughly
  1,850 tiny moves to 436 clean moves. Twenty-five substantial uncovered
  fragments remain; 121 sub-nozzle remnants are removed.
* This cleanup is offline-tested against the captured export. Alignment and
  surface quality still require a fresh Orca export and physical print.

## 0.0.11 — 2026-10-01

* Replaced the unreliable two-stage Orca slice-object design with one
  transactional exported-G-code pass. It no longer depends on an internal
  Polygon constructor, `slice_z`, `print_z`, object-to-bed calibration, or an
  in-memory plan surviving between Orca callbacks.
* The pass reconstructs the preceding layer's support footprint and each
  `Bridge` / `Internal Bridge` footprint from Orca's actual extrusion moves,
  then propagates wavefronts only through unsupported bridge area.
* Original bridge extrusion is subtracted only where buffered wave paths cover
  it. Every uncovered fragment is re-emitted. Any parsing or generation error
  returns the original G-code unchanged.
* Tested against the owner's real 0.30 mm `Cube^2_3m53s.gcode`: three layers
  receive wave blocks, 112 covered bridge moves are replaced, 158 uncovered
  fragments remain, fan state is restored, and a second pass is a no-op.
* This is offline proof against the real export, not yet proof from a new Orca
  export or a physical print.

## 0.0.10 — 2026-10-01

* Fixed the two failures measured with the owner's `Cube^2.STL` at 0.30 mm:
  wave plans now use Orca's exported `print_z` instead of its offset internal
  `slice_z`, and replacement geometry is built with Orca's supported empty
  `Polygon()` plus `append(Point)` API.
* The failed 0.0.9 export planned four wave layers but matched and inserted
  none, while carving failed safely. This release is intended to make those
  four layers match and remove that constructor error.
* Unsafe early slice carving is disabled. It happened before insertion could be
  proven, so a later splice failure could have left a print hollow. This test
  release keeps the original bridge while we verify wave insertion; final
  replacement will remove only G-code bridge moves covered by inserted waves.
* Bridge replacement is still not claimed working until a new real-Orca export
  contains wave blocks and proves geometrically bounded bridge removal.

## 0.0.9 — 2026-09-30

* **The permanent package name is now simply `Wave Overhangs`.** Release
  numbers will never be placed in the package or capability names again. Read
  Orca's separate Version column for the installed release.
* This is the final naming migration. Reselect `Wave Overhangs` once after
  updating; future releases keep that exact identity.
* Replacement remains the chosen behavior. Safe same-export removal of only
  bridge extrusion covered by successfully inserted waves is still open and
  is not claimed working in this release.

## 0.0.8 — 2026-09-30

* **Restored `Wave Overhangs v0.0.6` as the permanent compatibility
  identity.** Version 0.0.7 changed the identity and could disconnect Orca's
  saved pipeline selection/configuration just as 0.3.2 did to Unlayered
  Infill. The actual release remains visible as 0.0.8 in Orca's Version
  column, Check setup, logs, G-code stamp, and updater output.
* Confirmed the desired print behavior: replacement, not reinforcement. Waves
  must be generated and inserted successfully before only the covered bridge
  extrusion is removed. A failure must leave the original bridge untouched.
* A successful first export now says plainly that the original bridge was kept
  by the safety gate and instructs you to slice/export once more. Later exports
  report that replacement carving is enabled.
* The same-export replacement surgery is still under development and is not
  claimed as working in this release.

## 0.0.7 — 2026-09-30

* **The plugin name is now permanently `Wave Overhangs`.** Orca's development
  guide says a process preset saves the plugin name as part of its full
  capability reference. Putting the version in that name could leave a preset
  pointing at yesterday's identity, so the plugin appeared installed and
  selected but was never called. The version remains visible in Orca's Version
  column, Check setup, logs, G-code stamps, and updater output.
* The PEP 723 dependency declaration and plugin structure were audited against
  the repository's OrcaSlicer Plugin Development PDF. Orca's bundled `uv`
  installer remains responsible for installing `numpy` and `shapely`; the
  plugin does not run `pip` itself.
* After updating, select Wave Overhangs once more in the process preset so Orca
  saves the stable reference.

## 0.0.6 — 2026-09-30

* **Fixed: the plugin could fail to load, showing up in the Plugins list as
  failed or disabled.** v0.0.5 wrote its first log line while OrcaSlicer was
  still loading the plugin. Orca watches file activity during loading, so that
  write could either throw a permission prompt at you mid-install or stop the
  plugin loading altogether. The first log line now waits until the plugin is
  actually used.
* A log line that cannot be written can no longer interfere with a slice under
  any circumstances.

## 0.0.5 — 2026-09-30

* **A readable log now lands in your Downloads folder** as
  `orca-plugins.log`, shared with Unlayered Infill. It records whether the
  plugin loaded, whether it was selected in your preset, and whether the
  export step ran. Set `"log": false` in the capability config to switch it
  off.

## 0.0.4 — 2026-09-30

* **The version is now part of the plugin's display name**, so the Plugins
  dialog shows exactly which build is installed.
* **Check setup prints the running version** as its first line.

## 0.0.3 — 2026-09-30

* First version published in this repository, installable with
  `Update-Orca-Plugins.bat`.
* Experimental: aims to print steep overhangs support-free by replacing the
  overhang region with wave-propagated toolpaths.
* Port of the WaveOverhangs idea from Dennis Klappe's OrcaSlicer fork.
* Requires `numpy` and `shapely`, which OrcaSlicer downloads for you the first
  time the plugin is installed. If that download fails the plugin will not
  load — fully quit and reopen OrcaSlicer to let it retry.
