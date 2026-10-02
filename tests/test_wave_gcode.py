#!/usr/bin/env python3
"""Real-export regression for Wave's one-pass bridge replacement.

Run with numpy and shapely available (the same dependencies Orca installs):
  PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py
"""
import importlib.util
import math
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))

try:
    import numpy  # noqa: F401
    import shapely  # noqa: F401
except ImportError:
    print("SKIP -- test_wave_gcode needs numpy + shapely")
    raise SystemExit(0)

import fake_orca
import wave_cases
from shapely.geometry import LineString, MultiLineString, Point, Polygon
from shapely.ops import unary_union

orca = fake_orca.install()
path = ROOT / "plugins/wave-overhangs/wave_overhangs_orca.py"
spec = importlib.util.spec_from_file_location("wave_overhangs_orca", path)
wave = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = wave
spec.loader.exec_module(wave)

source = (ROOT / "tests/fixtures/Cube^2_3m53s.gcode").read_text(
    encoding="utf-8")
cfg = dict(wave._DEFAULTS)
cfg["_lh"] = 0.3
out, stats = wave._gcode_wave_rewrite(source, cfg)


def as_absolute_e(text):
    """Convert this captured relative-E fixture to an equivalent M82 file."""
    absolute = 0.0
    converted = []
    number = r"-?(?:\d+(?:\.\d*)?|\.\d+)"
    for line in text.splitlines(keepends=True):
        code = line.split(";", 1)[0]
        if re.match(r"^\s*M83\b", code):
            line = re.sub(r"^(\s*)M83\b", r"\1M82", line, count=1)
        elif re.match(r"^\s*G92\b", code):
            match = re.search(r"\bE(" + number + r")", code)
            if match:
                absolute = float(match.group(1))
        elif re.match(r"^\s*G[01]\b", code):
            match = re.search(r"\bE(" + number + r")", code)
            if match:
                absolute += float(match.group(1))
                line = line[:match.start(1)] + f"{absolute:.5f}" + line[match.end(1):]
        converted.append(line)
    return "".join(converted)


absolute_out, absolute_stats = wave._gcode_wave_rewrite(
    as_absolute_e(source), cfg)
assert absolute_stats["wave_layers"] == 3, absolute_stats
assert absolute_stats["removed_moves"] == stats["removed_moves"]
assert "\nM82\nG92 E" in absolute_out, (
    "absolute-E rewrite must restore both mode and command value")
assert absolute_out.count("G92 E") > as_absolute_e(source).count("G92 E")

# Geometry-level diffraction checks: growing a support footprint produces
# fronts through a concavity and separate fronts around an actual hole. The
# real Cube export does not contain either shape, so keep these as synthetic
# checks rather than pretending the fixture proves them.
geometry_cfg = wave.wc.WaveConfig(
    line_spacing=0.5, line_width=0.4, perimeter_overlap=0.1,
    max_iterations=100)
concave_support = Polygon([(0, 0), (8, 0), (8, 2), (3, 2), (3, 7), (0, 7)])
concave_target = Polygon([(0, 0), (12, 0), (12, 8), (0, 8)]).difference(
    concave_support.buffer(0.05))
concave_tracks = wave.wc.wave_tracks(
    concave_support, concave_target, geometry_cfg)
assert len(concave_tracks) >= 3
assert max(LineString(track.points).length for track in concave_tracks) > 5.0

hole_outer = Polygon([(0, 0), (12, 0), (12, 12), (0, 12)])
hole = Polygon([(4, 4), (8, 4), (8, 8), (4, 8)])
hole_support = hole_outer.difference(hole)
hole_target = Polygon([(0, 0), (14, 0), (14, 14), (0, 14)]).difference(
    hole_support.buffer(0.05))
hole_tracks = wave.wc.wave_tracks(hole_support, hole_target, geometry_cfg)
first_distance = min(track.distance for track in hole_tracks)
first_components = sum(
    abs(track.distance - first_distance) < 1e-9 for track in hole_tracks)
assert first_components >= 2, "a hole should produce separate front components"

# An internal hole is not part of the support boundary. It must still act as
# an obstacle after the wave reaches it, with fronts continuing on both sides.
internal_support = Polygon([(0, 0), (2, 0), (2, 10), (0, 10)])
internal_outer = Polygon([(0, 0), (12, 0), (12, 10), (0, 10)])
internal_hole = Point(7, 5).buffer(2.0, resolution=64)
internal_target = internal_outer.difference(internal_support).difference(
    internal_hole)
internal_tracks = wave.wc.wave_tracks(
    internal_support, internal_target, geometry_cfg)
assert any(max(x for x, y in track.points) < 6.0
           for track in internal_tracks), "no front reached the hole's left side"
assert any(min(x for x, y in track.points) > 8.0
           for track in internal_tracks), "no front diffracted to the right side"
for track in internal_tracks:
    front = LineString(track.points)
    assert not internal_hole.buffer(-0.01).intersects(
        front), "an internal-hole front crossed the void"
    for endpoint in (track.points[0], track.points[-1]):
        assert internal_target.boundary.distance(Point(endpoint)) < 2e-5, (
            "Wave front endpoints must terminate on the wall or hole boundary"
        )

# Endpoint cleanup must retain those valid boundary contacts while removing
# short endpoint stubs. Interior points are preserved so the boundary remains
# a curve instead of becoming a chord or a stair-step.
for track in internal_tracks:
    cleaned = wave._clean_wave_polyline(
        track.points, 0.4, tolerance=0.05, min_segment=0.30,
        allowed=internal_target)
    if len(cleaned) >= 2:
        assert internal_target.boundary.distance(Point(cleaned[0])) < 2e-5
        assert internal_target.boundary.distance(Point(cleaned[-1])) < 2e-5
        assert internal_target.buffer(0.02).covers(LineString(cleaned))

# A curved front must not be simplified into a chord through a circular void.
# This is the direct regression for the diagonal line visible in the user's
# second preview image.
curved_hole = Point(5, 5).buffer(2.0, resolution=64)
curved_allowed = Polygon([(0, 0), (10, 0), (10, 10), (0, 10)]).difference(
    curved_hole)
arc_points = [
    (5 + 2.01 * math.cos(math.pi - index * 0.1),
     5 + 2.01 * math.sin(math.pi - index * 0.1))
    for index in range(32)]
clean_arc = wave._clean_wave_polyline(
    arc_points, 0.4, tolerance=0.12, min_segment=0.30,
    allowed=curved_allowed)
