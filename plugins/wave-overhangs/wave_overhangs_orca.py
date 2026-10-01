# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy>=2.0", "shapely>=2.0"]
#
# [tool.orcaslicer.plugin]
# name = "Wave Overhangs"
# description = "Experimental: print steep overhangs support-free by replacing the overhang region with wave-propagated toolpaths (port of the WaveOverhangs fork's algorithm as a slicing-pipeline plugin). | What's new in v0.0.14: Added obstacle-aware propagation for holes entirely inside the unsupported plane."
# author = "Wave Overhangs plugin lane"
# version = "0.0.14"
# ///
"""Wave Overhangs for OrcaSlicer -- experimental slicing-pipeline plugin.

WHAT IT AIMS TO DO
  Reproduce dennisklappe/OrcaSlicer-WaveOverhangs (a C++ fork) as a Python plugin:
  detect each layer's unsupported overhang, and fill it with wave-propagated
  toolpaths (expanding fronts anchored to the supported edge) printed slowly with
  full cooling, instead of ordinary infill that would sag.

HOW IT WORKS (one transactional exported-G-code pass)
  1. `posSlice` does not mutate Orca geometry or keep a cross-callback plan.
  2. `psGCodePostProcess` reads the exported `Bridge` and `Internal Bridge`
     moves in absolute bed coordinates, plus the previous layer's support moves.
  3. It grows wavefronts from supported material, simplifies edge chatter, and
     removes only bridge centerline portions covered by successful wave paths.
  4. It re-emits every substantial uncovered bridge fragment and writes the file
     only after parsing, generation, subtraction, and assembly succeed.

The captured real-export regression is in `tests/fixtures/` and
`tests/test_wave_gcode.py`. A fresh 0.0.13 export and a physical print are still
needed. Nothing here may crash a slice: failures retain the original G-code.
"""
import json
import math
import os
import re
import time

import orca

# --- audit-safe dependency load (see the Support Fins plugin for the rationale:
#     Orca's plugin audit blocks any path containing "conf" during a capability,
#     and numpy imports numpy/__config__.py -- so import at load, never lazily) ---
np = None
shapely = None
_DEPS_ERROR = None


def _import_deps():
    global np, shapely, _DEPS_ERROR
    try:
        import numpy
        import numpy.__config__            # noqa: F401  (warm "conf" file now)
        import numpy._core._ufunc_config    # noqa: F401
        import shapely as _sh
        import shapely.geometry             # noqa: F401
        import shapely.ops                  # noqa: F401
        np = numpy
        shapely = _sh
        _DEPS_ERROR = None
    except ImportError:
        _DEPS_ERROR = ("Wave Overhangs is finishing its first-time dependency "
                       "install (numpy, shapely). Fully quit and reopen "
                       "OrcaSlicer, then try again.")
    except Exception as e:  # pragma: no cover - defensive
        _DEPS_ERROR = f"Wave Overhangs could not load its deps: {type(e).__name__}: {e}"


_import_deps()

