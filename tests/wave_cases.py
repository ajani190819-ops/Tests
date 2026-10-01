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


def synthetic_rounded_corner():
    """A twin of the owner's part: an L-shaped overhang around a solid body,
    with one sharp corner, one large-radius rounded corner and a round hole.

    Dimensions are taken from their own export (bridge layer at z 13.8): the
    plate spans x 106..145.5, y 102..141.5, the body below occupies the
    x 119..145.5, y 115.3..141.5 corner, the bottom-right corner is rounded
    with a 13 mm radius and there is a 5 mm hole at (113.3, 110).

    This reproduces both defects the owner circled: a void in the sharp
    corner, and a staircase along the rounded wall.
    """
    x0, y0, x1, y1 = 106.0, 102.0, 145.5, 141.5
    bx, by = 119.0, 115.3                      # inner corner of the body above
    radius = 13.0
    hole_x, hole_y, hole_r = 113.3, 110.0, 2.5
    width, half = 0.60, 0.30

    def outline(inset):
        """The plate's outline, inset by `inset` mm (0 = the silhouette)."""
        pts = [(x0 + inset, y1 - inset), (x0 + inset, y0 + inset)]
        cx, cy = x1 - radius, y0 + radius
        start = (cx, y0 + inset)
        pts.append(start)
        steps = 48
        for i in range(steps + 1):
            a = math.pi * 1.5 + (math.pi * 0.5) * i / steps
            pts.append((cx + (radius - inset) * math.cos(a),
                        cy + (radius - inset) * math.sin(a)))
        pts.append((x1 - inset, y1 - inset))
        pts.append((x0 + inset, y1 - inset))
        return pts

    def circle(cx, cy, r, n=64):
        return [(cx + r * math.cos(2 * math.pi * i / n),
                 cy + r * math.sin(2 * math.pi * i / n)) for i in range(n + 1)]

    def loop(points, kind="Outer wall"):
        block = [f";TYPE:{kind}", f";WIDTH:{width:.2f}",
                 "G0 X%.3f Y%.3f" % points[0]]
        for x, y in points[1:]:
            block.append(f"G1 X{x:.3f} Y{y:.3f} E0.40")
        return block

    text = ["M83", ";Z:13.5"]
    # The body below: its own wall loop plus solid infill.
    text += loop([(bx + half, by + half), (x1 - half, by + half),
                  (x1 - half, y1 - half), (bx + half, y1 - half),
                  (bx + half, by + half)])
    text += [";TYPE:Internal solid infill", ";WIDTH:0.60"]
    y = by + 0.9
    while y <= y1 - 0.9:
        text.append("G0 X%.3f Y%.3f" % (bx + 0.9, y))
        text.append("G1 X%.3f Y%.3f E0.40" % (x1 - 0.9, y))
        y += 0.45

    # The overhanging plate.
    text += [";Z:13.8"]
    text += loop(outline(half))
    text += loop(circle(hole_x, hole_y, hole_r + half), kind="Inner wall")
    text += [";TYPE:Bridge", ";WIDTH:0.60"]
    # Horizontal bridge lines, ends short of the wall by a varying amount,
    # exactly as an exported bridge infill is.
    y = y0 + 0.8
    flip = False
    while y <= y1 - 0.8:
        inset = 0.75 if flip else 0.45
        left = x0 + inset
        if y >= by:
            right = bx - inset               # beside the body
        else:
            cx, cy = x1 - radius, y0 + radius
            if y < cy:
                dy = cy - y
                right = cx + math.sqrt(max(0.0, radius ** 2 - dy ** 2)) - inset
            else:
                right = x1 - inset
        spans = [(left, right)]
        if abs(y - hole_y) < hole_r + 0.6:
            dx = math.sqrt(max(0.0, (hole_r + 0.6) ** 2 - (y - hole_y) ** 2))
            spans = [(left, hole_x - dx - 0.2), (hole_x + dx + 0.2, right)]
        for a, b in spans:
            if b - a >= 0.8:
                text.append(f"G0 X{a:.3f} Y{y:.3f}")
                text.append(f"G1 X{b:.3f} Y{y:.3f} E{(b - a) * 0.04:.5f}")
        y += 0.45
        flip = not flip
    return "\n".join(text) + "\n", dict(
        plate=(x0, y0, x1, y1), body=(bx, by), radius=radius,
        hole=(hole_x, hole_y, hole_r))


def synthetic_wedge_corner():
    """An overhang that narrows to an acute tip far from its support.

    The wavefronts march out from the supported edge in fixed steps, so the
    tip of the wedge is exactly the place a front cannot reach: it is the
    shape that leaves the unfilled corner sliver the owner photographed.
    """
    width = 0.50
    half = width * 0.5
    tip = (130.0, 110.0)
    top = (100.0, 118.0)
    bottom = (100.0, 102.0)

    def loop(points, kind="Outer wall"):
        block = [f";TYPE:{kind}", f";WIDTH:{width:.2f}",
                 "G0 X%.3f Y%.3f" % points[0]]
        for x, y in points[1:]:
            block.append(f"G1 X{x:.3f} Y{y:.3f} E0.40")
        return block

    # Wall centreline: the wedge, inset by half a wall width.
    wall = [(top[0] + half, top[1] - half), (tip[0] - 1.2, tip[1]),
            (bottom[0] + half, bottom[1] + half),
            (top[0] + half, top[1] - half)]
    text = ["M83", ";Z:0.3"]
    # The support: a block under the wide end only.
    text += loop([(100.25, 102.25), (104.0, 102.25), (104.0, 117.75),
                  (100.25, 117.75), (100.25, 102.25)])
    text += [";TYPE:Internal solid infill", ";WIDTH:0.45", "G0 X100.7 Y102.7"]
    y = 102.7
    while y <= 117.3:
        text.append("G1 X103.6 Y%.3f E0.30" % y)
        text.append("G0 X100.7 Y%.3f" % (y + 0.45))
        y += 0.45
    text += [";Z:0.6"]
    text += loop(wall)
    text += [";TYPE:Bridge", ";WIDTH:0.50"]
    y = 102.6
    flip = False
    while y <= 117.4:
        # the wedge's upper and lower edges, as x limits for this scanline
        if y >= 110.0:
            frac = (118.0 - y) / 8.0
        else:
            frac = (y - 102.0) / 8.0
        right = 100.0 + frac * 30.0 - (0.75 if flip else 0.45)
        left = 100.6
        if right - left >= 0.8:
            text.append("G0 X%.3f Y%.3f" % (left, y))
            text.append("G1 X%.3f Y%.3f E%.5f" % (right, y, (right - left) * 0.033))
        y += 0.45
        flip = not flip
    return "\n".join(text) + "\n", dict(tip=tip, top=top, bottom=bottom)