assert curved_allowed.buffer(0.02).covers(LineString(clean_arc)), (
    "simplified Wave arc must not cross the hole")

assert stats["wave_layers"] == 3, stats
assert stats["replaced_sections"] == 3, stats
assert stats["removed_moves"] == 107, stats
assert stats["kept_fragments"] == 25, (
    "substantial uncovered bridge fragments must remain", stats)
assert stats["tiny_fragments_dropped"] == 122, (
    "sub-nozzle edge remnants should be cleaned up", stats)
assert stats["short_wave_paths_dropped"] == 11, (
    "isolated short wavefronts should be cleaned up", stats)
assert stats["wall_bounded_sections"] == 3, (
    "every exported bridge section should be squared up against the wall "
    "the layer actually printed", stats)
assert out.startswith("; wave-overhangs v"), "missing build stamp"
assert out.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert out.count("; ==== WAVE OVERHANG END ====") == 3
wave_blocks = re.findall(
    r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
    r"; ==== WAVE OVERHANG END ====", out, re.DOTALL)
wave_move_count = sum(block.count("\nG1 X") for block in wave_blocks)


def wave_polylines(blocks):
    """The Wave paths inside emitted Wave blocks, as point lists."""
    paths = []
    for block in blocks:
        current = None
        for line in block.splitlines():
            if line.startswith("G0"):
                words = wave._gwords(line)
                if "X" in words and "Y" in words:
                    if current and len(current) > 1:
                        paths.append(current)
                    current = [(words["X"], words["Y"])]
            elif line.startswith("G1") and " X" in line and " E" in line:
                words = wave._gwords(line)
                if current is not None and "X" in words and "Y" in words:
                    current.append((words["X"], words["Y"]))
        if current and len(current) > 1:
            paths.append(current)
    return paths


def wave_path_length(blocks):
    return sum(wave._polyline_length(path) for path in wave_polylines(blocks))
old_style_out, old_style_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, edge_taper_distance=0.0, edge_clearance=0.0,
                 edge_snap_distance=0.0))
old_style_blocks = re.findall(
    r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
    r"; ==== WAVE OVERHANG END ====", old_style_out, re.DOTALL)
old_style_move_count = sum(block.count("\nG1 X") for block in old_style_blocks)
clearance_only_out, clearance_only_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, edge_taper_distance=0.0, edge_clearance="auto"))
clearance_only_blocks = re.findall(
    r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
    r"; ==== WAVE OVERHANG END ====", clearance_only_out, re.DOTALL)
clearance_only_move_count = sum(
    block.count("\nG1 X") for block in clearance_only_blocks)
assert old_style_stats["removed_moves"] == stats["removed_moves"]
assert old_style_move_count == 508, (
    "old no-clearance/no-taper cleanup changed unexpectedly")
assert clearance_only_stats["removed_moves"] == stats["removed_moves"]
assert wave_path_length(clearance_only_blocks) < wave_path_length(
        old_style_blocks), (
    "edge clearance should trim emitted paths without changing bridge coverage")
assert wave_move_count == old_style_move_count, (
    "default endpoint taper must not add tiny grid-like endpoint moves")
subdivided_out, subdivided_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, edge_taper_segment=0.20))
subdivided_blocks = re.findall(
    r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
    r"; ==== WAVE OVERHANG END ====", subdivided_out, re.DOTALL)
subdivided_move_count = sum(
    block.count("\nG1 X") for block in subdivided_blocks)
assert subdivided_stats["removed_moves"] == stats["removed_moves"]
assert subdivided_move_count > wave_move_count, (
    "endpoint micro-segmentation should only happen when explicitly requested")
assert "; wave-overhangs edge clearance" not in out, (
    "default Wave output must not create endpoint gaps with edge clearance")
assert "; wave-overhangs edge clearance" in clearance_only_out, (
    "edge clearance should remain available as an explicit comparison option")
assert "; wave-overhangs edge taper" in out

def wave_e_per_mm_values(blocks):
    values = []
    for block in blocks:
        x = y = None
        for line in block.splitlines():
            if line.startswith("G0"):
                words = wave._gwords(line)
                x, y = words.get("X", x), words.get("Y", y)
            elif line.startswith("G1") and " X" in line and " E" in line:
                words = wave._gwords(line)
                nx, ny = words.get("X", x), words.get("Y", y)
                e = words.get("E")
                if None not in (x, y, nx, ny, e):
                    length = math.hypot(nx - x, ny - y)
                    if length > 1e-6:
                        values.append(e / length)
                x, y = nx, ny
    return values

default_ratios = wave_e_per_mm_values(wave_blocks)
old_style_ratios = wave_e_per_mm_values(old_style_blocks)
assert min(default_ratios) < min(old_style_ratios) * 0.75, (
    "tapered Wave endpoint flow should be visibly lower than normal flow")
assert max(default_ratios) <= max(old_style_ratios) * 1.01

synthetic_settings = wave._wave_taper_settings(
    cfg, internal_target, internal_support, geometry_cfg)
assert synthetic_settings is not None
assert not wave._endpoint_touches_detail(
    (2.0, 5.0), synthetic_settings["detail"], geometry_cfg.line_width), (
    "support-side anchor boundary should not be tapered")
assert wave._endpoint_touches_detail(
    (12.0, 5.0), synthetic_settings["detail"], geometry_cfg.line_width), (
    "outer overhang wall endpoint should taper")
assert wave._endpoint_touches_detail(
    (9.0, 5.0), synthetic_settings["detail"], geometry_cfg.line_width), (
    "hole wall endpoint should taper")

snap_cfg = dict(cfg, edge_snap_distance=0.30, edge_clearance=0.0,
                edge_taper_distance=0.0)
snapped_outer = wave._snap_wave_polylines(
    [[(2.05, 1.0), (11.82, 1.0)]], internal_target,
    internal_support, geometry_cfg, snap_cfg)
assert snapped_outer[0][-1][0] > 11.99, (
    "outer wall endpoint should snap to the perimeter instead of stopping short")
assert snapped_outer[0][0][0] == 2.05, (
    "support-side anchor boundary should not be snapped away from support")
snapped_hole = wave._snap_wave_polylines(
    [[(2.05, 5.0), (4.82, 5.0)]], internal_target,
    internal_support, geometry_cfg, snap_cfg)