# wave_core is embedded in this single-file plugin. It is executed at module
# load so its shapely imports run before capability audit restrictions apply.
import sys as _sys, types as _types
# Register the embedded module so its dataclasses can resolve annotations.
_WAVE_CORE_SRC = "\"\"\"Wave-overhang toolpath core -- pure geometry, no OrcaSlicer bindings.\n\nThis is the algorithm behind dennisklappe/OrcaSlicer-WaveOverhangs (itself a port\nof stmcculloch/PrusaSlicer-WaveOverhangs), reimplemented as a slicer-independent\nPython module so it can run inside an Orca slicing-pipeline plugin AND be unit\ntested offline with shapely.\n\nThe idea (see waveoverhangs.com \"How it works\"):\n\n  * For each layer, the *overhang region* is the part of the layer that sticks out\n    past the layer below -- it has nothing underneath it.\n  * A *seed* is taken at the supported edge (the boundary between the overhang and\n    the material below).\n  * Wavefronts are grown outward from the seed into the overhang: each front is\n    the set of points a fixed distance further from the supported edge than the\n    last. Because we grow by buffering the supported region, the fronts naturally\n    diffract around corners and holes, exactly like ripples on a pond.\n  * Each front becomes an extrusion polyline. A pattern mode decides how the\n    fronts are connected into a print order.\n\nEverything here is in millimetres, in the object's own XY frame. Mapping into the\nprinter's absolute G-code coordinates is the plugin's job (see the plugin module).\n\"\"\"\nfrom __future__ import annotations\n\nimport math\nfrom dataclasses import dataclass, field\n\nfrom shapely.geometry import (\n    GeometryCollection,\n    LineString,\n    MultiLineString,\n    MultiPolygon,\n    Polygon,\n)\nfrom shapely.ops import linemerge, unary_union\n\n# ---------------------------------------------------------------------------------\n# Configuration\n# ---------------------------------------------------------------------------------\n\n\n@dataclass\nclass WaveConfig:\n    \"\"\"Mirrors the fork's tunables (waveoverhangs.com \"~20 expert tunables\").\"\"\"\n\n    # Detection\n    overhang_tol: float = 0.05        # mm the layer below is grown before subtracting\n    min_overhang_area: float = 0.5    # mm^2, ignore slivers\n\n    # Wave field\n    line_spacing: float = 0.35        # mm, centreline spacing between wave tracks\n    line_width: float = 0.40          # mm, extrusion width of a wave line\n    max_iterations: int = 400         # safety cap on wavefronts per region\n    perimeter_overlap: float = 0.10   # mm, push the field toward the kept perimeter\n\n    # Pattern: \"monotonic\" | \"zigzag\" | \"smart\"\n    pattern: str = \"smart\"\n\n    # Motion / cooling / flow (used by the G-code emitter)\n    layer_height: float = 0.20\n    flow_ratio: float = 1.0\n    filament_diameter: float = 1.75\n    print_speed: float = 2.0          # mm/s\n    travel_speed: float = 120.0       # mm/s\n    fan: float = 1.0                  # 0..1, forced during wave extrusion\n\n    def mm3_per_mm(self) -> float:\n        return self.line_width * self.layer_height * self.flow_ratio\n\n    def e_per_mm(self) -> float:\n        area = math.pi * (self.filament_diameter / 2.0) ** 2\n        return self.mm3_per_mm() / area\n\n\n# ---------------------------------------------------------------------------------\n# Geometry helpers\n# ---------------------------------------------------------------------------------\n\n\ndef _iter_lines(geom):\n    \"\"\"Yield LineStrings from any shapely geometry (skip empties/points).\"\"\"\n    if geom is None or geom.is_empty:\n        return\n    if isinstance(geom, LineString):\n        yield geom\n    elif isinstance(geom, (MultiLineString, GeometryCollection)):\n        for g in geom.geoms:\n            yield from _iter_lines(g)\n    elif hasattr(geom, \"boundary\"):\n        yield from _iter_lines(geom.boundary)\n\n\ndef overhang_region(layer: Polygon, support: Polygon, cfg: WaveConfig):\n    \"\"\"The part of `layer` that overhangs open air (not over `support`).\n\n    `layer`   : this layer's sliced area.\n    `support` : the layer-below area (what this layer can rest on). Empty for the\n                first layer -> the whole layer is \"supported\" by the bed, so no\n                overhang.\n    \"\"\"\n    if support is None or support.is_empty:\n        # First layer / nothing below: treat as fully supported by the bed.\n        return Polygon()\n    grown = support.buffer(cfg.overhang_tol) if cfg.overhang_tol else support\n    ov = layer.difference(grown)\n    if ov.is_empty:\n        return ov\n    # Drop slivers below the area threshold.\n    keep = [p for p in _polys(ov) if p.area >= cfg.min_overhang_area]\n    return unary_union(keep) if keep else Polygon()\n\n\ndef _polys(geom):\n    if geom.is_empty:\n        return []\n    if isinstance(geom, Polygon):\n        return [geom]\n    if isinstance(geom, MultiPolygon):\n        return list(geom.geoms)\n    if isinstance(geom, GeometryCollection):\n        out = []\n        for g in geom.geoms:\n            out.extend(_polys(g))\n        return out\n    return []\n\n\n# ---------------------------------------------------------------------------------\n# Wavefront propagation\n# ---------------------------------------------------------------------------------\n\n\n@dataclass\nclass WaveTrack:\n    distance: float               # mm from the supported edge (front index * spacing)\n    points: list                  # [(x, y), ...] centreline polyline\n\n\ndef wave_tracks(support: Polygon, overhang: Polygon, cfg: WaveConfig):\n    \"\"\"Grow wavefronts from the supported edge across the overhang.\n\n    Returns a list of WaveTrack ordered near->far from support. Each track is the\n    portion of an offset of the supported boundary that lies inside the overhang.\n    \"\"\"\n    tracks: list[WaveTrack] = []\n    if overhang is None or overhang.is_empty or support is None or support.is_empty:\n        return tracks\n\n    # Let the first front sit half a spacing into the overhang, then step outward.\n    # perimeter_overlap nudges the whole field back toward the kept perimeter/support\n    # so the last front hugs the supported edge on the far side.\n    base = 0.5 * cfg.line_spacing - cfg.perimeter_overlap\n    target = overhang.buffer(1e-6)\n    has_internal_void = any(poly.interiors for poly in _polys(overhang))\n    domain = support.union(target) if has_internal_void else None\n    reachable = support\n    for i in range(cfg.max_iterations):\n        d = base + i * cfg.line_spacing\n        if d <= 0:\n            continue\n        if not has_internal_void:\n            # Preserve the established fast path for ordinary overhangs.\n            grown = support.buffer(d)\n            front = grown.boundary.intersection(target)\n            made_any = False\n            for ln in _iter_lines(front):\n                if ln.length <= 1e-6:\n                    continue\n                tracks.append(WaveTrack(distance=d, points=list(ln.coords)))\n                made_any = True\n            if grown.contains(target):\n                break\n            if not made_any and d > 1e-6 and grown.covers(target):\n                break\n            continue\n\n        # An internal hole is an obstacle, not merely a clipped part of the\n        # target. Grow the already-reachable region through the real domain so\n        # the front stops at the hole and advances around both sides.\n        previous = reachable\n        if i == 0:\n            reachable = support.buffer(d).intersection(domain)\n        else:\n            reachable = reachable.buffer(cfg.line_spacing).intersection(domain)\n        front = reachable.boundary.intersection(target)\n        if i > 0:\n            # Do not emit a domain or hole boundary again after it has\n            # already been reached. Only the newly advanced edge is a front.\n            front = front.difference(previous.buffer(1e-5))\n        made_any = False\n        for ln in _iter_lines(front):\n            if ln.length <= max(1e-6, cfg.line_spacing * 0.2):\n                continue\n            tracks.append(WaveTrack(distance=d, points=list(ln.coords)))\n            made_any = True\n        if reachable.covers(target):\n            break\n        if not made_any and reachable.equals(previous):\n            break\n    return tracks\n\n\n# ---------------------------------------------------------------------------------\n# Pattern / ordering\n# ---------------------------------------------------------------------------------\n\n\ndef _endpoints(pts):\n    return pts[0], pts[-1]\n\n\ndef _dist(a, b):\n    return math.hypot(a[0] - b[0], a[1] - b[1])\n\n\ndef order_tracks(tracks, support: Polygon, cfg: WaveConfig):\n    \"\"\"Turn wavefronts into an ordered list of printable polylines.\n\n    monotonic : print near->far, each front as its own line (lots of travels).\n    zigzag    : same order, but flip alternate fronts so the end of one is near\n                the start of the next -> connected back-and-forth motion.\n    smart     : like monotonic, but each front starts from its better-supported\n                (nearer-to-support) end so no line begins in thin air.\n    \"\"\"\n    ordered = sorted(tracks, key=lambda t: t.distance)\n    polylines = []\n    mode = (cfg.pattern or \"smart\").lower()\n\n    if mode == \"zigzag\":\n        flip = False\n        for t in ordered:\n            pts = list(reversed(t.points)) if flip else t.points\n            polylines.append(pts)\n            flip = not flip\n        return polylines\n\n    if mode == \"smart\" and support is not None and not support.is_empty:\n        for t in ordered:\n            a, b = _endpoints(t.points)\n            # Start from whichever end is closer to the supported region.\n            da = support.distance(_pt(a))\n            db = support.distance(_pt(b))\n            polylines.append(t.points if da <= db else list(reversed(t.points)))\n        return polylines\n\n    # monotonic (and fallback)\n    return [t.points for t in ordered]\n\n\ndef _pt(xy):\n    from shapely.geometry import Point\n\n    return Point(xy[0], xy[1])\n\n\n# ---------------------------------------------------------------------------------\n# G-code emission\n# ---------------------------------------------------------------------------------\n\n\ndef emit_layer_gcode(polylines, z, cfg: WaveConfig, restore_fan=None):\n    \"\"\"Emit G-code lines for one layer's wave polylines.\n\n    Coordinates are absolute bed XY; `z` is the layer height. `restore_fan` is an\n    optional 0..255 value to reset the fan to after the wave block (None = leave\n    the forced wave fan in place; the plugin usually passes the layer's fan back).\n    Relative-E is used inside the block and reset with M83/G92 so it composes with\n    Orca's own extrusion accounting.\n    \"\"\"\n    if not polylines:\n        return []\n    e_per_mm = cfg.e_per_mm()\n    print_f = int(round(cfg.print_speed * 60))\n    travel_f = int(round(cfg.travel_speed * 60))\n    out = [\"; ==== WAVE OVERHANG BEGIN ====\",\n           \"M83\",                                   # relative extrusion for our block\n           f\"M106 S{int(round(max(0.0, min(1.0, cfg.fan)) * 255))}\"]\n    for pts in polylines:\n        if len(pts) < 2:\n            continue\n        x0, y0 = pts[0]\n        out.append(f\"G0 F{travel_f} X{x0:.3f} Y{y0:.3f} Z{z:.3f}\")\n        out.append(f\"G1 F{print_f}\")\n        px, py = x0, y0\n        for (x, y) in pts[1:]:\n            seg = math.hypot(x - px, y - py)\n            if seg <= 1e-9:\n                continue\n            out.append(f\"G1 X{x:.3f} Y{y:.3f} E{seg * e_per_mm:.5f}\")\n            px, py = x, y\n    if restore_fan is not None:\n        out.append(f\"M106 S{int(restore_fan)}\")\n    out.append(\"; ==== WAVE OVERHANG END ====\")\n    return out\n\n\n# ---------------------------------------------------------------------------------\n# One-call convenience\n# ---------------------------------------------------------------------------------\n\n\n@dataclass\nclass LayerWaveResult:\n    z: float\n    polylines: list = field(default_factory=list)\n    overhang_area: float = 0.0\n    n_tracks: int = 0\n\n\ndef plan_layer(layer: Polygon, support: Polygon, z: float, cfg: WaveConfig):\n    \"\"\"Full per-layer plan: detect overhang, propagate waves, order them.\"\"\"\n    ov = overhang_region(layer, support, cfg)\n    if ov.is_empty:\n        return LayerWaveResult(z=z)\n    tracks = wave_tracks(support, ov, cfg)\n    polylines = order_tracks(tracks, support, cfg)\n    return LayerWaveResult(z=z, polylines=polylines,\n                           overhang_area=float(ov.area), n_tracks=len(tracks))\n\n\n# ---------------------------------------------------------------------------------\n# G-code layer parsing, self-calibration and splicing (pure text; unit tested)\n# ---------------------------------------------------------------------------------\n\nZ_KEYS = (\";Z:\", \";HEIGHT:\", \";LAYER_Z:\")\n\n\ndef parse_layer_z(line: str):\n    \"\"\"The layer height a G-code line announces, or None.\n\n    Handles Orca/Prusa comment markers (;Z: / ;HEIGHT: / ;LAYER_Z:) and a bare\n    layer-change move (`G1 Z.. F..` with no X/Y).\n    \"\"\"\n    s = line.strip()\n    for k in Z_KEYS:\n        if s.startswith(k):\n            try:\n                return float(s[len(k):].strip().split()[0])\n            except Exception:\n                return None\n    if s[:2] in (\"G0\", \"G1\") and \"Z\" in s and \" X\" not in (\" \" + s) and \" Y\" not in (\" \" + s):\n        for tok in s.split():\n            if tok.startswith(\"Z\"):\n                try:\n                    return float(tok[1:])\n                except Exception:\n                    return None\n    return None\n\n\ndef _extruding_xy(line: str):\n    \"\"\"(x, y) for an extruding G1 move (has X, Y and an E token), else None.\"\"\"\n    s = line.strip()\n    if not s.startswith(\"G1\"):\n        return None\n    x = y = None\n    has_e = False\n    for tok in s.split():\n        if tok.startswith(\"X\"):\n            try:\n                x = float(tok[1:])\n            except Exception:\n                return None\n        elif tok.startswith(\"Y\"):\n            try:\n                y = float(tok[1:])\n            except Exception:\n                return None\n        elif tok.startswith(\"E\"):\n            has_e = True\n    if x is not None and y is not None and has_e:\n        return (x, y)\n    return None\n\n\ndef layer_extrusion_min(lines, target_z, tol=1e-3):\n    \"\"\"Min (x, y) corner of extruding moves on the layer nearest `target_z`.\"\"\"\n    minx = miny = None\n    cur = None\n    for line in lines:\n        z = parse_layer_z(line)\n        if z is not None:\n            cur = z\n            continue\n        if cur is not None and abs(cur - target_z) <= tol:\n            xy = _extruding_xy(line)\n            if xy is not None:\n                minx = xy[0] if minx is None else min(minx, xy[0])\n                miny = xy[1] if miny is None else min(miny, xy[1])\n    return (minx, miny)\n\n\ndef _match_z(z, plans, tol=1e-3):\n    for pz in plans:\n        if abs(pz - z) <= tol:\n            return pz\n    return None\n\n\ndef splice_gcode(text, layer_plans, cfg: WaveConfig, calibration):\n    \"\"\"Insert wave moves into exported G-code. Pure text in / out.\n\n    layer_plans : {round(z,3): [polyline_in_object_frame, ...]}\n    calibration : (\"manual\", dx, dy)                     -> use this XY offset, or\n                  (\"auto\", calib_z, obj_min_x, obj_min_y) -> derive the offset by\n                    aligning Orca's own printed outline on layer `calib_z` to the\n                    object-frame outline min corner (a pure translation).\n\n    Wave moves for a layer are inserted just before the NEXT layer marker, i.e.\n    after Orca has printed that layer's own perimeters/infill.\n    Returns (new_text, inserted_layer_count, (dx, dy)).\n    \"\"\"\n    lines = text.splitlines(keepends=True)\n\n    if calibration and calibration[0] == \"manual\":\n        dx, dy = float(calibration[1]), float(calibration[2])\n    elif calibration and calibration[0] == \"auto\":\n        _, cz, omx, omy = calibration\n        gmin = layer_extrusion_min(lines, cz)\n        if gmin[0] is None or omx is None:\n            dx, dy = 0.0, 0.0\n        else:\n            dx, dy = gmin[0] - omx, gmin[1] - omy\n    else:\n        dx, dy = 0.0, 0.0\n\n    out = []\n    inserted = 0\n    pending = None  # (z, polylines) waiting to be flushed at the next layer marker\n\n    def flush():\n        nonlocal inserted\n        if pending is None:\n            return\n        z, polys = pending\n        shifted = [[(x + dx, y + dy) for (x, y) in pts] for pts in polys]\n        for ln in emit_layer_gcode(shifted, z, cfg):\n            out.append(ln + \"\\n\")\n        inserted += 1\n\n    for line in lines:\n        z = parse_layer_z(line)\n        if z is not None:\n            flush()\n            pending = None\n            key = _match_z(z, layer_plans)\n            if key is not None:\n                pending = (z, layer_plans[key])\n        out.append(line)\n    flush()\n\n    return \"\".join(out), inserted, (dx, dy)\n\n"
wc = _types.ModuleType("wave_core")
_sys.modules["wave_core"] = wc
try:
    exec(compile(_WAVE_CORE_SRC, "wave_core (inlined)", "exec"), wc.__dict__)
