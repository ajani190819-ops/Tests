# Changelog — Unlayered Infill

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog in its separate Version column, and running
**Unlayered Infill - Check setup** prints it with a short version history.

Dates are the day the change was made, not a release date.

## 0.4.7 — 2026-10-02

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

## 0.4.6 — 2026-10-02

* wave_angle now defaults to "auto": the ripples run square across your
  profile's infill angle, read from the export. A wave only does interlocking
  work where an infill line CROSSES it, so this is where the feature earns
  its keep -- with the common 45 degree infill the ripples now run at 135
  instead of straight along X.
* max_lift_mm now defaults to "auto", a ceiling of 1.5 layer heights on the Z
  offset. At the shipped amplitude nothing is clamped by it (the wave peaks
  at one layer height), so this is a safety net against a big amplitude
  driving the nozzle into material that is already printed, not a change to
  how the part looks.

With these two, every setting that can be derived from the print now is:
frequency, segment_mm, blend_mm and cell_mm from the nozzle, wave_angle from
fill_angle, max_lift_mm from the layer height.

**What stays a fixed number.** amplitude, pattern, shape, layer_phase and the
switches. Nothing in the G-code implies how strongly you want the layers
keyed together or what the ripple should look like.

## 0.4.5 — 2026-10-02

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

## 0.4.4 — 2026-10-02

**A settings panel you can actually read, and settings that follow your
printer.** Nothing about the wave itself changed; on a 0.4 mm nozzle this
produces the same G-code as 0.4.3, to the digit.

**The panel is grouped now.** Five numbered sections — BASICS, THE SHAPE OF
THE WAVE, LIMITS AND SAFETY, RESOLUTION AND SMOOTHING, DIAGNOSTICS — instead
of fourteen keys in no particular order. Every note is one short sentence
shaped `accepted values -- what it does`, because a JSON editor renders a
newline as the two characters `\n` and a paragraph-long note is a smear. The
long explanations moved to the README, where there is room for them. The
panel order and the "Check setup" guide order are now generated from the same
list, so they cannot disagree.

**Your values survive the upgrade.** The migration that landed in 0.4.2 is
unchanged: on the first slice after updating, your saved config is merged
with this build's, keeping every value you set. Amplitude at 300% stays at
300%. Only missing keys are added, and the notes are refreshed.

**`segment_mm`, `blend_mm` and `frequency` now default to `"auto"`.** These
were never really constants, they were multiples of the nozzle that happened
to be written down for a 0.4:

| setting | auto means | 0.4 nozzle | 0.6 nozzle |
| --- | --- | --- | --- |
| `segment_mm` | 2.5 x nozzle | 1.00 mm | 1.50 mm |
| `blend_mm` | 5 x nozzle | 2.00 mm | 3.00 mm |
| `frequency` | one ripple per 10.5 nozzle widths | 1.50 /mm | 1.00 /mm |

So a 0.4 nozzle gets exactly what it got before, and a 0.6 finally gets
settings that suit a 0.6 instead of settings that suit somebody else's
printer. Pinned by `tests/test_plugin_runtime.py`.

**Two more settings accept `"auto"`, but are still off by default** because
turning them on changes how a part prints and that should be your decision:

* `wave_angle: "auto"` reads `fill_angle` from the export and runs the
  ripples square across your infill — which is where they do the most work,
  since a line parallel to the ripples never crosses one.
* `max_lift_mm: "auto"` caps the Z offset at 1.5 layer heights.

**Check setup and the log now print what each auto resolved to**, with the
reasoning — `frequency : 0.997 ripples/mm (auto: one ripple every 10.5 x
0.60 mm nozzle widths)` — rather than echoing the word "auto" back at you.

## 0.4.3 — 2026-10-02

**Fixes the failure you get after pressing Refresh in the Plugins dialog**,
where Wave Overhangs comes back fine and Unlayered Infill does not. Two
causes, both in this plugin only, which is why only this one broke.

1. **An import that happened inside OrcaSlicer's audit scope.** The engine
   worked out your layer height with `statistics.multimode`, and it imported
   `statistics` *the first time that line ran* — i.e. inside a capability
   call. Orca's audit hook is off while a plugin is being imported and ON
   during a capability call, where every file open is audited. A first-use
   import inside that scope is an audited read of a file the plugin never
   declared; it is the same shape as the numpy failure in OrcaSlicer issue
   #15944. `statistics` is now imported at module load with everything else,
   where the hook is not watching. See docs/ORCA-PLUGIN-FACTS.md, "The audit
   hook".

2. **Re-importing the plugin could destroy its own engine.** Refresh re-runs
   discovery and imports the plugin module again in the same interpreter.
   The old code published an empty `nonplanar_core` into `sys.modules`
   *before* executing the engine into it, and removed it on failure — so a
   second pass could replace a working engine with nothing and leave the
   plugin reporting "engine MISSING". The new module is now built off to one
   side and published only once it has executed cleanly; if it cannot, the
   engine that already worked is kept.