assert abs(snapped_hole[0][-1][0] - 5.0) < 0.01, (
    "hole endpoint should snap to the hole perimeter")

angled = wave._snap_wave_polylines(
    [[(2.05, 1.0), (11.82, 2.0)]], internal_target,
    internal_support, geometry_cfg, snap_cfg)
ax, ay = angled[0][-2]
bx, by = angled[0][-1]
cross = abs((11.82 - 2.05) * (by - 2.0) - (2.0 - 1.0) * (bx - 11.82))
assert bx > 11.99 and cross < 0.01, (
    "endpoint snap should extend the existing line, not jump sideways to the nearest boundary")

clear_cfg = dict(cfg, edge_clearance=0.30, edge_taper_distance=0.0,
                 edge_snap_distance=0.0)
clear_domain, clear_distance = wave._clearance_domain(
    internal_target, internal_support, geometry_cfg, clear_cfg)
assert abs(clear_distance - 0.30) < 1e-9
clear_detail = wave._detail_boundary(internal_target, internal_support, geometry_cfg)
outer_trim = wave._inset_wave_polylines(
    [[(2.05, 1.0), (12.0, 1.0)]], internal_target,
    internal_support, geometry_cfg, clear_cfg)
assert outer_trim, "edge clearance removed the whole outer-wall test line"
outer_line = LineString(outer_trim[0])
assert min(x for x, _y in outer_trim[0]) <= 2.06, (
    "support-side anchor boundary should not be inset")
assert max(x for x, _y in outer_trim[0]) < 11.75, (
    "outer wall endpoint should stop before the perimeter")
assert outer_line.distance(clear_detail) >= 0.28, (
    "trimmed Wave line still bleeds into a detail boundary")
hole_trim = wave._inset_wave_polylines(
    [[(2.05, 5.0), (5.0, 5.0)]], internal_target,
    internal_support, geometry_cfg, clear_cfg)
assert hole_trim, "edge clearance removed the whole hole-wall test line"
assert max(x for x, _y in hole_trim[0]) < 4.75, (
    "hole endpoint should stop before the hole perimeter")
assert LineString(hole_trim[0]).distance(clear_detail) >= 0.28, (
    "trimmed hole Wave line still bleeds into the hole boundary")

assert "; wave-overhangs replaced covered bridge move" in out
lines = out.splitlines()


def _is_motion(line):
    """True for a line that actually moves the nozzle.

    A bare "G1 F1800" sets the modal feedrate and moves nothing, so it must
    not count as motion when checking that an extrusion is preceded by a
    travel.
    """
    return (line.startswith(("G0", "G1"))
            and (" X" in line or " Y" in line))


for marker in [i for i, line in enumerate(lines)
               if line.startswith("; wave-overhangs replaced covered bridge move")]:
    first_motion = next(
        (lines[j] for j in range(marker + 1, len(lines))
         if _is_motion(lines[j])), None)
    assert first_motion is not None and first_motion.startswith("G0"), (
        "every replaced bridge segment must return with non-extruding travel")

# A covered segment can leave the nozzle at the end of a generated Wave path.
# The next original retained bridge move must begin with an explicit G0, never
# a diagonal G1 extrusion from the Wave endpoint.
for end in [i for i, line in enumerate(lines)
            if line == "; ==== WAVE OVERHANG END ===="]:
    for i in range(end + 1, len(lines)):
        if lines[i].startswith(";LAYER_CHANGE"):
            break
        if lines[i].startswith("G1") and " E" in lines[i]:
            previous_motion = next(
                (lines[j] for j in range(i - 1, end, -1)
                 if _is_motion(lines[j])), None)
            assert previous_motion is not None and previous_motion.startswith("G0"), (
                "first retained extrusion after a Wave block needs a non-extruding travel")
            break

# Regression guard for the feedrate leak fixed in 0.0.28.
#
# A Wave block ends with the deliberately very slow Wave print speed in force
# (print_speed defaults to 2 mm/s, so F120). G-code feedrates are modal: the
# last F stays in force until something changes it. The moves the plugin
# writes after a Wave block used to carry no F at all, so they inherited
# 2 mm/s. On the owner's own export that stranded 373 moves covering 3.95 m
# which should have taken 30 seconds and instead took 32.9 minutes -- a third
# of the whole print, reported by the slicer as a nonsensical "Travel" figure.
#
# So: no move outside a Wave block may run on a feedrate that was set inside
# one.
modal_f = None
f_came_from_wave = False
inside_block = False
for line in lines:
    if line == "; ==== WAVE OVERHANG BEGIN ====":
        inside_block = True
        continue
    if line == "; ==== WAVE OVERHANG END ====":
        inside_block = False
        continue
    if not line.startswith(("G0", "G1")):
        continue
    for word in line.split()[1:]:
        if word.startswith("F"):
            modal_f = word
            f_came_from_wave = inside_block
    if _is_motion(line) and not inside_block:
        assert not f_came_from_wave, (
            "move after a Wave block inherited the Wave print speed "
            f"({modal_f}): {line!r}")

assert ";Z:5.4" in out and "Z5.650" in out, (
    "Wave must use the bridge move's real Z, including Orca's 0.25 mm Z offset")
assert "Z9.850" in out and "Z14.650" in out
assert "Z5.400" not in out.split("; ==== WAVE OVERHANG END ====", 1)[0], (
    "nominal layer Z leaked into the first Wave block")
assert ";TYPE:Bridge" in out, "section labels should remain inspectable"
assert "M106 S" in out, "wave blocks must restore the prior fan setting"

# The expanded pattern setting is real, not just documentation. Monotonic uses
# one endpoint direction for all fronts while retaining the same safe geometry
# bounds and transactional behavior.
monotonic, monotonic_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, pattern="monotonic", start_policy="consistent"))
assert monotonic_stats["wave_layers"] == 3, monotonic_stats
assert monotonic.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert monotonic != out, "pattern setting did not change the ordered toolpaths"

nearest, nearest_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, component_order="nearest"))
assert nearest_stats["removed_moves"] == stats["removed_moves"]
assert nearest.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert nearest != out, "nearest component ordering did not change toolpath order"