except Exception:  # pragma: no cover - surfaced via the setup check / execute()
    _sys.modules.pop("wave_core", None)
    wc = None


_DEFAULTS = {
    "enabled": True,
    "overhang_tol": 0.05,
    "min_overhang_area": 0.5,
    "line_spacing": 0.35,
    "line_width": 0.40,        # fallback; exported Bridge width takes precedence
    "perimeter_overlap": 0.10,
    "pattern": "smart",          # "smart" | "monotonic" | "zigzag"
    "start_policy": "supported",  # "supported" | "consistent" | min/max-x/y
    "min_wave_length": 1.0,       # mm; discard isolated wavefronts shorter than this
    "min_wave_segment": 0.30,     # mm; suppress short endpoint/stub extrusions
    "simplify_tolerance": 0.05,   # mm; remove harmless boundary point noise
    "min_bridge_fragment": 0.5,   # line-width multiplier for retained bridge pieces
    "flow_ratio": 1.0,
    "print_speed": 2.0,
    "travel_speed": 120.0,
    "fan": 1.0,
    "max_iterations": 400,
}

# The version this file was built as. Kept in lockstep with the PEP 723 header
# at the top (tests/test_installer.py fails if they drift), so everything that
# reports a version at runtime reports the one actually running.
PLUGIN_VERSION = "0.0.14"

