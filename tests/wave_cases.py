"""Synthetic G-code cases shared by the Wave regressions and diagnostics.

These are hand-built exports, not captured ones: they exist so a specific
geometric situation (an overhang with a hole in it, for example) can be
tested exactly, next to the real captured Cube export.
"""
import math


def synthetic_overhang_with_hole(arc_walls=False):
    """A layer that overhangs to the right and has a round hole in it.

    The layer below is a 10 x 20 block, so x > 110 hangs in the air. Bridge
    lines stop short of the walls by a varying amount, exactly like a real
    export, and they also keep a varying margin around the hole.

    With arc_walls=True the hole's wall is written as G2/G3 arc moves, which
    is what Orca exports once "Arc fitting" is switched on in the print
    profile. Wave has to read that as the same circle.
    """
    hole_x, hole_y, hole_r = 115.0, 110.0, 3.0

    def loop(points, kind="Outer wall"):
        block = [f";TYPE:{kind}", ";WIDTH:0.50", "G0 X%.3f Y%.3f" % points[0]]
        for x, y in points[1:]:
            block.append(f"G1 X{x:.3f} Y{y:.3f} E0.50")
        return block

    def circle(radius, n=72):
        return [(hole_x + radius * math.cos(2 * math.pi * i / n),
                 hole_y + radius * math.sin(2 * math.pi * i / n))
                for i in range(n + 1)]

    text = ["M83", ";Z:0.3"]
    text += loop([(100.25, 100.25), (109.75, 100.25), (109.75, 119.75),
                  (100.25, 119.75), (100.25, 100.25)])
    text += [";TYPE:Internal solid infill", ";WIDTH:0.45", "G0 X100.6 Y100.7"]
    y = 100.7
    while y <= 119.3:
        text.append(f"G1 X109.4 Y{y:.3f} E0.40")
        text.append(f"G0 X100.6 Y{y + 0.45:.3f}")
        y += 0.45
    text += [";Z:0.6"]
    text += loop([(100.25, 100.25), (119.75, 100.25), (119.75, 119.75),
                  (100.25, 119.75), (100.25, 100.25)])
    if arc_walls:
        radius = hole_r + 0.25
        text += [";TYPE:Inner wall", ";WIDTH:0.50",
                 "G0 X%.3f Y%.3f" % (hole_x + radius, hole_y)]
        quarters = [(hole_x, hole_y + radius), (hole_x - radius, hole_y),
                    (hole_x, hole_y - radius), (hole_x + radius, hole_y)]
        here = (hole_x + radius, hole_y)
        for target in quarters:
            text.append("G3 X%.3f Y%.3f I%.3f J%.3f E0.50"
                        % (target[0], target[1],
                           hole_x - here[0], hole_y - here[1]))
            here = target
    else:
        text += loop(circle(hole_r + 0.25), kind="Inner wall")
    text += [";TYPE:Bridge", ";WIDTH:0.50"]
    y, flip = 100.9, False
    while y <= 119.3:
        low, high = (100.6, 119.4) if flip else (100.95, 119.05)
        if low <= y <= high:
            spans = [(109.6, 119.4 if flip else 119.05)]
            outer = hole_r + 0.5
            if abs(y - hole_y) < outer:
                dx = math.sqrt(max(0.0, outer ** 2 - (y - hole_y) ** 2))
                margin = 0.45 if flip else 0.1
                spans = [(109.6, hole_x - dx - margin),
                         (hole_x + dx + margin, 119.4 if flip else 119.05)]
            for a, b in spans:
                if b - a >= 0.6:
                    text.append(f"G0 X{a:.3f} Y{y:.3f}")
                    text.append(f"G1 X{b:.3f} Y{y:.3f} E{(b - a) * 0.033:.5f}")
        y += 0.5
        flip = not flip
    return "\n".join(text) + "\n", (hole_x, hole_y, hole_r)