# ---------------------------------------------------------------------------
# Perimeter conformance (the 0.0.20 fix).
#
# Orca exports bridge infill as separate lines, so the area they cover has a
# castellated edge that stops short of the wall. Wave used to clip its fronts
# to that edge, which is what made the ends look frayed. The ends must now sit
# on one straight line along each wall, inside the wall bead.
# ---------------------------------------------------------------------------
legacy_out, legacy_stats = wave._gcode_wave_rewrite(source, dict(cfg, wall_snap=False))
legacy_blocks = re.findall(
    r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
    r"; ==== WAVE OVERHANG END ====", legacy_out, re.DOTALL)
assert legacy_stats["wall_bounded_sections"] == 0, legacy_stats
assert sum(block.count("\nG1 X") for block in legacy_blocks) == 739, (
    "wall_snap=False must still produce the 0.0.19 bridge-footprint edges")


def endpoints_along(paths, pick, keep):
    """Front endpoint coordinates that belong to one straight wall."""
    values = []
    for path in paths:
        for end in (path[0], path[-1]):
            if keep(end):
                values.append(pick(end))
    return values


# The real export's open-air overhang layer: the L-shaped arm at z 5.65.
overhang_paths = [path for path in wave_polylines(wave_blocks[:1])]
legacy_paths = [path for path in wave_polylines(legacy_blocks[:1])]
walls_5_4 = wave._wall_material(
    wave._parse_gcode_geometry(source.splitlines(keepends=True))[17])
edges = {
    "left": (lambda p: p[0], lambda p: p[0] < 103.6 and 103.4 < p[1] < 111.6),
    "bottom": (lambda p: p[1], lambda p: p[1] < 103.6 and 103.4 < p[0] < 111.6),
    "right": (lambda p: p[0], lambda p: p[0] > 111.4 and 103.4 < p[1] < 107.2),
    "top": (lambda p: p[1], lambda p: p[1] > 111.4 and 103.4 < p[0] < 107.2),
}
for name, (pick, keep) in edges.items():
    fixed = endpoints_along(overhang_paths, pick, keep)
    assert len(fixed) >= 5, (name, len(fixed))
    assert max(fixed) - min(fixed) <= 0.02, (
        f"Wave ends along the {name} wall must lie on one straight line",
        name, sorted(fixed))
# The same measurement on the old behaviour is visibly ragged: this is the bug
# being fixed, so the test must be able to see it.
ragged = endpoints_along(legacy_paths, *edges["top"])
assert max(ragged) - min(ragged) > 0.2, (
    "the 0.0.19 comparison run should still show the castellated edge",
    sorted(ragged))

# No end may stop in the "just short of the wall" band: an end either sits in
# the wall bead or is an interior/support-side anchor well away from a wall.
def ends_just_short(paths, bead, minimum=1.0):
    """Full-length rungs whose end stops in the 'nearly at the wall' band.

    Short anchored stubs are excluded: a 0.6 mm rung tucked into a corner is
    not a rung that failed to reach the wall, and it is the full-length ones
    that made the edge look castellated.
    """
    return [round(bead.distance(Point(end)), 3) for path in paths
            if wave._polyline_length(path) >= minimum
            for end in (path[0], path[-1])
            if 0.001 < bead.distance(Point(end)) < 0.30]


assert ends_just_short(overhang_paths, walls_5_4) == [], (
    "Wave ends must not stop just short of the wall",
    ends_just_short(overhang_paths, walls_5_4))
assert len(ends_just_short(legacy_paths, walls_5_4)) >= 5, (
    "the 0.0.19 comparison run should still stop short of the wall")
# A higher share of ends finish inside the wall bead, and none of them are
# left hovering just outside it.
def bead_share(paths):
    ends = [end for path in paths for end in (path[0], path[-1])]
    inside = sum(1 for end in ends if walls_5_4.covers(Point(end)))
    return inside, len(ends)


in_bead, total_ends = bead_share(overhang_paths)
legacy_in_bead, legacy_total = bead_share(legacy_paths)
assert in_bead / total_ends > legacy_in_bead / legacy_total, (
    "more of the Wave ends should finish in the wall bead than in 0.0.19",
    (in_bead, total_ends), (legacy_in_bead, legacy_total))
assert in_bead >= 30, in_bead


hole_source, (hx, hy, hr) = wave_cases.synthetic_overhang_with_hole()
hole_out, hole_stats = wave._gcode_wave_rewrite(hole_source, dict(cfg))
hole_legacy_out, _ = wave._gcode_wave_rewrite(
    hole_source, dict(cfg, wall_snap=False))
assert hole_stats["wall_bounded_sections"] == 1, hole_stats


def hole_paths(text):
    return wave_polylines(re.findall(
        r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
        r"; ==== WAVE OVERHANG END ====", text, re.DOTALL))


paths = hole_paths(hole_out)
legacy_hole_paths = hole_paths(hole_legacy_out)
assert paths and legacy_hole_paths

# 1. Nothing may be printed inside the opening.
for path in paths:
    for x, y in path:
        assert math.hypot(x - hx, y - hy) >= hr - 1e-6, (
            "a Wave path entered the hole", (x, y))

# 2. Ends that reach the hole must land on its perimeter, all at one radius.
hole_radii = [math.hypot(x - hx, y - hy) for path in paths
              for x, y in (path[0], path[-1])
              if math.hypot(x - hx, y - hy) < 4.3]
assert len(hole_radii) >= 20, len(hole_radii)
assert max(hole_radii) - min(hole_radii) <= 0.02, (
    "Wave ends around a hole must follow the hole, not a staircase",
    round(min(hole_radii), 3), round(max(hole_radii), 3))
assert hr < min(hole_radii) <= hr + 0.5, (
    "hole ends must sit in the hole's wall bead", min(hole_radii))
legacy_radii = [math.hypot(x - hx, y - hy) for path in legacy_hole_paths
                for x, y in (path[0], path[-1])
                if math.hypot(x - hx, y - hy) < 4.3]
assert max(legacy_radii) - min(legacy_radii) > 0.2, (
    "the comparison run should still show ragged hole ends")

# 3. Ends at the straight top and bottom walls must be on one line, and must
#    reach the wall instead of stopping a third of a millimetre short.
for pick, keep, label in (
        (lambda p: p[1], lambda p: p[1] > 118.6 and p[0] > 111.0, "top"),
        (lambda p: p[1], lambda p: p[1] < 101.4 and p[0] > 111.0, "bottom")):
    values = sorted(endpoints_along(paths, pick, keep))
    legacy_values = sorted(endpoints_along(legacy_hole_paths, pick, keep))
    assert len(values) >= 20, (label, len(values))
    trimmed = values[1:-1] if len(values) > 4 else values
    assert max(trimmed) - min(trimmed) <= 0.02, (label, trimmed[:5], trimmed[-5:])
    reach = (min(values) if label == "top" else -max(values))
    legacy_reach = (min(legacy_values) if label == "top" else -max(legacy_values))
    assert reach > legacy_reach + 0.2, (
        f"{label} ends must now reach the wall", reach, legacy_reach)

