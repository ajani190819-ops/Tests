# Unlayered Infill (OrcaSlicer pipeline plugin)

**Experimental.** Rewrites sparse infill so it rides a sine wave in Z:
`dz = amplitude * scale * sin(f * x)`. Successive layers interlock instead of
stacking as clean planes, so the part is no longer only as strong as its
weakest layer boundary. The wave tapers to flat where it meets the solid
skin above or below.

Adapts Roman Tenger's NonPlanarInfill (GPL-3.0,
https://github.com/TengerTechnologies/NonPlanarInfill). The simpler
predecessor tool is kept at `tools/nonplanar-infill-tool/`.

## Install

Run `Orca-Plugins.bat` (repo root). It lands here:

```
<Orca data dir>\orca_plugins\UnlayeredInfill\unlayered_infill_orca.py
```

## Use

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. **Enable "Use relative E distances"** (Printer Settings → Advanced).
   The plugin refuses absolute-E (M82) G-code rather than corrupt it.
3. Process preset → Others → **Slicing Pipeline Plugin** → *Unlayered Infill*.
4. If Orca preserved an old configuration, open the plugin settings and use
   **Restore defaults**. This keeps the complete working control set while
   clearing stale saved values.
5. Slice, then run **Unlayered Infill - Check setup** (Plugins dialog) — it
   reports the running version on its first line, then whether the export
   step actually fired.

The plugin appears as *Unlayered Infill* in the Plugins dialog. Its separate
Version column currently reads **0.4.2**, and exported G-code is stamped with
`; unlayered-infill v0.4.2`. The package and capability names stay version-free
so an update never orphans your process preset.

## Configuration

| Key | Default | Notes |
| --- | --- | --- |
| amplitude | `"200%"` | **a share of the layer height**: 200% of a 0.3 mm layer is 0.6 mm. Also accepts `2x` and plain mm (`0.6`). Because it is relative, it keeps meaning when you change layer height. |
| frequency | 1.5 | ripples per mm |
| segment_mm | 1.0 | move subdivision length |
| cell_mm | `"auto"` | width of the solid-skin grid columns. `auto` = **one column per nozzle diameter**, read from the G-code. A number forces a fixed size. |
| blend_mm | 2.0 | smooths the taper across neighbouring columns |
| full_strength | false | classic taper peaks at 0.5; this reaches 1.0 mid-span |
| pattern | `"linear"` | `linear` ripples along one direction only; `cross` is an egg-crate rippling along **both**, so an infill line running in any direction still rises and falls |
| wave_angle | 0.0 | degrees to turn the ripples, counter-clockwise, 0 = along X. Aim them across your infill lines |
| shape | `"sine"` | `sine`, `triangle` (straight flanks, sharper peaks) or `square` (flat crests, short ramps — a saturated sine, never a Z step) |
| layer_phase | 0.0 | degrees of extra phase per waved layer, so crests walk sideways instead of stacking. 180 puts a crest over the trough below; 360 is a full turn and does nothing |
| max_lift_mm | 0.0 | hard ceiling on the Z displacement in mm, whatever amplitude and taper work out to. 0 = no ceiling |
| require_relative_e | true | refuse M82 rather than corrupt it |
| log | true | append a readable record of every run to the plugin-storage `orca-plugins.log` |

Set these per process preset via the plugin's config in Orca. Defaults are
used when you set nothing.

### Which direction does the wave run? (new in 0.4.0)

This is the setting most worth understanding. Up to 0.3.4 the displacement was
always `sin(frequency × X)` — a ripple that varies along X, and **only** along
X. Picture a washboard whose ridges all run north-south.

Now picture an infill line running north-south too, straight along one ridge.
Every point on that line has the same X, so it gets the same displacement: the
whole line is lifted to one height and set down flat. It is not waved at all,
and it keys into nothing above or below it.

With the usual 45°/135° alternating infill, every line crosses the ridges at an
angle and the cost is small. With 0°/90° infill, half your infill lines run
along the ridges and do no interlocking work whatsoever.

Three ways to deal with it:

* **`pattern = "cross"`** — the wave becomes an egg-crate rippling along both
  axes at once. A line running in any direction crosses bumps. This is the
  simplest fix and the one to reach for first.
* **`wave_angle`** — keep the single-direction ripple but turn it so your
  infill lines cross it. For 45° infill, `wave_angle = 45`.
* Change the infill angle in Orca so it is not parallel to the ripples.

`cross` averages its two axes rather than adding them, so the displacement
still never exceeds the amplitude you asked for.

### Which shape, and why there is no true square wave

`sine` is smooth. `triangle` has straight flanks and sharp peaks, so for the
same peak height the layers key together harder. `square` holds most of the
infill at full offset with short ramps between crests.

`square` is a **saturated sine**, not a real square wave — the crests are flat
but the transitions are ramps. A true square wave would ask the nozzle to
change Z instantly, which no printer can do: you would get a skipped step, a
layer shift, or a gouge. All three shapes share their zero crossings and peak
positions, so switching between them changes the character of the wave without
moving it.

### Why advance the phase each layer

At `layer_phase = 0` every layer puts its crest at the same XY, so the part
ends up with a column of crests stacked on top of one another. That still
interlocks — the taper makes each layer's wave a different size — but the
weakness runs in a line. A small advance per layer walks the crests sideways as
the part grows, which is what actually braids the layers. 180° puts each crest
directly over the trough below it. 360° is a full turn and changes nothing.

