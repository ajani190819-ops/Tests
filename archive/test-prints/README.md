# Archived test prints and exports

Real files from the owner's printer and OrcaSlicer, kept as **evidence**. They
are not test fixtures — nothing in `tests/` reads them, and no code depends on
them. They live here so a future session can re-check a claim instead of
taking a changelog's word for it.

If you are looking for the file the automated tests actually use, that is
`tests/fixtures/Cube^2_3m53s.gcode`, and it stays where it is.

Nothing here should be deleted. Several changelog and `MEMORY.md` entries cite
these files by name, and those numbers cannot be reproduced without them.

## What each file is

### `test print.stl` / `test print.3mf` / `test print_19m50s.gcode`

The part behind the **0.0.28 and 0.0.29** work, uploaded by the owner on
2026-10-02. The `.3mf` is the OrcaSlicer project (model plus the settings that
were in force); the `.stl` is the bare model; the `.gcode` is the export with
Wave Overhangs **0.0.27** applied.

This is the single most useful file here. Things measured from it:

* The `19m50s` in the filename is **OrcaSlicer's own estimate before the
  plugin ran**. The preview of the finished file reported **1h46m**, so the
  plugin was turning a 20-minute print into a 106-minute one.
* Walking every move gives 104.0 min, which matches that 1h46m: wave fill
  58.8 min, **373 moves stranded on the wave speed 32.9 min**, normal Orca
  moves 11.6 min, in-block travel 0.6 min.
* All **342** "replaced covered bridge move" lines carried **no feedrate**,
  which is the 0.0.28 bug.
* Total extrusion is **7.62 g**, of which the wave blocks are **1.50 g (20%)**
  — the evidence that the "gram usage is too high" worry was a false alarm.
* The owner's bridge speed reads as **F1200 = 20 mm/s** here, which is what
  0.0.29's `print_speed = "orca"` picks up. (They have since changed it to
  10 mm/s in their profile; the plugin follows whatever the profile says, so
  that number is not baked in anywhere.)

### `Cube.stl` / `Cube_39m10s.gcode`

An older real export, from roughly the **0.0.20** era. 91,355 lines with three
`; ==== WAVE OVERHANG BEGIN ====` blocks. This was the first file to show the
feedrate leak, independently of the part above: **445 of 445** replacement
moves with no feedrate, and 111 moves stranded at 2 mm/s wasting 5.5 minutes.

It is also the file that **ruled out** the wrong theories: all 186
non-extruding moves *inside* its wave blocks correctly carry `F7200`
(120 mm/s), so in-block travel was never the problem.

### `debug_Fri_Oct_02_01_59_10_40336.log.0`

OrcaSlicer's debug log from the same session. Worth keeping only to record
that it contains **no plugin error** — just a notice that a second OrcaSlicer
instance was already running and this one exited. Checked so nobody has to
chase it again.