# 4. The field itself must march all the way to the far overhang perimeter.
far = max(x for path in paths for x, _y in path)
legacy_far = max(x for path in legacy_hole_paths for x, _y in path)
assert far >= 119.5, ("Wave must reach the far wall's inner edge", far)
assert far > legacy_far + 0.2, (far, legacy_far)

# 5. Nothing may be printed outside the part.
for path in paths:
    for x, y in path:
        assert 100.0 <= x <= 120.0 and 100.0 <= y <= 120.0, (x, y)

# ---------------------------------------------------------------------------
# Arc moves (the 0.0.21 feature).
#
# Wave runs after Orca has written the file, so Orca's own arc fitter never
# sees these moves. Wave emits its own G2/G3 -- but only when the export says
# the profile has arc fitting switched on, so a printer whose firmware cannot
# read arcs never receives any.
# ---------------------------------------------------------------------------
def arc_points(blocks, step=0.05):
    """Replay Wave blocks, expanding G2/G3 back into points."""
    paths, extrusion = [], 0.0
    for block in blocks:
        current, x, y = None, None, None
        for line in block.splitlines():
            words = wave._gwords(line)
            if line.startswith("G0") and "X" in words:
                if current and len(current) > 1:
                    paths.append(current)
                x, y = words["X"], words["Y"]
                current = [(x, y)]
            elif line.startswith("G1") and "X" in words:
                x, y = words["X"], words["Y"]
                extrusion += words.get("E", 0.0)
                if current is not None:
                    current.append((x, y))
            elif line.startswith(("G2 ", "G3 ")):
                target = (words["X"], words["Y"])
                arc = wave._arc_move_points(
                    (x, y), target, words, line.startswith("G2"), step=step)
                extrusion += words.get("E", 0.0)
                if current is not None:
                    current.extend(arc[1:])
                x, y = target
        if current and len(current) > 1:
            paths.append(current)
    return paths, extrusion


def wave_blocks_of(text):
    return re.findall(
        r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
        r"; ==== WAVE OVERHANG END ====", text, re.DOTALL)


# 1. Arcs are OFF by default (0.0.24). G2/G3 is the one genuinely new kind of
#    output Wave can produce, and Orca re-parses the finished file for its
#    preview, so the default must not change the command vocabulary at all.
assert wave._DEFAULTS["arc_fitting"] is False, (
    "arc_fitting must ship off until arcs are confirmed safe in real Orca")
assert "; enable_arc_fitting = 0" in source, "fixture precondition"
assert stats["arc_moves"] == 0, stats
assert not re.search(r"^G[23] ", out, re.MULTILINE), (
    "no arc may be emitted when the profile has arc fitting switched off")
forced_off, _ = wave._gcode_wave_rewrite(source, dict(cfg, arc_fitting=False))
assert forced_off == out, "arc_fitting=auto must match arc_fitting=false here"

# 2. Flip that one profile line and the same export gains arcs -- but only
#    once the user opts in with arc_fitting="auto".
arc_source = source.replace("; enable_arc_fitting = 0",
                            "; enable_arc_fitting = 1")
still_off, still_stats = wave._gcode_wave_rewrite(arc_source, dict(cfg))
assert still_stats["arc_moves"] == 0, (
    "the shipped default must stay arc-free even when the profile has arc "
    "fitting switched on")
assert not re.search(r"^G[23] ", still_off, re.MULTILINE), still_stats
arc_out, arc_stats = wave._gcode_wave_rewrite(
    arc_source, dict(cfg, arc_fitting="auto"))
arc_blocks = wave_blocks_of(arc_out)
emitted_arcs = sum(block.count("\nG2 ") + block.count("\nG3 ")
                   for block in arc_blocks)
assert arc_stats["arc_moves"] == emitted_arcs == 44, (arc_stats, emitted_arcs)
arc_move_count = sum(block.count("\nG1 X") for block in arc_blocks)
assert arc_move_count == 329, arc_move_count
assert arc_move_count + emitted_arcs < wave_move_count, (
    "arcs must reduce the number of commands, not add to them")
assert arc_stats["removed_moves"] == stats["removed_moves"], (
    "arc fitting must not change which bridge extrusion is replaced")

# 3. Same shape, same plastic. The arcs are checked by expanding them back
#    into points and comparing with the straight-move version.
straight_paths, straight_e = arc_points(wave_blocks)
curved_paths, curved_e = arc_points(arc_blocks)
assert abs(curved_e - straight_e) <= straight_e * 0.005, (straight_e, curved_e)
straight_shape = MultiLineString([p for p in straight_paths if len(p) > 1])
worst = max(straight_shape.distance(Point(q))
            for path in curved_paths for q in path)
assert worst <= 0.12, ("an arc strayed too far from the path it replaced", worst)

# 4. Arcs must respect the same perimeters the straight moves do.
arc_hole_source, (ax, ay, ar) = wave_cases.synthetic_overhang_with_hole()
arc_hole_out, _ = wave._gcode_wave_rewrite(
    arc_hole_source, dict(cfg, arc_fitting=True))
for path in arc_points(wave_blocks_of(arc_hole_out))[0]:
    for x, y in path:
        assert math.hypot(x - ax, y - ay) >= ar - 1e-6, (
            "an arc entered the hole", (x, y))
        assert 100.0 <= x <= 120.0 and 100.0 <= y <= 120.0, (x, y)

# 5. Reading arcs back. With arc fitting on, Orca exports a round hole's wall
#    as G2/G3, and Wave has to see the same circle it sees from straight
#    moves -- otherwise the wall-conformance fix above goes blind.
straight_walls = wave._wall_material(wave._parse_gcode_geometry(
    wave_cases.synthetic_overhang_with_hole()[0].splitlines(keepends=True))[1])
arced_walls = wave._wall_material(wave._parse_gcode_geometry(
    wave_cases.synthetic_overhang_with_hole(arc_walls=True)[0]
    .splitlines(keepends=True))[1])