# --- BEGIN changelog (generated by tools/sync_changelog.py) ---
CHANGELOG_RECENT = """\
v0.0.14  (2026-10-01)
   * Added obstacle-aware propagation for holes entirely inside the
     unsupported plane. Fronts now continue around both sides instead of
     only reacting when a hole touches the supported boundary.
   * Prevented curved Wave fronts from being simplified into straight
     chords through holes or concave voids. Topology-safe cleanup keeps
     the original boundary when a shortcut would leave the unsupported
     region or enter an interior hole.
   * Added internal-hole and circular-hole regressions alongside the
     concave-front test. Disconnected components remain separate and
     their travel remains non-extruding.
   * The captured Cube export keeps its 3-layer, 386-Wave-move result
     and the existing cleanup/bridge-retention measurements. A fresh
     0.0.14 Orca export and physical print are still required.

v0.0.13  (2026-10-01)
   * Fixed the Wave-to-original-toolpath handoff. Every replaced bridge
     segment now ends with an explicit non-extruding G0 travel to its
     original endpoint, so the next retained G1 move cannot draw a sharp
     line from a Wave endpoint.
   * Refined cleanup with configurable minimum front length, minimum
     Wave segment length, simplification tolerance, and retained-bridge
     fragment threshold. The captured fixture now emits 386 cleaned Wave
     extrusion moves and removes 30 isolated short fronts.
   * Added real pattern and endpoint settings: smart, monotonic, and
     zigzag, plus supported, consistent, and min/max X/Y endpoint
     policies. Monotonic keeps one direction across fronts while all
     inter-front travel remains non-extruding.
   * Added safe uniform absolute-E support: the temporary relative block
     restores M82 and the prior command value with G92; mixed E-mode
     sections fail closed. The fixture retains 31 substantial uncovered
     fragments and remains idempotent. A fresh 0.0.13 Orca export and
     physical print are still required.

v0.0.12  (2026-10-01)
   * Fixed the real-print Z alignment: Orca's ;Z: comment is nominal and
     the owner's profile adds z_offset = 0.25. Waves now use the bridge
     extrusion's actual modal nozzle Z, changing the test layers from
     5.4/9.6/14.4 mm to the correct 5.65/9.85/14.65 mm.
   * Cleaned up edge dots by simplifying each conforming wavefront,
     merging sub-0.15 mm chatter, deriving Wave width from Orca's real
     bridge width (0.573 mm in the captured export), and dropping
     isolated uncovered remnants shorter than half a line width.
   * On the captured 0.30 mm cube export, wave extrusion moves fall from
     roughly 1,850 tiny moves to 436 clean moves. Twenty-five
     substantial uncovered fragments remain; 121 sub-nozzle remnants are
     removed.
   * This cleanup is offline-tested against the captured export.
     Alignment and surface quality still require a fresh Orca export and
     physical print.
"""
# --- END changelog ---

# Stamped into the exported G-code, so the file itself says which build made
# the waves -- no need to open OrcaSlicer to find out.
WAVE_STAMP_PREFIX = "; wave-overhangs"
WAVE_STAMP = f"{WAVE_STAMP_PREFIX} v{PLUGIN_VERSION} (wave overhang toolpaths)\n"

# The active implementation is intentionally G-code-only. These state helpers
# record diagnostics for Check setup; they do not carry geometry between steps.

def _state_path():
    return os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "wave_overhangs_state.json")


def _load_state():
    """What happened on previous slices. Empty dict if we have no history."""
    try:
        with open(_state_path(), "r", encoding="utf-8") as f:
            s = json.load(f)
        return s if isinstance(s, dict) else {}
    except Exception:
        return {}


def _save_state(state):
    try:
        with open(_state_path(), "w", encoding="utf-8") as f:
            json.dump(state, f)
    except Exception:
        pass


def _splice_confirmed():
    """Return whether an export pass has previously been observed.

    This is a diagnostic fact for Check setup and the shared log. The active
    rewrite does not use it to permit or deny geometry changes.
    """
    return bool(_load_state().get("splice_ever"))


