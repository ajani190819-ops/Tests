# Test prints

Real exports and the models they came from. These are the ground truth for
everything the plugins do: a captured G-code file is the only thing in this
repo that proves a change behaves on a real slice rather than on a synthetic
case someone wrote to pass.

## What goes here

One folder per model:

```
test-prints/
  cube-squared/
    Cube^2.stl
    Cube^2_3m53s.gcode        <- exported with the plugin OFF
    notes.md                  <- printer, nozzle, layer height, what to look at
  holes/
  curved-perimeters/
```

**Export with the plugins switched off** unless the file is specifically
there to show a plugin's output. A clean export is reusable as an input; a
processed one is only evidence of one run.

`notes.md` should say, in one or two lines: the printer and nozzle, the layer
height, and **what you are supposed to look at** -- "the wall snapping is
jagged on the curved side", "the nozzle scans across after the waves". That
is the part nobody remembers six months later.

## How the tests use them

`tests/fixtures/Cube^2_3m53s.gcode` is the one the regression suite reads
today, and it stays where it is so the tests keep working. Files added here
are picked up by name in a test that asks a specific question of a specific
export -- see the "real Cube^2 export" section of `tests/test_wave_gcode.py`
for the pattern.

## Size

G-code is large and compresses well, but it is still committed as plain text
so a diff is readable when a fixture changes. Keep single files under a few
MB; if a model needs more than that, cut the print down to the layers that
show the problem.