straight_ring = [p for p in wave._polygon_parts(straight_walls)
                 if p.bounds[0] > 110.0][0]
arced_ring = [p for p in wave._polygon_parts(arced_walls)
              if p.bounds[0] > 110.0][0]
assert abs(arced_ring.area - straight_ring.area) < 0.01, (
    "a hole wall written as arcs must give the same wall material",
    straight_ring.area, arced_ring.area)
assert arced_ring.symmetric_difference(straight_ring).area < 0.05
quarter = wave._arc_move_points((15.0, 10.0), (10.0, 15.0),
                                {"I": -5.0, "J": 0.0}, False)
assert max(abs(math.hypot(x - 10.0, y - 10.0) - 5.0)
           for x, y in quarter) < 1e-9, "I/J arcs must be read exactly"
assert wave._arc_move_points((0.0, 0.0), (10.0, 0.0), {"R": 1.0}, False) == [
    (0.0, 0.0), (10.0, 0.0)], "an impossible radius falls back to the chord"
assert wave._MOVE_CODE.match("G28 X0 Y0") is None, "G28 is not a move"

# 6. Arcs stay idempotent and fail closed like everything else.
arc_again, arc_second = wave._gcode_wave_rewrite(arc_out, dict(cfg))
assert arc_second.get("already_processed") is True, arc_second
assert arc_again == arc_out, "a second pass over arc output must change nothing"

# ---------------------------------------------------------------------------
# Corner slivers (the 0.0.22 fix).
#
# A wavefront is a contour of equal distance from the supported edge, and the
# contours step outward one spacing at a time. Where the far boundary runs at
# an angle to that march -- the tip of a corner -- the last contour stops
# short and leaves a sliver with nothing in it. The owner photographed one.
# ---------------------------------------------------------------------------
wedge_source, wedge_info = wave_cases.synthetic_wedge_corner()


def unfilled_slivers(text, source_text, minimum=0.02):
    """Area inside the walls that no extrusion covers, by piece."""
    layers = wave._parse_gcode_geometry(source_text.splitlines(keepends=True))
    silhouette = wave._layer_outline(layers[1])
    support = wave._footprint(layers[0]["all"])
    beads = []
    for path in wave_polylines(wave_blocks_of(text)):
        if len(path) > 1:
            beads.append(LineString(path).buffer(0.25, cap_style=2))
    covered = unary_union(beads) if beads else Polygon()
    gap = silhouette.buffer(-0.375).difference(covered).difference(
        support.buffer(0.05))
    return sorted((p for p in wave._polygon_parts(gap) if p.area > minimum),
                  key=lambda p: -p.area)


wedge_plain, wedge_plain_stats = wave._gcode_wave_rewrite(
    wedge_source, dict(cfg, gap_fill=False))
wedge_filled, wedge_stats = wave._gcode_wave_rewrite(wedge_source, dict(cfg))
tip_x = wedge_info["tip"][0]


def near_tip(text):
    return sum(p.area for p in unfilled_slivers(text, wedge_source)
               if p.centroid.x > tip_x - 10.0)


assert wedge_plain_stats["gap_fills"] == 0, wedge_plain_stats
assert wedge_stats["gap_fills"] >= 1, wedge_stats
assert near_tip(wedge_plain) > 0.1, (
    "the wedge fixture must still show the unfilled corner being fixed",
    near_tip(wedge_plain))
assert near_tip(wedge_filled) < 0.05, (
    "the corner sliver must be filled", near_tip(wedge_filled))

# Gap fill only ever adds material where there is none: a part with no sliver
# must come out byte for byte identical.
hole_no_fill, _ = wave._gcode_wave_rewrite(hole_source, dict(cfg, gap_fill=False))
assert hole_no_fill == hole_out, (
    "gap fill must not change a part that has no slivers")

# A short front that touches a full-length rung is anchored, so it is kept;
# an isolated speck is still dropped. Which ones survive must not depend on
# the print order, or two orderings would cover different bridge area.
for ordering in ({"component_order": "nearest"}, {"pattern": "monotonic"},
                 {"pattern": "zigzag"}):
    _text, ordered_stats = wave._gcode_wave_rewrite(source, dict(cfg, **ordering))
    assert ordered_stats["removed_moves"] == stats["removed_moves"], (
        "print order must not change which bridge extrusion is covered",
        ordering, ordered_stats["removed_moves"], stats["removed_moves"])
    assert ordered_stats["kept_fragments"] == stats["kept_fragments"], ordering

# ---------------------------------------------------------------------------
# Processing cost (the 0.0.23 work).
#
# Geometry is only built for layers that have a Bridge section and the layer
# that holds each one up. On a tall export that is a handful of layers out of
# hundreds, and building the rest was pure waste.
# ---------------------------------------------------------------------------
interesting = wave._bridge_layer_indices(source.splitlines(keepends=True))
all_layers = wave._parse_gcode_geometry(source.splitlines(keepends=True))
assert interesting == {16, 17, 18, 30, 31, 46, 47}, sorted(interesting)
selective = wave._parse_gcode_geometry(
    source.splitlines(keepends=True), interesting)
built = sum(len(layer["all"]) for layer in selective)
everything = sum(len(layer["all"]) for layer in all_layers)
assert built < everything * 0.5, (built, everything)
# The layers that are built must be byte-identical to building all of them.
for index in sorted(interesting):
    assert len(selective[index]["all"]) == len(all_layers[index]["all"]), index
    assert len(selective[index]["walls"]) == len(all_layers[index]["walls"]), index
assert selective[17]["z"] == all_layers[17]["z"]
# ... and the result is the same either way.
full_out, full_stats = wave._gcode_wave_rewrite(source, dict(cfg))
assert full_out == out and full_stats["removed_moves"] == stats["removed_moves"]

# An export with no Bridge section at all must cost almost nothing: no
# geometry is built for it.
plain = source.replace(";TYPE:Bridge", ";TYPE:Internal solid infill").replace(
    ";TYPE:Internal Bridge", ";TYPE:Internal solid infill")
plain_out, plain_stats = wave._gcode_wave_rewrite(plain, dict(cfg))
assert plain_out == plain, "a file with no bridge must come back untouched"
assert plain_stats["wave_layers"] == 0, plain_stats
assert wave._bridge_layer_indices(plain.splitlines(keepends=True)) == set()

# The stats carry timings so a slow export can be diagnosed from the log.
for key in ("seconds", "parse_seconds", "plan_seconds", "geometry_layers",
            "layers_scanned"):
    assert key in stats, (key, sorted(stats))
