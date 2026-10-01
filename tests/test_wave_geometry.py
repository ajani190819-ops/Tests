#!/usr/bin/env python3
"""Geometry-stage Wave prototype regression.

Run with the plugin dependencies available:
  PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_geometry.py

This is deliberately a host-independent geometry test. It proves the new
plugin's Wave fronts and preview ribbons are safe around a hole; only a real
OrcaSlicer run can prove that its host bindings mutate and re-slice the live
preview graph on the installed build.
"""
import importlib.util
import os
import sys
import tempfile
from pathlib import Path

try:
    from shapely.geometry import LineString, Point, Polygon
except ImportError:
    print("SKIP -- test_wave_geometry needs shapely")
    raise SystemExit(0)

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import fake_orca  # noqa: E402

fake_orca.install()
path = ROOT / "plugins/wave-overhangs-geometry/wave_overhangs_geometry_orca.py"
spec = importlib.util.spec_from_file_location("wave_overhangs_geometry", path)
wave = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = wave
spec.loader.exec_module(wave)
assert wave._bridge_surface_type().name == "stBottomBridge", (
    "Wave preview geometry must be classified as bridge fill surfaces")

cfg = dict(wave._DEFAULTS)
cfg.update({"line_spacing": 0.5, "line_width": 0.4,
            "perimeter_overlap": 0.1, "max_iterations": 100})

support = Polygon([(0, 0), (2, 0), (2, 10), (0, 10)])
hole = Point(7, 5).buffer(2.0, resolution=64)
current = Polygon([(0, 0), (12, 0), (12, 10), (0, 10)]).difference(hole)
overhang = current.difference(support.buffer(cfg["overhang_tol"]))
fronts = wave._fronts(support, overhang, cfg)
assert fronts, "the geometry prototype did not generate Wave fronts"
for front in fronts:
    assert not hole.buffer(-0.01).intersects(front), (
        "a preview Wave front crossed the circular hole")
    for endpoint in (front.coords[0], front.coords[-1]):
        assert overhang.buffer(0.001).boundary.distance(Point(endpoint)) < 0.002, (
            "a Wave front endpoint escaped the overhang wall or hole boundary")

ribbons = wave._ribbons(fronts, current, support, cfg)
assert ribbons, "the geometry prototype did not create preview ribbons"
last_distance = -1.0
for ribbon in ribbons:
    assert current.buffer(0.002).covers(ribbon), (
        "a preview ribbon expanded outside the current slice")
    assert not hole.buffer(-0.01).intersects(ribbon), (
        "a preview ribbon crossed the circular hole")
    distance = support.distance(ribbon)
    assert distance + 1e-9 >= last_distance, (
        "preview ribbons were not handed to Orca from support outward")
    last_distance = distance

# Tiny clipped fragments should not become visible dot islands.
tiny = wave._ribbons([LineString([(2.10, 1.0), (2.20, 1.0)])],
                     current, support, cfg)
assert not tiny, "a tiny clipped front survived as a preview dot"

original, edited, bridge_parts, wall_parts, count = wave.plan_layer_geometry(
    [(None, [], [current])], support, cfg)
assert not original.is_empty
assert not edited.is_empty
assert bridge_parts
assert not wall_parts, (
    "the fill-surface method must not rebuild a separate wall shell; Orca keeps the original perimeter")
assert count >= 1
assert edited.area < original.area, (
    "the geometry-stage prototype should replace unsupported fill with bridge ribbons")
for bridge in bridge_parts:
    assert current.buffer(0.002).covers(bridge), (
        "bridge-classified Waves must stay inside the prepared fill domain")

blocked_cfg = dict(cfg)
blocked_cfg["min_wave_length"] = 999.0
blocked_original, blocked_edited, blocked_bridges, blocked_walls, _ = wave.plan_layer_geometry(
    [(None, [], [current])], support, blocked_cfg)
assert not blocked_bridges and not blocked_walls and blocked_edited.equals(blocked_original), (
    "unsupported fill must remain unchanged when no anchored Wave survives")

# The capability remains harmless when the fake host does not provide a live
# PrintObject. This is the fail-closed path used by older Orca builds.
cap = wave.WaveOverhangsGeometrySlicing()
with tempfile.TemporaryDirectory() as tmp:
    os.environ["ORCA_PLUGIN_STORAGE_DIR"] = tmp
    try:
        result = cap.execute(fake_orca.Context(fake_orca.Step.posPrepareInfill))
    finally:
        os.environ.pop("ORCA_PLUGIN_STORAGE_DIR", None)
assert result.ok

print("ok -- geometry-stage Wave fronts become bridge fill surfaces, stay "
      "inside the fill domain, and avoid the circular hole")
