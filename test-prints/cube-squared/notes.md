# Cube^2

The original test print, moved here from `tests/fixtures/` (a copy stays
there because the regression suite reads it by that path).

* 0.6 mm nozzle, 0.3 mm layer height, relative E, arc fitting off.
* Profile resolution 0.06 mm, travel 120 mm/s, bridge speed 20 mm/s.
* Two stacked boxes: the upper one overhangs on all four sides, so layer 17
  is a full ring of unsupported bridge with an overhanging wall around it.

What to look at: the waves on layer 17, the overhanging wall that is now
printed after them, and the retained bridge fragments the waves did not
cover.