assert stats["layers_scanned"] == len(all_layers)
assert stats["geometry_layers"] == len(interesting)

again, second = wave._gcode_wave_rewrite(out, cfg)
assert again == out and second["already_processed"], "second pass must be a no-op"

# --- wake_blend: rounding the crease behind a hole (0.0.26, experimental) ---
# Off by default, and off must mean *exactly* the old behaviour: this knob
# reaches into the propagation loop, so the no-op case has to be provably
# free. It is off because on real geometry it still loses coverage.
assert wave._DEFAULTS["wake_blend"] == 0.0, wave._DEFAULTS["wake_blend"]
zero_out, zero_stats = wave._gcode_wave_rewrite(source, dict(cfg, wake_blend=0.0))
assert zero_out == out, "wake_blend=0 must reproduce the default output exactly"

# Switched on it must still run clean on every shape we have -- the earlier
# attempt at this crashed GEOS on degenerate rings and silently produced no
# waves at all, which is the failure mode that matters most here.
for label, case_source in (("cube", source),
                           ("hole", hole_source),
                           ("wedge", wedge_source)):
    blended, blend_stats = wave._gcode_wave_rewrite(
        case_source, dict(cfg, wake_blend=1.0))
    assert "error" not in blend_stats, (label, blend_stats)
    assert blend_stats["wave_layers"] >= 1, (label, blend_stats)

# Out-of-range values are clamped rather than rejected, and junk falls back
# to the default instead of throwing.
for value in (99.0, -5.0, "nonsense", None):
    guarded, guarded_stats = wave._gcode_wave_rewrite(
        source, dict(cfg, wake_blend=value))
    assert "error" not in guarded_stats, (value, guarded_stats)
    assert guarded_stats["wave_layers"] >= 1, (value, guarded_stats)

# The underlying robustness fix: linemerge raises GEOSException (which is
# NOT a ValueError, so the old handler could not catch it) when a clipped
# boundary leaves a single-point crumb. wave_tracks must survive that.
import shapely.errors
assert not issubclass(shapely.errors.GEOSException, (TypeError, ValueError)), (
    "if this ever becomes a ValueError the narrow handler would be enough")
real_linemerge = wave.wc.linemerge
try:
    calls = {"n": 0}

    def exploding(arg):
        calls["n"] += 1
        if calls["n"] == 2:        # fail once, mid-propagation
            raise shapely.errors.GEOSException(
                "IllegalArgumentException: point array must contain 0 or >1 elements")
        return real_linemerge(arg)

    wave.wc.linemerge = exploding
    survived, survived_stats = wave._gcode_wave_rewrite(
        hole_source, dict(cfg, wake_blend=1.0))
finally:
    wave.wc.linemerge = real_linemerge
assert calls["n"] > 1, calls
assert "error" not in survived_stats, survived_stats
assert survived_stats["wave_layers"] >= 1, (
    "a GEOSException from linemerge must not lose the whole layer")

# --- no move may be written that does nothing (0.0.25) ---
# Coordinates go out with three decimals. Guarding on the unrounded step
# length meant sub-micron samples were written as moves whose X/Y rounded to
# the previous line's, producing literal no-ops -- 542 of them in the owner's
# real export. Every emitted G1 must change the written position, and the
# extrusion of any skipped step must survive in the next real move.
def noop_moves_in(text):
    noops = 0
    for block in wave_blocks_of(text):
        px = py = None
        for line in block.splitlines():
            words = wave._gwords(line)
            if line.startswith("G0") and "X" in words:
                px, py = round(words["X"], 3), round(words["Y"], 3)
            elif line.startswith("G1") and "X" in words:
                here = (round(words["X"], 3), round(words["Y"], 3))
                if (px, py) == here:
                    noops += 1
                px, py = here
            elif line.startswith(("G2 ", "G3 ")):
                px, py = round(words["X"], 3), round(words["Y"], 3)
    return noops


def wave_extrusion_in(text):
    total = 0.0
    for block in wave_blocks_of(text):
        for line in block.splitlines():
            if line.startswith(("G1 X", "G2 ", "G3 ")):
                total += wave._gwords(line).get("E", 0.0)
    return total


for label, candidate in (("default", out),
                         ("arcs", arc_out),
                         ("hole", hole_out),
                         ("wedge", wedge_filled)):
    assert noop_moves_in(candidate) == 0, (
        label, noop_moves_in(candidate), "wrote a move that goes nowhere")

# Dropping those steps must not quietly drop their material either. A
# taper-segmented export is the case that produced them, so check the total
# extrusion is still what the geometry asks for.
# Drive the emitter directly with a front that contains sub-micron steps --
# the shape that produced 542 dead lines in the owner's real export. The
# whole-file fixtures happen not to contain any, so without this the check
# above would pass even with the fix removed.
hairline = [(100.0, 100.0)]
for step in (0.0002, 0.0003, 0.0001, 0.0004):      # all round to X100.000
    hairline.append((hairline[-1][0] + step, 100.0))
for step in (0.4, 0.4, 0.4):                        # then real movement
    hairline.append((hairline[-1][0] + step, 100.0))
for step in (0.0002, 0.0002):                       # and a dead tail
    hairline.append((hairline[-1][0] + step, 100.0))

swcfg = wave._wave_config(dict(wave._DEFAULTS), 0.3)
emitted = wave._emit_wave_gcode([hairline], 1.5, swcfg, dict(wave._DEFAULTS))
body = [l for l in emitted if l.startswith("G1 X")]
assert body, emitted
seen = (100.0, 100.0)
for line in body:
    words = wave._gwords(line)
    here = (round(words["X"], 3), round(words["Y"], 3))
    assert here != seen, f"emitter wrote a move that goes nowhere: {line}"
    seen = here
# Nine samples collapse to four written moves: the three real 0.4 mm steps,
# plus one for the four hairline steps, which together come to exactly
# 0.001 mm and so do legitimately cross the three-decimal threshold. The
# 0.0004 mm tail never does, and is correctly never written.
assert len(body) == 4, (len(body), body)
assert len(hairline) == 10, len(hairline)

# The material from the skipped hairline steps has to survive, rolled into
# the next real move rather than silently dropped.
span = hairline[-1][0] - hairline[0][0]
written = sum(wave._gwords(l).get("E", 0.0) for l in body)
# the dead 0.0004 mm tail legitimately extrudes nothing, so allow for it
expected = (span - 0.0004) * swcfg.e_per_mm()
assert abs(written - expected) < expected * 0.01, (written, expected)