def _cfg(self):
    try:
        src = json.loads(self.get_config() or "{}")
    except (AttributeError, TypeError, ValueError):
        src = {}
    cfg = dict(_DEFAULTS)
    for k, v in src.items():
        if k in cfg:
            cfg[k] = v
    return cfg


def _wave_config(cfg, layer_height):
    return wc.WaveConfig(
        overhang_tol=float(cfg["overhang_tol"]),
        min_overhang_area=float(cfg["min_overhang_area"]),
        line_spacing=float(cfg["line_spacing"]),
        line_width=float(cfg["line_width"]),
        perimeter_overlap=float(cfg["perimeter_overlap"]),
        pattern=str(cfg["pattern"]),
        layer_height=float(layer_height),
        flow_ratio=float(cfg["flow_ratio"]),
        print_speed=float(cfg["print_speed"]),
        travel_speed=float(cfg["travel_speed"]),
        fan=float(cfg["fan"]),
        max_iterations=int(cfg["max_iterations"]),
    )


# The old slice-object planning path was removed. Orca's exported G-code is the
# source of truth for both support and bridge geometry; keeping a second object
# frame would reintroduce the alignment and cross-callback failures this plugin
# is designed to avoid.

# ---------------------------------------------------------------------------------
# G-code-only replacement
# ---------------------------------------------------------------------------------

_GWORD = re.compile(r"([A-Z])(-?(?:\d+(?:\.\d*)?|\.\d+))")
_BRIDGE_TYPES = ("bridge", "internal bridge")


def _gwords(line):
    return {k: float(v) for k, v in _GWORD.findall(line.split(";", 1)[0])}


def _line_parts(geom):
    if geom is None or geom.is_empty:
        return []
    if geom.geom_type == "LineString":
        return [geom]
    if geom.geom_type in ("MultiLineString", "GeometryCollection"):
        out = []
        for g in geom.geoms:
            out.extend(_line_parts(g))
        return out
    return []


def _parse_gcode_geometry(lines):
    """Return layers and bridge sections using actual exported toolpaths."""
    layers = []
    layer = None
    section = None
    x = y = z = None
    width = 0.4
    relative_e = True
    e_position = 0.0
    fan = None
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith(";Z:"):
            try:
                layer = {"z": float(s[3:]), "all": [], "sections": []}
                layers.append(layer)
                section = None
            except ValueError:
                layer = None
            continue
        if s.startswith(";TYPE:"):
            name = s[6:].strip().lower()
            section = {"type": name, "segments": [], "marker": i, "fan": fan,
                       "relative_e": relative_e, "e_start": e_position}
            if layer is not None and name in _BRIDGE_TYPES:
                layer["sections"].append(section)
            continue
        if s.startswith(";WIDTH:"):
            try:
                width = float(s[7:])
            except ValueError:
                pass
            continue
        if s.startswith("M83"):
            relative_e = True
        elif s.startswith("M82"):
            relative_e = False
        elif s.startswith("M106"):
            try:
                fan = int(_gwords(s).get("S", 255))
            except (TypeError, ValueError):
                fan = None
        if s.startswith("G92"):
            reset = _gwords(s).get("E")
            if reset is not None:
                e_position = reset
            continue
        if not s.startswith(("G0", "G1")):
            continue
        words = _gwords(s)
        nx, ny = words.get("X", x), words.get("Y", y)
        nz = words.get("Z", z)
        e = words.get("E")
        e_start = e_position
        if e is None:
            e_delta = 0.0
        elif relative_e:
            e_delta = e
            e_position += e
        else:
            e_delta = e - e_position
            e_position = e
        extruding = e_delta > 0.0
        if (layer is not None and extruding and None not in (x, y, nx, ny)
                and (x, y) != (nx, ny)):
            seg = {"line": i, "a": (x, y), "b": (nx, ny), "z": nz,
                   "e": e_delta, "e_start": e_start, "e_end": e_position,
                   "relative_e": relative_e, "width": width,
                   "geom": shapely.geometry.LineString([(x, y), (nx, ny)])}
            layer["all"].append(seg)
            if section is not None and section["type"] in _BRIDGE_TYPES:
                section["segments"].append(seg)
        x, y, z = nx, ny, nz
    return layers


def _footprint(segments):
    polys = [s["geom"].buffer(s["width"] * 0.5, cap_style=2,
                               join_style=2) for s in segments]
    return shapely.ops.unary_union(polys) if polys else shapely.geometry.Polygon()


def _clean_wave_polyline(points, line_width, tolerance=0.05,
                         min_segment=0.30, allowed=None):
    """Remove point noise without shortcutting across a hole or concavity."""
    if len(points) < 2:
        return []
    tolerance = max(0.01, min(0.12, float(tolerance)))
    original = shapely.geometry.LineString(points)
    geom = original.simplify(tolerance, preserve_topology=False)
    coords = list(geom.coords)
    if len(coords) < 2:
        return []
    minimum = max(0.08, float(min_segment))
    clean = [coords[0]]
    for point in coords[1:-1]:
        if math.hypot(point[0] - clean[-1][0],
                      point[1] - clean[-1][1]) >= minimum:
            clean.append(point)
    end = coords[-1]
    # Keep the boundary endpoint, but merge a short final stub into the
    # previous segment. If the whole front is shorter than the threshold,
    # discard it as a dot rather than print a blob.
    while len(clean) > 1 and math.hypot(
            end[0] - clean[-1][0], end[1] - clean[-1][1]) < minimum:
        clean.pop()
    if math.hypot(end[0] - clean[-1][0], end[1] - clean[-1][1]) >= minimum:
        clean.append(end)
    if len(clean) < 2:
        return []

    # Simplifying a curved front can replace an arc with a straight chord.
    # With a hole, that chord can cross empty space. The original front came
    # from a Shapely boundary intersection and is the safe fallback.
    if allowed is not None:
        candidate = shapely.geometry.LineString(clean)
        guard = allowed.buffer(max(0.002, min(0.06, tolerance * 1.25)))
        voids = _interior_voids(allowed)
        if (not guard.covers(candidate)
                or (not voids.is_empty and candidate.intersects(voids))):
            return list(original.coords)
    return clean


def _polyline_length(points):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1])
               for a, b in zip(points, points[1:]))


