# Changelog — Undahang

The in-app Check setup view carries these three current releases. Full version history is preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).

## 0.0.55 — 2026-11-16

* Keeps speed-segmented overhanging walls intact after the waves: standalone
  G0/G1 F changes no longer split a continuous wall's retract/wipe feature.
* Matches copied wall retraction to the source entry state, carries that
  state between moved walls, and primes at the wave resume point. Preserves
  original wall commands, entry/exit feed and net extrusion without leaving
  a ghost retract/wipe cycle at the old seam.
* Refuses unknown firmware/tool retraction state, mixed XY/E retract plumbing,
  unbalanced primes, unsafe feeds and absolute/nonprimed insertion points.
* The owner's older 0.0.53 output confirms the missed speed-segmented case.
  Exact wall-block diagnostics and synthetic fresh-export checks are not a
  new owner export, native-host trial or physical print/curl acceptance.
  The existing S127 primary fan ceiling and preset read-only policy remain.

## 0.0.54 — 2026-11-16

* Moves eligible whole overhanging wall loops after the waves even when only
  a small portion of the loop is unsupported; removes the 25%-of-loop blind spot.
* Caps enabled export-pass primary part-fan M106 requests at S127 (below 50%
  of 255): Auto, explicit values, restoration and original slicer commands.
  Lower/off requests survive; unrelated fan channels are not changed.
* Preserves layer Z, balanced retract/wipe plumbing, final wipe XY and modal
  feed at wall handoffs. Unsupported fan syntax/write failures refuse export
  rather than reporting an uncapped success.
* PWM duty is NOT voltage conversion or 12 V fan protection. Curl's cause and
  the owner's exact failing strand remain unconfirmed; no physical fix claimed.

## 0.0.53 — 2026-11-16

* Adds a grouped settings form with every tuning control visible, units,
  inline guidance, explicit global/preset scope and a value-preserving JSON fallback.
* Settings guide and setup check report active prerequisites without inventing
  universal process fixes. Printer/filament profiles remain read-only; no
  selection rewiring, automatic preset saves or toolpath experiments.
* Labels backed-up guide values honestly and keeps engines, tuning defaults,
  identities and slicing callbacks unchanged. No native UI/print claim.

## 0.0.52 — 2026-11-16

* Introduces Undahang branding in the description, setup report and logs while
  preserving the Wave Overhangs package/capability identities used by presets.
* Keeps Python/storage paths, settings recovery, output stamps, attribution and
  all toolpath behavior unchanged; Unlayered Infill stays separate.

## 0.0.51 — 2026-10-20

* Fixes settings recovery when Orca clears the Config panel to `{}` after
  an update, makes `restore_backup: true` work immediately mid-session, and
  stops `Wave Overhangs - Settings guide & check` from writing factory
  defaults over the settings backup history.
* Honours JSON string booleans (`"enabled": "false"`, `"wall_snap": "false"`)
  and writes rewritten G-code files atomically via a same-directory temp file.

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