# --- the time budget: Wave must never be able to hang an export (0.0.24) ---
# A wall-clock ceiling is the backstop for every slow path we have not
# measured, including any we introduce later. When it fires the file must
# come back exactly as Orca wrote it: unchanged, unstamped, and flagged.
assert wave._DEFAULTS["time_budget"] == 30.0, wave._DEFAULTS["time_budget"]
timed, timed_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, time_budget=0.0001))
assert timed == source, "a timed-out pass must not alter a single byte"
assert wave.WAVE_STAMP_PREFIX not in timed, "it must not stamp what it skipped"
assert timed_stats["timed_out"] is True, timed_stats
assert timed_stats["wave_layers"] == 0, timed_stats
assert "time budget exceeded" in timed_stats["error"], timed_stats
# Nothing else may be flagged as a timeout...
assert not stats.get("timed_out"), stats
# ...0 switches the ceiling off entirely, and junk falls back to the default
# rather than throwing or disabling the plugin.
for budget in (0, "nonsense", None):
    ok_out, ok_stats = wave._gcode_wave_rewrite(
        source, dict(cfg, time_budget=budget))
    assert ok_out == out, budget
    assert not ok_stats.get("timed_out"), (budget, ok_stats)

old = wave.wc.wave_tracks
try:
    def broken(*_args, **_kwargs):
        raise RuntimeError("deliberate generation failure")
    wave.wc.wave_tracks = broken
    failed, failure_stats = wave._gcode_wave_rewrite(source, cfg)
finally:
    wave.wc.wave_tracks = old
assert failed == source, "generation failure must retain the original G-code byte-for-byte"
assert "error" in failure_stats

print("ok -- real Cube^2 export: 3 cleaned wave layers use actual offset Z, "
      "replace covered moves, retain substantial fragments, snap/taper edge "
      "endpoints, restore fan state, fail closed, and are idempotent; Wave "
      "ends now sit on one straight line in the wall bead (and on a hole's "
      "perimeter in the synthetic overhang-with-hole export) instead of the "
      "0.0.19 castellated edge, and G2/G3 arcs are both read from the export "
      "and emitted when the profile asks for them")

# ---------------------------------------------------------------------------
# print_speed = "orca": follow the bridge speed already in the user's profile
# ---------------------------------------------------------------------------
# The owner asked for this directly: 2 mm/s is the single biggest cost in a
# Wave print, and their Orca profile already states a bridge speed. Rather
# than make them copy a number across, "orca" reads the feedrate off the very
# bridge moves the plugin is replacing.

def _bridge_feedrates(text):
    """Every feedrate Orca used on an extruding move in a bridge section."""
    found = []
    feed = None
    section = None
    for raw in text.splitlines():
        s = raw.strip()
        if s.startswith(";TYPE:"):
            section = s[6:].strip().lower()
            continue
        if not s.startswith(("G0", "G1")):
            continue
        words = {t[0]: t[1:] for t in s.split()[1:] if t[:1] in "XYEF"}
        if "F" in words:
            try:
                feed = float(words["F"])
            except ValueError:
                pass
        if section in ("bridge", "internal bridge") and "E" in words and feed:
            try:
                if float(words["E"]) > 0:
                    found.append(feed)
            except ValueError:
                pass
    return found


source_bridge_feeds = _bridge_feedrates(source)
assert source_bridge_feeds, "fixture has no bridge extrusions to read a speed from"
# Resolution is per bridge section, not one value for the whole file: this
# fixture genuinely contains sections at different speeds, and each wave block
# must follow the section it replaces.
source_feed_set = set(source_bridge_feeds)
expected_f = max(source_feed_set, key=source_bridge_feeds.count)

orca_out, orca_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, print_speed="orca"))
assert orca_stats.get("wave_layers"), "print_speed='orca' produced no wave layers"

orca_block_feeds = set()
inside = False
for raw in orca_out.splitlines():
    s = raw.strip()
    if s == "; ==== WAVE OVERHANG BEGIN ====":
        inside = True
        continue
    if s == "; ==== WAVE OVERHANG END ====":
        inside = False
        continue
    if inside and s.startswith("G1 F"):
        orca_block_feeds.add(float(s.split()[1][1:]))

assert orca_block_feeds, "print_speed='orca' emitted no wave feedrates at all"
assert orca_block_feeds <= source_feed_set, (
    f"print_speed='orca' invented a feedrate the export never used: "
    f"{sorted(orca_block_feeds - source_feed_set)} not in {sorted(source_feed_set)}")
assert 120.0 not in orca_block_feeds, (
    "print_speed='orca' still fell back to the 2 mm/s default somewhere")
assert expected_f in orca_block_feeds, (
    f"the fixture's dominant bridge feedrate F{expected_f:.0f} was not used "
    f"by any wave block; got {sorted(orca_block_feeds)}")

# ...and the default must be untouched by the new option.
default_block_feeds = set()
inside = False
for raw in out.splitlines():
    s = raw.strip()
    if s == "; ==== WAVE OVERHANG BEGIN ====":
        inside = True
        continue
    if s == "; ==== WAVE OVERHANG END ====":
        inside = False
        continue
    if inside and s.startswith("G1 F"):
        default_block_feeds.add(float(s.split()[1][1:]))
assert default_block_feeds == {120.0}, (
    f"default print_speed must stay 2 mm/s (F120), got {sorted(default_block_feeds)}")

# A junk value must fall back to the safe default rather than crash or run fast.
junk_out, _junk_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, print_speed="definitely not a number"))
junk_feeds = set()
inside = False
for raw in junk_out.splitlines():
    s = raw.strip()
    if s == "; ==== WAVE OVERHANG BEGIN ====":
        inside = True
        continue
    if s == "; ==== WAVE OVERHANG END ====":
        inside = False
        continue
    if inside and s.startswith("G1 F"):
        junk_feeds.add(float(s.split()[1][1:]))
assert junk_feeds == {120.0}, (
    f"an unparseable print_speed must fall back to the 2 mm/s default, got {sorted(junk_feeds)}")

print(f"ok -- print_speed='orca' follows the export's own bridge feedrate "
      f"(F{expected_f:.0f}), the default stays F120, and junk falls back to F120")