Also: when the engine genuinely cannot load, the reason is now printed in
Check setup and in the failure message, instead of a bare "MISSING".

No change to the G-code this plugin produces. The stamp version moves to
v0.4.3 with the release, as always.

## 0.4.2 — 2026-10-02

**The missing settings now repair themselves.** If `pattern`, `shape`,
`wave_angle`, `layer_phase` or `max_lift_mm` are not in your Config panel,
this release puts them there.

0.4.1 told you the settings were missing and gave you a recipe that was based
on a wrong idea of where OrcaSlicer keeps them. Both have been fixed.

**What was actually going on.** OrcaSlicer stores each capability's settings
in one global file, `orca_plugins/config.json`, and the Config tab shows you
that saved copy. A copy written while you were on 0.3.4 has 0.3.4's nine
settings in it, so the five added since never appear — even though the plugin
is running them. (A preset can also hold its own override, which wins while it
is there; the old advice about setting the preset to None and back was aimed
at a storage model that does not exist.)

**What this release does about it.** OrcaSlicer documents a migration hook for
exactly this, and the plugin now implements it. When it finds a configuration
written by an older build it merges this build's settings into it and saves it
back:

* every value you had set is kept, untouched;
* settings this build added appear at their default;
* the plain-English notes are refreshed to describe the code you are running;
* anything it does not recognise is left in place rather than deleted.

It happens when OrcaSlicer loads the plugin, and again on the first slice or
the first **Check setup** run of a session, whichever comes first — so one
slice is always enough. Reopen the Config tab afterwards.

If it still looks short, **Check setup** now gives the correct manual fix:
Plugins dialog → *Config* tab → **Restore defaults**, which deletes the saved
copy so the panel falls back to this build's full defaults. The only thing you
lose is your plugin settings.

Nothing about slicing changed. Exports from 0.4.2 are identical to 0.4.1's.

## 0.4.1 — 2026-10-02

> **Superseded by 0.4.2.** The explanation below — that Orca copies the
> settings into your *process preset* — was wrong, and so was the None-and-back
> recipe. The settings are stored globally in `orca_plugins/config.json`, and
> a stale copy *can* be repaired in code, which is what 0.4.2 does.


Explains why new settings can be missing from the Settings panel.

> "I'm not seeing all of those new config options for unlayered infill."

Nothing was wrong with the plugin: all 14 settings are there, and 0.4.0's four
new ones (`pattern`, `wave_angle`, `shape`, `layer_phase`) are handed to
OrcaSlicer every time it asks. The catch is that **Orca only asks once**. When
you first pick a plugin, Orca copies its settings into your process preset and
from then on shows you that saved copy. Settings added by a later version are
simply not in it, so they never appear — and there is nothing the plugin can
do about it from its side.

The prints themselves were never affected: anything missing from the saved
copy is filled in from the plugin's own defaults before a single line of
G-code is touched. You just could not see or change those settings.

So **Check setup** now tells you this directly. It prints how many settings
the installed build has, and a "not seeing all the settings?" section with the
fix:

1. Process preset → Others → Slicing Pipeline Plugin
2. Set it to None, then back to Unlayered Infill
3. Save the process preset

If they are still missing after that, the installed *file* is an old one — run
`Orca-Plugins.bat` again and fully quit and reopen OrcaSlicer.

This section prints even when you have turned the long settings guide off,
because it is exactly what you need when the panel looks wrong.

## 0.4.0 — 2026-10-02

Five new controls that decide the SHAPE of the wave, not just its size.
**Nothing changes unless you change a setting**: every new control defaults to
exactly what 0.3.4 did, and the test suite checks that by comparing real
G-code, not by reading the defaults.

* **The wave can now run in both directions at once (`pattern`).** Until now
  the infill rode `sin(frequency x)` — a ripple that varies along X and only
  along X. That has a hole in it. An infill line running along Y crosses no
  ripple at all: every point on it has the same X, so the whole line is
  lifted to one height and set down flat. It keys into nothing. With the
  usual 45-degree infill you lose a little; with 0/90-degree infill, half
  your infill was doing no interlocking work. Set `pattern` to `cross` and
  the wave becomes an egg-crate rippling along both axes, so a line running
  in ANY direction still rises and falls.

* **`wave_angle` aims the ripples.** Degrees, counter-clockwise, 0 being
  along X — the old behaviour. Turn them across your infill lines so every
  line crosses them. If you print 45-degree infill, try 45.

* **`shape` changes the profile: `sine`, `triangle` or `square`.** Triangle
  has straight flanks and sharp peaks, so layers key together harder for the
  same height. Square keeps most of the infill at full offset with short
  ramps between crests. Square is a SATURATED sine, never a vertical step:
  no printer can move Z instantly, so a true square wave would just be a
  skipped step and a scar. All three share their zero crossings and peak
  positions, so swapping between them changes the character of the wave
  without moving it.