def _interior_voids(geom):
    """Return polygon holes that a simplified front must never enter."""
    if geom is None or geom.is_empty:
        return shapely.geometry.GeometryCollection()
    polygons = ([geom] if geom.geom_type == "Polygon" else
                list(geom.geoms) if geom.geom_type == "MultiPolygon" else [])
    holes = []
    for polygon in polygons:
        for ring in polygon.interiors:
            hole = shapely.geometry.Polygon(ring).buffer(-0.001)
            if not hole.is_empty:
                holes.append(hole)
    return shapely.ops.unary_union(holes) if holes else shapely.geometry.GeometryCollection()


def _endpoint_order(points, policy, support=None):
    """Orient one front without making an arc start at an arbitrary midpoint."""
    if len(points) < 2:
        return points
    a, b = points[0], points[-1]
    policy = str(policy or "supported").lower()
    if policy == "supported" and support is not None and not support.is_empty:
        da, db = support.distance(shapely.geometry.Point(a)), support.distance(
            shapely.geometry.Point(b))
        if abs(da - db) > 1e-6:
            return points if da < db else list(reversed(points))
        policy = "min-x"
    key = {
        "min-x": lambda p: (p[0], p[1]),
        "max-x": lambda p: (-p[0], p[1]),
        "min-y": lambda p: (p[1], p[0]),
        "max-y": lambda p: (-p[1], p[0]),
    }.get(policy)
    if key is None:
        return points
    return points if key(a) <= key(b) else list(reversed(points))


def _order_wave_tracks(tracks, support, pattern="smart", start_policy="supported"):
    """Order and orient fronts while keeping travel moves non-extruding.

    `monotonic` uses one global endpoint direction for all fronts. It does not
    extrude across the gap between fronts: those connecting moves remain G0
    travels so a warm perimeter is never scored by an accidental line.
    """
    ordered = sorted(tracks, key=lambda t: t.distance)
    if not ordered:
        return []
    mode = str(pattern or "smart").lower()
    if mode not in ("smart", "monotonic", "zigzag"):
        mode = "smart"
    if mode == "monotonic" or (mode == "smart" and
                                str(start_policy).lower() == "consistent"):
        first = list(ordered[0].points)
        if len(first) >= 2:
            dx = first[-1][0] - first[0][0]
            dy = first[-1][1] - first[0][1]
            # Use a single projection axis so every front has a consistent
            # direction even when Shapely returns its endpoints unpredictably.
            if abs(dx) + abs(dy) < 1e-9:
                dx, dy = 1.0, 0.0
            want_low = (first[0][0] * dx + first[0][1] * dy <=
                        first[-1][0] * dx + first[-1][1] * dy)
            out = []
            for track in ordered:
                pts = list(track.points)
                a, b = pts[0], pts[-1]
                low = a[0] * dx + a[1] * dy <= b[0] * dx + b[1] * dy
                out.append(pts if low == want_low else list(reversed(pts)))
            return out
    out = []
    for index, track in enumerate(ordered):
        pts = _endpoint_order(list(track.points), start_policy, support)
        if mode == "zigzag" and index % 2:
            pts = list(reversed(pts))
        out.append(pts)
    return out