### Why there is a Z ceiling

`amplitude` defaults to a *share of layer height*, which is what makes it keep
meaning when you change layer height — but it also means the absolute movement
grows when you do. `max_lift_mm` is a hard ceiling in millimetres applied after
the amplitude and taper are worked out, so the nozzle cannot be driven up into
material it has already printed. 0 means no ceiling. The run report says how
many segments it caught, so you can tell the difference between "the ceiling is
protecting me" and "the ceiling is flattening my wave".

### Why amplitude is a percentage

A fixed `0.2 mm` means something different on a 0.1 mm layer than on a 0.3 mm
one. A percentage of layer height is the same *relationship* at any layer
height, so you tune it once. The engine reads the layer height from the
G-code (`;HEIGHT:`, falling back to the `layer_height =` config line).

### Why the grid is one nozzle wide

Solid skin is rasterised into XY columns, and each column records only the Z
heights that are solid **in that column**. Every infill move is then bracketed
by its own local floor and roof, so a ledge, a bridge, or a short neighbouring
tower cannot distort the taper anywhere else in the part — the failure mode
you get from a single global list of solid heights. One column per nozzle
width is the finest grid that still corresponds to something the printer can
actually lay down.

## The log

By default, the plugin writes `orca-plugins.log` in its Orca plugin storage
folder. Orca allows writes there without prompting during slicing. Run
**Unlayered Infill - Check setup** to print the exact path, or use Orca's
Plugins dialog → Show in folder.

It rolls over at about 1 MB. `ORCA_PLUGIN_LOG_DIR` is only a debug override;
pointing it outside plugin storage can reintroduce approval prompts. Set
`"log": false` to turn it off. A run looks like this:

```
2026-10-02 21:14:02  Unlayered Infill v0.4.2 loaded (engine ok)
2026-10-02 21:14:19  Unlayered Infill v0.4.2: EXPORT STEP RUNNING
                       file         : C:\Users\you\AppData\Local\Temp\x.gcode
                       settings     : amplitude='200%' frequency=1.5 cell_mm='auto'
2026-09-30 21:14:20  Unlayered Infill: DONE -- the G-code was rewritten
                       layer height : 0.300 mm
                       nozzle       : 0.600 mm
                       grid columns : 0.600 mm (auto: one column per nozzle width)
                       amplitude    : 200% of layer height 0.300 mm = 0.600 mm
                       solid skin   : 4 layer height(s) over 1155 column(s)
                       infill runs  : 8
                       moves waved  : 72  ->  1440 segment(s)
                       largest Z    : 0.267 mm
```

Read it as a ladder:

| What you see | What it means |
| --- | --- |
| no file at all | the plugin is not installed, or Orca never loaded it |
| `loaded` only | installed, but never selected in a process preset — or you never sliced |
| `pipeline step '...' seen` but no export step | selected and running, but you pressed **Print/Send** instead of **Export G-code file** |
| `EXPORT STEP RUNNING` then `NOTHING CHANGED` | it ran; the log names the reason |
| `EXPORT STEP RUNNING` then `DONE` | it worked |
| `REFUSED` | usually absolute E — turn on relative E distances |

## How the taper works

Solid extrusions are rasterised into XY columns `cell_mm` across. Each column
records the Z heights that are solid in that column, so each infill move is
bracketed by its own local floor and roof: `scale = min(d_above, d_below) /
span`, peaking at 0.5 mid-span (1.0 with `full_strength`), then
`dz = amplitude * scale * sin(frequency * x)`.

A single global list of solid heights — the obvious implementation, and what
the upstream script does — is wrong on any part whose skins are not flat
planes across the whole footprint (two towers of different heights: the
short one's roof pinches the tall one flat). Per-column bracketing fixes it;
`tests` pin it.

## Engine fixes vs the reference tool

The engine (inlined as `nonplanar_core`) improves on the reference tool in
six test-pinned ways: extrusion attached to the right move, no duplicated
points (no blobs), Z restored on exit, Orca's skin markers recognized, taper
can't invert above the topmost solid, and per-column bracketing.

## Standalone version

`unlayered_infill_post.py` (in this folder) runs the **same engine** on an
already-exported `.gcode` file, with no plugin system involved. Double-click
it for a window, or:

```
python unlayered_infill_post.py part.gcode          # writes part_unlayered.gcode
python unlayered_infill_post.py -n part.gcode       # report only, write nothing
python unlayered_infill_post.py -s part.gcode       # full strength, obvious wave
python unlayered_infill_post.py --inplace part.gcode   # for slicer post-processing
```

Use `--inplace` in Process → Others → *Post-processing scripts*.

The engine source is stored verbatim in both files.
`tests/test_post_script.py` fails if the two copies drift, so a fix always
lands in both.

### Why the preview looks unchanged

It always will. Orca builds the preview from the slice; post-processing runs
afterwards, at export, and nothing redraws the preview
([OrcaSlicer#7489](https://github.com/OrcaSlicer/OrcaSlicer/issues/7489)).
Post-processing also only runs on **Export G-code file** — not on Print or
Send ([#4432](https://github.com/SoftFever/OrcaSlicer/issues/4432)).
To see the result, drag the exported file back into OrcaSlicer.

## Licence

GPL-3.0 (derivative of NonPlanarInfill).
