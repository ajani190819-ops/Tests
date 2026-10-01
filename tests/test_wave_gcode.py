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
from shapely.geometry import LineString, Point, Polygon

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
    assert not internal_hole.buffer(-0.01).intersects(
        LineString(track.points)), "an internal-hole front crossed the void"

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
assert stats["removed_moves"] == 103, stats
assert stats["kept_fragments"] == 31, (
    "substantial uncovered bridge fragments must remain", stats)
assert stats["tiny_fragments_dropped"] == 111, (
    "sub-nozzle edge remnants should be cleaned up", stats)
assert stats["short_wave_paths_dropped"] == 30, (
    "isolated short wavefronts should be cleaned up", stats)
assert out.startswith("; wave-overhangs v"), "missing build stamp"
assert out.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert out.count("; ==== WAVE OVERHANG END ====") == 3
wave_move_count = sum(
    block.count("\nG1 X") for block in re.findall(
        r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
        r"; ==== WAVE OVERHANG END ====", out, re.DOTALL))
assert wave_move_count == 386, "wavefront cleanup changed unexpectedly"
assert "; wave-overhangs replaced covered bridge move" in out
lines = out.splitlines()
for marker in [i for i, line in enumerate(lines)
               if line.startswith("; wave-overhangs replaced covered bridge move")]:
    first_motion = next(
        (lines[j] for j in range(marker + 1, len(lines))
         if lines[j].startswith(("G0", "G1"))), None)
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
                 if lines[j].startswith(("G0", "G1"))), None)
            assert previous_motion is not None and previous_motion.startswith("G0"), (
                "first retained extrusion after a Wave block needs a non-extruding travel")
            break

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

again, second = wave._gcode_wave_rewrite(out, cfg)
assert again == out and second["already_processed"], "second pass must be a no-op"

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
      "replace covered moves, retain substantial fragments, restore fan state, "
      "fail closed, and are idempotent")