def _gcode_wave_rewrite(text, cfg):
    """Replace covered bridge extrusion and retain every uncovered fragment.

    Planning, insertion and subtraction happen in this one in-memory operation.
    Any exception returns the original text unchanged.
    """
    if WAVE_STAMP_PREFIX in text:
        return text, {"replaced_sections": 0, "wave_layers": 0,
                      "already_processed": True}
    lines = text.splitlines(keepends=True)
    try:
        layers = _parse_gcode_geometry(lines)
        replacements = {}
        sections_done = wave_layers = removed = kept = dropped = paths_dropped = 0
        for li, layer in enumerate(layers):
            if li == 0 or not layer["sections"]:
                continue
            support = _footprint(layers[li - 1]["all"])
            if support.is_empty:
                continue
            layer_changed = False
            for sec in layer["sections"]:
                if not sec["segments"]:
                    continue
                bridge = _footprint(sec["segments"])
                swcfg = _wave_config(cfg, cfg.get("_lh", 0.2))
                widths = sorted(s["width"] for s in sec["segments"])
                swcfg.line_width = widths[len(widths) // 2]
                unsupported = bridge.difference(support.buffer(swcfg.overhang_tol))
                if unsupported.is_empty or unsupported.area < swcfg.min_overhang_area:
                    continue
                tracks = wc.wave_tracks(support, unsupported, swcfg)
                polylines = _order_wave_tracks(
                    tracks, support, cfg.get("pattern"), cfg.get("start_policy"))
                polylines = [
                    _clean_wave_polyline(
                        p, swcfg.line_width,
                        cfg.get("simplify_tolerance", 0.05),
                        cfg.get("min_wave_segment", 0.30),
                        allowed=unsupported)
                    for p in polylines]
                minimum_wave = max(0.0, float(cfg.get("min_wave_length", 1.0)))
                kept_polylines = []
                for polyline in polylines:
                    if len(polyline) >= 2 and _polyline_length(polyline) >= minimum_wave:
                        kept_polylines.append(polyline)
                    else:
                        paths_dropped += 1
                polylines = kept_polylines
                if not polylines:
                    continue
                wave_lines = [shapely.geometry.LineString(p) for p in polylines
                              if len(p) >= 2]
                if not wave_lines:
                    continue
                coverage = shapely.ops.unary_union([
                    p.buffer(swcfg.line_width * 0.55, cap_style=2, join_style=2)
                    for p in wave_lines])
                if coverage.is_empty:
                    continue

                # Build this section separately. Do not insert reinforcement
                # waves unless at least one original move is truly covered.
                sec_replacements = {}
                replaced_segments = []
                sec_removed = sec_kept = sec_dropped = 0
                for seg in sec["segments"]:
                    remaining = seg["geom"].difference(coverage)
                    parts = _line_parts(remaining)
                    original_len = seg["geom"].length
                    covered_len = max(0.0, original_len - sum(p.length for p in parts))
                    if covered_len <= 1e-5:
                        continue
                    sec_removed += 1
                    replaced_segments.append(seg)
                    out = [f"; wave-overhangs replaced covered bridge move {seg['line'] + 1}\n"]
                    if not seg["relative_e"]:
                        # Removed absolute-E moves no longer advance the
                        # original command value. Re-anchor each retained
                        # fragment to the source value before emitting it.
                        out.append(f"G92 E{seg['e_start']:.5f}\n")
                    e_cursor = seg["e_start"]
                    for part in parts:
                        coords = list(part.coords)
                        if len(coords) < 2 or part.length <= 1e-5:
                            continue
                        min_fragment = max(
                            0.0, float(cfg.get("min_bridge_fragment", 0.5)))
                        if part.length < swcfg.line_width * min_fragment:
                            sec_dropped += 1
                            continue
                        ax, ay = coords[0]
                        out.append(f"G0 X{ax:.3f} Y{ay:.3f}\n")
                        share = seg["e"] * part.length / original_len
                        for pi, (bx, by) in enumerate(coords[1:], 1):
                            prev = coords[pi - 1]
                            piece = math.hypot(bx - prev[0], by - prev[1])
                            e_piece = share * piece / part.length
                            if seg["relative_e"]:
                                e_word = e_piece
                            else:
                                e_cursor += e_piece
                                e_word = e_cursor
                            out.append(f"G1 X{bx:.3f} Y{by:.3f} E{e_word:.5f}\n")
                        sec_kept += 1
                    # Covered or discarded pieces may leave the nozzle at a
                    # Wave endpoint. Restore the original segment's endpoint
                    # before the next original G1 move, or that move would
                    # extrude a sharp diagonal through the new Wave field.
                    out.append(f"G0 X{seg['b'][0]:.3f} Y{seg['b'][1]:.3f}\n")
                    sec_replacements.setdefault(seg["line"], []).extend(out)
                if not sec_removed:
                    continue
                e_modes = {seg["relative_e"] for seg in sec["segments"]}
                if len(e_modes) > 1:
                    # A mode switch inside one bridge section would need a
                    # much larger state machine. Leave that unusual section
                    # untouched rather than risk an E jump.
                    continue
                absolute_e = not next(iter(e_modes))
                if absolute_e:
                    last_replaced = replaced_segments[-1]
                    sec_replacements[last_replaced["line"]].append(
                        f"G92 E{last_replaced['e_end']:.5f}\n")

                first = sec["segments"][0]["line"]
                actual_z = sec["segments"][0].get("z")
                if actual_z is None:
                    actual_z = layer["z"]
                block = wc.emit_layer_gcode(polylines, actual_z, swcfg,
                                             restore_fan=sec.get("fan"))
                if absolute_e:
                    # The emitter uses relative E internally. Return to the
                    # source's absolute mode and command value before the
                    # original retained bridge lines are replayed.
                    block = block[:-1] + ["M82",
                                          f"G92 E{sec['segments'][0]['e_start']:.5f}",
                                          block[-1]]
                sec_replacements.setdefault(first, [])[:0] = [s + "\n" for s in block]
                for line_no, emitted in sec_replacements.items():
                    replacements.setdefault(line_no, []).extend(emitted)
                removed += sec_removed
                kept += sec_kept
                dropped += sec_dropped
                sections_done += 1
                layer_changed = True
            if layer_changed:
                wave_layers += 1
        if not sections_done:
            return text, {"replaced_sections": 0, "wave_layers": 0,
                          "removed_moves": 0, "kept_fragments": 0,
                          "tiny_fragments_dropped": 0,
                          "short_wave_paths_dropped": paths_dropped}
        out = []
        for i, line in enumerate(lines):
            if i in replacements:
                out.extend(replacements[i])
            else:
                out.append(line)
        return WAVE_STAMP + "".join(out), {
            "replaced_sections": sections_done, "wave_layers": wave_layers,
            "removed_moves": removed, "kept_fragments": kept,
            "tiny_fragments_dropped": dropped,
            "short_wave_paths_dropped": paths_dropped,
            "already_processed": False}
    except Exception as e:
        return text, {"replaced_sections": 0, "wave_layers": 0,
                      "short_wave_paths_dropped": 0,
                      "error": f"{type(e).__name__}: {e}"}


def _splice_gcode(gcode_path, cfg, log):
    with open(gcode_path, "r", encoding="utf-8", errors="ignore") as f:
        text = f.read()
    # G-code records its actual layer height; use it rather than depending on
    # state from an earlier Orca pipeline callback.
    m = re.search(r"^;\s*layer_height\s*=\s*([\d.]+)", text, re.MULTILINE)
    if m:
        cfg = dict(cfg)
        cfg["_lh"] = float(m.group(1))
    new_text, stats = _gcode_wave_rewrite(text, cfg)
    if new_text != text:
        with open(gcode_path, "w", encoding="utf-8") as f:
            f.write(new_text)
    log.update(stats)
    log["spliced_layers"] = stats.get("wave_layers", 0)
    return stats.get("wave_layers", 0)


# ---------------------------------------------------------------------------------
# Capabilities
# ---------------------------------------------------------------------------------


class WaveOverhangsSlicing(orca.slicing.SlicingPipelineCapabilityBase):
    def get_name(self):
        return "Wave Overhangs"

    def get_default_config(self):
        return _DEFAULTS

    def execute(self, ctx):
        _log_loaded_once()
        cfg = _cfg(self)
        if not cfg["enabled"]:
            return orca.ExecutionResult.success("Wave Overhangs: disabled")
        if np is None or shapely is None or wc is None:
            return orca.ExecutionResult.failure(
                orca.PluginResult.RecoverableError,
                _DEPS_ERROR or "Wave Overhangs needs numpy + shapely.")

        # G-code-only design: do not mutate Orca's internal slice graph and do
        # not rely on state surviving between pipeline callbacks. Planning,
        # insertion and covered-bridge subtraction all happen transactionally
        # at psGCodePostProcess below.
        if ctx.step == orca.slicing.Step.posSlice:
            return orca.ExecutionResult.success(
                "Wave Overhangs: waiting for exported Bridge G-code")

        # --- g-code seam: splice ---
        if ctx.step == orca.slicing.Step.psGCodePostProcess:
            # One-pass G-code implementation: no cross-callback geometry
            # survives from Orca's earlier posSlice callback.
            first_splice = not _splice_confirmed()
            log = {"phase": "gcode", "started": time.time(),
                   "first_splice": first_splice}
            try:
                n = _splice_gcode(ctx.gcode_path, cfg, log)
            except Exception as e:
                log["error"] = f"{type(e).__name__}: {e}"
                _write_log(log)
                # Don't fail the export over post-processing.
                return orca.ExecutionResult.success(
                    f"Wave Overhangs: splice skipped ({type(e).__name__})")
            # The seam ran. Record it for Check setup and the shared log.
            st = _load_state()
            st["splice_ever"] = True
            st["last_splice_at"] = time.time()
            st["last_splice_layers"] = n
            _save_state(st)

            log["seconds"] = round(time.time() - log["started"], 3)
            _write_log(log)
            if n:
                return orca.ExecutionResult.success(
                    f"Wave Overhangs: replaced covered bridge extrusion on "
                    f"{n} layer(s); uncovered fragments were retained")
            return orca.ExecutionResult.success(
                "Wave Overhangs: no unsupported Bridge sections were replaced")

        return orca.ExecutionResult.success()


class WaveOverhangsCheck(orca.script.ScriptPluginCapabilityBase):
    def get_name(self):
        return "Wave Overhangs - Check setup"

    def execute(self):
        _log_loaded_once()
        lines = [f"Wave Overhangs v{PLUGIN_VERSION} -- setup check"]
        if np is None or shapely is None:
            # This is the state the owner is most likely to be debugging, so
            # say the version and what changed here too -- not just on the
            # happy path further down.
            msg = [f"Wave Overhangs v{PLUGIN_VERSION} -- setup check",
                   "",
                   _DEPS_ERROR or "Wave Overhangs needs numpy + shapely.",
                   "",
                   "numpy and shapely are downloaded by OrcaSlicer itself the",
                   "first time this plugin is installed. If they never arrive:",
                   "  1. Fully quit OrcaSlicer (not just close the window) and",
                   "     reopen it -- the download is retried on startup.",
                   "  2. Check the Plugins dialog -> Diagnostics tab for the",
                   "     real error.",
                   "  3. A firewall or proxy blocking the download will show up",
                   "     there.",
                   "",
                   "Unlayered Infill does NOT need numpy or shapely, so it",
                   "keeps working regardless.",
                   "",
                   "--- what changed recently ---"]
            msg.extend(CHANGELOG_RECENT.splitlines())
            msg.append("")
            msg.append("Full history: plugins/wave-overhangs/CHANGELOG.md in the repo.")
            return orca.ExecutionResult.failure(
                orca.PluginResult.RecoverableError, "\n".join(msg))
        lines.append("deps: numpy + shapely loaded at startup (audit-safe)")
        lines.append(f"wave_core: {'inlined/available' if wc else 'MISSING (rebuild)'}")
        lines.append("mode: one transactional G-code pass")
        lines.append("source: exported Bridge/Internal Bridge paths")

        st = _load_state()
        lines.append("")
        lines.append("--- what the last export actually did ---")
        if st.get("last_splice_at"):
            lines.append(f"G-code step: ran, {st.get('last_splice_layers', 0)} "
                         "layer(s) replaced")
            lines.append("Covered bridge extrusion was removed; uncovered")
            lines.append("fragments were retained.")
        else:
            lines.append("No export recorded yet. Select Wave Overhangs under")
            lines.append("Others -> Slicing Pipeline Plugin and use Export G-code file.")
        lines.append("")
        lines.append("--- what changed recently ---")
        lines.extend(CHANGELOG_RECENT.splitlines())
        lines.append("")
        lines.append("Full history: plugins/wave-overhangs/CHANGELOG.md in the repo.")
        return orca.ExecutionResult.success("\n".join(lines))


# ---------------------------------------------------------------------------
#  Logging -- shared with the other plugins in this repo.
#
#  This used to be JSONL written next to the plugin file, inside Orca's data
#  folder: unfindable, and unreadable once found. It now appends plain text to
#  <Downloads>/orca-plugins.log, the same file Unlayered Infill writes to, so
#  one paste shows everything that happened.
# ---------------------------------------------------------------------------
LOG_NAME = "orca-plugins.log"
LOG_MAX_BYTES = 1000000          # roll over at ~1 MB so it cannot grow forever


def log_path():
    """<Downloads>/orca-plugins.log, or the best available stand-in."""
    override = os.environ.get("ORCA_PLUGIN_LOG_DIR")
    if override:
        return os.path.join(override, LOG_NAME)
    home = os.path.expanduser("~")
    downloads = os.path.join(home, "Downloads")
    return os.path.join(downloads if os.path.isdir(downloads) else home, LOG_NAME)


def _log(headline, *detail):
    """Append one readable block. Never raises -- logging must not break a print."""
    try:
        path = log_path()
        try:
            if os.path.getsize(path) > LOG_MAX_BYTES:
                os.replace(path, path + ".1")
        except OSError:
            pass
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        pad = " " * len(stamp)
        with open(path, "a", encoding="utf-8") as f:
            f.write(f"{stamp}  {headline}\n")
            for line in detail:
                f.write(f"{pad}    {line}\n")
    except BaseException:
        # Broader than Exception on purpose: Orca's audit hook can raise
        # non-Exception types, and a lost log line must never fail a print.
        pass


def _write_log(entry):
    """Render one run record into the shared log.

    Kept dict-shaped so every existing call site works unchanged.
    """
    if not isinstance(entry, dict):
        return
    phase = entry.get("phase") or ("plan" if "object" in entry else "run")
    head = f"Wave Overhangs v{PLUGIN_VERSION}: {phase}"
    if entry.get("error"):
        head = f"Wave Overhangs v{PLUGIN_VERSION}: ERROR during {phase}"
    detail = []
    for k, v in entry.items():
        if k in ("started", "phase"):
            continue
        if k == "layers" and isinstance(v, list):
            detail.append(f"{'layers':<17}: {len(v)} planned layer record(s)")
            continue
        detail.append(f"{k:<17}: {v}")
    _log(head, *detail)


# NEVER log at import time -- see the note in unlayered_infill_orca.py. Orca's
# audit hook turns a filesystem write during PluginLoader's import into a
# permission prompt or a failed load. Emitted lazily instead.
_LOADED_LOGGED = False


def _log_loaded_once():
    global _LOADED_LOGGED
    if _LOADED_LOGGED:
        return
    _LOADED_LOGGED = True
    ok = np is not None and shapely is not None
    _log(f"Wave Overhangs v{PLUGIN_VERSION} loaded",
         f"numpy/shapely : {'ok' if ok else 'MISSING'}",
         *([f"dependency problem: {_DEPS_ERROR}"] if _DEPS_ERROR else []))


@orca.plugin
class WaveOverhangsPlugin(orca.base):
    def register_capabilities(self):
        orca.register_capability(WaveOverhangsSlicing)
        orca.register_capability(WaveOverhangsCheck)