* **`layer_phase` stops every layer being a copy of the one below.** At 0 —
  the old behaviour — each layer puts its crest at the same place, so the
  part ends up with a column of crests stacked on top of each other. Give it
  a small advance per layer and the crests walk sideways as the part grows,
  which is the thing that actually braids layers together. 180 puts each
  crest directly over the trough beneath it; 360 is a full turn and does
  nothing.

* **`max_lift_mm` is a hard ceiling on the Z movement.** In millimetres,
  whatever amplitude and taper work out to; 0 means no ceiling. This is a
  safety net. The default amplitude is a SHARE OF LAYER HEIGHT, so raising
  layer height or amplitude without thinking can drive the nozzle up into
  material it has already printed. Set this and it cannot, and the run
  report tells you how many segments it caught.

* **Every setting now explains itself inside OrcaSlicer.** The config panel
  carries a plain-English note above each setting, and **Unlayered Infill -
  Check setup** prints the full guide after the usual diagnostics — what each
  control does, in words, wrapped to fit Orca's message box. Set
  `settings_guide` to false on that item once you know them. OrcaSlicer only
  lets an item read its own settings, so the guide shows the defaults and
  says so, rather than printing a default and calling it your value.

* The standalone tool gained the matching `--pattern`, `--angle`, `--shape`,
  `--layer-phase` and `--max-lift` options, and its report now names the wave
  it used.

## 0.3.4 — 2026-09-30

* **The permanent package name is now simply `Unlayered Infill`.** Release
  numbers will never be placed in the package or capability names again. Read
  the separate Version column for the installed release.
* The complete 0.3.0 control set is pinned as the default configuration:
  percentage amplitude (`200%`), frequency, segment length, automatic
  nozzle-width grid, blending radius, and full-strength switch. The controls
  no longer depend on finding a configuration slot with an old versioned name.
* This is the final naming migration. Reselect `Unlayered Infill` once after
  updating; future releases keep that exact identity.

## 0.3.3 — 2026-09-30

* **Restored the `Unlayered Infill v0.3.0` compatibility identity.** Changing
  the plugin name in 0.3.2 made Orca look in a new configuration slot, so the
  owner's saved percentage amplitude, nozzle grid, blending, frequency, and
  full-strength controls appeared to stop working. The actual code was still
  present; this reconnects Orca to the configuration that worked in 0.3.0.
* The current import-safety, logging, G-code, and updater fixes remain. This is
  not a rollback to the unsafe 0.3.0 source.
* The real release remains visible as 0.3.3 in Orca's Version column, Check
  setup, logs, updater output, standalone tool, and G-code stamp.

## 0.3.2 — 2026-09-30

* **The plugin name is now permanently `Unlayered Infill`.** Orca's
  development guide says a process preset saves the plugin name as part of its
  full capability reference. A version inside that name changed the saved
  identity every release. The version remains visible in Orca's Version
  column, Check setup, logs, G-code stamps, the standalone tool, and updater
  output.
* After updating, select Unlayered Infill once more in the process preset so
  Orca saves the stable reference.

## 0.3.1 — 2026-09-30

* **Fixed: the plugin could fail to load, showing up in the Plugins list as
  failed or disabled.** v0.3.0 wrote its first log line while OrcaSlicer was
  still loading the plugin. Orca watches file activity during loading, so that
  write could either throw a permission prompt at you mid-install or stop the
  plugin loading altogether. The first log line now waits until the plugin is
  actually used.
* **Fixed:** the updater was putting the standalone tool in the same folder as
  the plugin copies. Orca needs exactly one `.py` file per plugin folder, so a
  stray second file can stop a plugin loading. Tools now go in
  `Downloads\OrcaPlugins\tools\`.
* A log line that cannot be written can no longer interfere with a slice under
  any circumstances.

## 0.3.0 — 2026-09-30

* **Amplitude is now a percentage of layer height**, default `"200%"`. The old
  fixed millimetre value meant the effect scaled wrongly whenever you changed
  layer height. You can still give a plain number for an absolute millimetre
  amplitude.
* **The grid is now one nozzle wide by default** (`cell_mm: "auto"`). Infill is
  grouped into columns the width of your nozzle, so neighbouring extrusions
  move together instead of tearing away from each other.
* **A readable log now lands in your Downloads folder** as
  `orca-plugins.log`, so you can tell what the plugin did without guessing.
  Set `"log": false` in the capability config to switch it off.
* The standalone tool gained the same defaults, so the plugin and the
  script behave identically.

## 0.2.1 — 2026-09-30

* **The version is now part of the plugin's display name**, so the Plugins
  dialog shows exactly which build is installed.
* **Exported G-code carries a version stamp**, so a saved file records which
  version produced it.
* **Check setup prints the running version** as its first line.

## 0.2.0 — 2026-09-30

* First version published in this repository, installable with
  `Update-Orca-Plugins.bat`.
* Non-planar sparse infill: rides a sine wave in Z so successive layers
  interlock instead of stacking as clean planes, tapering flat where it meets
  the solid skin.
* Derived from Roman Tenger's NonPlanarInfill (GPL-3.0).
