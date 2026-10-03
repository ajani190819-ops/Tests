# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy>=2.0", "shapely>=2.0"]
#
# [tool.orcaslicer.plugin]
# name = "Wave Overhangs"
# description = "Experimental: print steep overhangs support-free by replacing the overhang region with wave-propagated toolpaths (port of the WaveOverhangs fork's algorithm as a slicing-pipeline plugin). | What's new in v0.0.50: Settings now come back by themselves after an update."
# author = "Wave Overhangs plugin lane"
# version = "0.0.50"
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
`tests/test_wave_gcode.py`. A fresh 0.0.27 export and a physical print are still
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
_WAVE_CORE_SRC = "\"\"\"Wave-overhang toolpath core -- pure geometry, no OrcaSlicer bindings.\n\nThis is the algorithm behind dennisklappe/OrcaSlicer-WaveOverhangs (itself a port\nof stmcculloch/PrusaSlicer-WaveOverhangs), reimplemented as a slicer-independent\nPython module so it can run inside an Orca slicing-pipeline plugin AND be unit\ntested offline with shapely.\n\nThe idea (see waveoverhangs.com \"How it works\"):\n\n  * For each layer, the *overhang region* is the part of the layer that sticks out\n    past the layer below -- it has nothing underneath it.\n  * A *seed* is taken at the supported edge (the boundary between the overhang and\n    the material below).\n  * Wavefronts are grown outward from the seed into the overhang: each front is\n    the set of points a fixed distance further from the supported edge than the\n    last. Because we grow by buffering the supported region, the fronts naturally\n    diffract around corners and holes, exactly like ripples on a pond.\n  * Each front becomes an extrusion polyline. A pattern mode decides how the\n    fronts are connected into a print order.\n\nEverything here is in millimetres, in the object's own XY frame. Mapping into the\nprinter's absolute G-code coordinates is the plugin's job (see the plugin module).\n\"\"\"\nfrom __future__ import annotations\n\nimport math\nfrom dataclasses import dataclass, field\n\nfrom shapely.geometry import (\n    GeometryCollection,\n    LineString,\n    MultiLineString,\n    MultiPolygon,\n    Point,\n    Polygon,\n)\nfrom shapely.ops import linemerge, unary_union\n\n# ---------------------------------------------------------------------------------\n# Configuration\n# ---------------------------------------------------------------------------------\n\n\n@dataclass\nclass WaveConfig:\n    \"\"\"Mirrors the fork's tunables (waveoverhangs.com \"~20 expert tunables\").\"\"\"\n\n    # Detection\n    overhang_tol: float = 0.05        # mm the layer below is grown before subtracting\n    min_overhang_area: float = 0.5    # mm^2, ignore slivers\n\n    # Wave field\n    line_spacing: float = 0.35        # mm, centreline spacing between wave tracks\n    line_width: float = 0.40          # mm, extrusion width of a wave line\n    max_iterations: int = 400         # safety cap on wavefronts per region\n    perimeter_overlap: float = 0.10   # mm, push the field toward the kept perimeter\n\n    # Pattern: \"monotonic\" | \"zigzag\" | \"smart\"\n    pattern: str = \"smart\"\n\n    # Motion / cooling / flow (used by the G-code emitter)\n    layer_height: float = 0.20\n    flow_ratio: float = 1.0\n    filament_diameter: float = 1.75\n    print_speed: float = 2.0          # mm/s\n    travel_speed: float = 120.0       # mm/s\n    fan: float = 1.0                  # 0..1, forced during wave extrusion\n\n    def mm3_per_mm(self) -> float:\n        return self.line_width * self.layer_height * self.flow_ratio\n\n    def e_per_mm(self) -> float:\n        area = math.pi * (self.filament_diameter / 2.0) ** 2\n        return self.mm3_per_mm() / area\n\n\n# ---------------------------------------------------------------------------------\n# Geometry helpers\n# ---------------------------------------------------------------------------------\n\n\ndef _iter_lines(geom):\n    \"\"\"Yield LineStrings from any shapely geometry (skip empties/points).\"\"\"\n    if geom is None or geom.is_empty:\n        return\n    if isinstance(geom, LineString):\n        yield geom\n    elif isinstance(geom, (MultiLineString, GeometryCollection)):\n        for g in geom.geoms:\n            yield from _iter_lines(g)\n    elif hasattr(geom, \"boundary\"):\n        yield from _iter_lines(geom.boundary)\n\n\ndef overhang_region(layer: Polygon, support: Polygon, cfg: WaveConfig):\n    \"\"\"The part of `layer` that overhangs open air (not over `support`).\n\n    `layer`   : this layer's sliced area.\n    `support` : the layer-below area (what this layer can rest on). Empty for the\n                first layer -> the whole layer is \"supported\" by the bed, so no\n                overhang.\n    \"\"\"\n    if support is None or support.is_empty:\n        # First layer / nothing below: treat as fully supported by the bed.\n        return Polygon()\n    grown = support.buffer(cfg.overhang_tol) if cfg.overhang_tol else support\n    ov = layer.difference(grown)\n    if ov.is_empty:\n        return ov\n    # Drop slivers below the area threshold.\n    keep = [p for p in _polys(ov) if p.area >= cfg.min_overhang_area]\n    return unary_union(keep) if keep else Polygon()\n\n\ndef _polys(geom):\n    if geom.is_empty:\n        return []\n    if isinstance(geom, Polygon):\n        return [geom]\n    if isinstance(geom, MultiPolygon):\n        return list(geom.geoms)\n    if isinstance(geom, GeometryCollection):\n        out = []\n        for g in geom.geoms:\n            out.extend(_polys(g))\n        return out\n    return []\n\n\n# ---------------------------------------------------------------------------------\n# Wavefront propagation\n# ---------------------------------------------------------------------------------\n\n\n@dataclass\nclass WaveTrack:\n    distance: float               # mm from the supported edge (front index * spacing)\n    points: list                  # [(x, y), ...] centreline polyline\n\n\ndef _heal_wake(reachable, domain, blend):\n    \"\"\"Round off the crease left where a split wavefront rejoins.\n\n    When the field flows around a hole, the two arriving sides meet behind it\n    in a sharp V, and every later front inherits that same kink -- a hard\n    seam running downstream of the hole. Real ripples do not keep it: the\n    crease heals as they travel on.\n\n    A morphological closing fills concave notches narrower than ``blend``\n    while never removing area that has already been reached, so the V gets\n    rounded a little more on each successive front. Re-clipping to the domain\n    keeps the result out of the hole and inside the part. Any GEOS failure\n    leaves the region exactly as it was.\n    \"\"\"\n    if blend <= 0.0 or reachable.is_empty:\n        return reachable\n    # The crease only exists once the field has flowed around an obstacle and\n    # closed up behind it, which is exactly when the reached region gains an\n    # interior ring (the obstacle itself). Before that there is nothing to\n    # heal, and skipping the two buffer calls keeps the common case cheap.\n    if not any(poly.interiors for poly in _polys(reachable)):\n        return reachable\n    try:\n        healed = reachable.buffer(blend, join_style=1).buffer(\n            -blend, join_style=1)\n        if healed.is_empty or not healed.is_valid:\n            return reachable\n        healed = healed.intersection(domain)\n        if healed.is_empty or not healed.is_valid:\n            return reachable\n        # Closing is extensive, so the clipped result can only match or\n        # exceed what was already reached. If it does not, something went\n        # wrong and the untouched region is the safe answer.\n        if healed.area + 1e-9 < reachable.area:\n            return reachable\n        return healed\n    except Exception:\n        return reachable\n\n\ndef wave_tracks(support: Polygon, overhang: Polygon, cfg: WaveConfig):\n    \"\"\"Grow wavefronts from the supported edge across the overhang.\n\n    Returns a list of WaveTrack ordered near->far from support. Each track is the\n    portion of an offset of the supported boundary that lies inside the overhang.\n    \"\"\"\n    tracks: list[WaveTrack] = []\n    if overhang is None or overhang.is_empty or support is None or support.is_empty:\n        return tracks\n\n    # Let the first front sit half a spacing into the overhang, then step outward.\n    # perimeter_overlap nudges the whole field back toward the kept perimeter/support\n    # so the last front hugs the supported edge on the far side.\n    base = 0.5 * cfg.line_spacing - cfg.perimeter_overlap\n    target = overhang.buffer(1e-6)\n    mode = str(getattr(cfg, \"propagation_mode\", \"auto\") or \"auto\").lower()\n    if mode not in (\"auto\", \"obstacle\", \"legacy\"):\n        mode = \"auto\"\n    has_internal_void = any(poly.interiors for poly in _polys(overhang))\n    obstacle_aware = mode == \"obstacle\" or (mode == \"auto\" and has_internal_void)\n    domain = support.union(target) if obstacle_aware else None\n    # How far a rejoining wavefront is allowed to heal its own crease, as a\n    # multiple of the line spacing. 0 reproduces the hard seam exactly.\n    blend = max(0.0, float(getattr(cfg, \"wake_blend\", 1.0))) * cfg.line_spacing\n    reachable = support\n    shown = support\n    for i in range(cfg.max_iterations):\n        d = base + i * cfg.line_spacing\n        if d <= 0:\n            continue\n        if not obstacle_aware:\n            # Preserve the established fast path for ordinary overhangs.\n            grown = support.buffer(d)\n            front = grown.boundary.intersection(target)\n            made_any = False\n            for ln in _iter_lines(front):\n                if ln.length <= 1e-6:\n                    continue\n                tracks.append(WaveTrack(distance=d, points=list(ln.coords)))\n                made_any = True\n            if grown.contains(target):\n                break\n            if not made_any and d > 1e-6 and grown.covers(target):\n                break\n            continue\n\n        # An internal hole is an obstacle, not merely a clipped part of the\n        # target. Grow the already-reachable region through the real domain so\n        # the front stops at the hole and advances around both sides.\n        previous = reachable\n        if i == 0:\n            reachable = support.buffer(d).intersection(domain)\n        else:\n            reachable = reachable.buffer(cfg.line_spacing).intersection(domain)\n        # Heal the front we are about to draw, but keep propagating -- and\n        # keep measuring \"already reached\" against -- the raw region. Two\n        # reasons: feeding the healed region back in compounds the growth,\n        # and healing snaps the notch to the same place on consecutive\n        # steps, so consecutive healed boundaries partly coincide. Measuring\n        # the subtraction below against the healed region would then delete\n        # those coincident stretches and cut the fronts into dashes.\n        shown = _heal_wake(reachable, domain, blend)\n        front = shown.boundary.intersection(target)\n        if i > 0:\n            # Do not emit a domain or hole boundary again after it has\n            # already been reached. Only the newly advanced edge is a front.\n            front = front.difference(previous.buffer(1e-5))\n        # Join pieces that meet at a wall or hole boundary before cleanup.\n        # This removes artificial saw-tooth gaps between adjacent front pieces.\n        # Clipping a boundary against another region can leave single-point\n        # and zero-length crumbs, and linemerge raises GEOSException on those\n        # (\"point array must contain 0 or >1 elements\"), which would lose the\n        # whole layer. Drop the crumbs before merging.\n        try:\n            front = linemerge(front)\n        except Exception:\n            # Clipping a boundary against another region can leave\n            # single-point and zero-length crumbs, and linemerge raises\n            # GEOSException on those (\"point array must contain 0 or >1\n            # elements\"), which would otherwise lose the whole layer. Retry\n            # without the crumbs; this path only runs when the normal merge\n            # has already failed, so ordinary fronts are untouched.\n            pieces = [ln for ln in _iter_lines(front)\n                      if len(ln.coords) > 1 and ln.length > 1e-9]\n            if not pieces:\n                front = GeometryCollection()\n            elif len(pieces) == 1:\n                front = pieces[0]\n            else:\n                try:\n                    front = linemerge(pieces)\n                except Exception:\n                    front = MultiLineString(pieces)\n        made_any = False\n        for ln in _iter_lines(front):\n            if ln.length <= max(1e-6, cfg.line_spacing * 0.2):\n                continue\n            tracks.append(WaveTrack(distance=d, points=list(ln.coords)))\n            made_any = True\n        if reachable.covers(target) or shown.covers(target):\n            break\n        if not made_any and reachable.equals(previous):\n            break\n    return tracks\n\n\n# ---------------------------------------------------------------------------------\n# Pattern / ordering\n# ---------------------------------------------------------------------------------\n\n\ndef _endpoints(pts):\n    return pts[0], pts[-1]\n\n\ndef _dist(a, b):\n    return math.hypot(a[0] - b[0], a[1] - b[1])\n\n\ndef order_tracks(tracks, support: Polygon, cfg: WaveConfig):\n    \"\"\"Turn wavefronts into an ordered list of printable polylines.\n\n    monotonic : print near->far, each front as its own line (lots of travels).\n    zigzag    : same order, but flip alternate fronts so the end of one is near\n                the start of the next -> connected back-and-forth motion.\n    smart     : like monotonic, but each front starts from its better-supported\n                (nearer-to-support) end so no line begins in thin air.\n    \"\"\"\n    ordered = sorted(tracks, key=lambda t: t.distance)\n    polylines = []\n    mode = (cfg.pattern or \"smart\").lower()\n\n    if mode == \"zigzag\":\n        flip = False\n        for t in ordered:\n            pts = list(reversed(t.points)) if flip else t.points\n            polylines.append(pts)\n            flip = not flip\n        return polylines\n\n    if mode == \"smart\" and support is not None and not support.is_empty:\n        for t in ordered:\n            a, b = _endpoints(t.points)\n            # Start from whichever end is closer to the supported region.\n            da = support.distance(_pt(a))\n            db = support.distance(_pt(b))\n            polylines.append(t.points if da <= db else list(reversed(t.points)))\n        return polylines\n\n    # monotonic (and fallback)\n    return [t.points for t in ordered]\n\n\ndef _pt(xy):\n    return Point(xy[0], xy[1])\n\n\n# ---------------------------------------------------------------------------------\n# G-code emission\n# ---------------------------------------------------------------------------------\n\n\ndef emit_layer_gcode(polylines, z, cfg: WaveConfig, restore_fan=None):\n    \"\"\"Emit G-code lines for one layer's wave polylines.\n\n    Coordinates are absolute bed XY; `z` is the layer height. `restore_fan` is an\n    optional 0..255 value to reset the fan to after the wave block (None = leave\n    the forced wave fan in place; the plugin usually passes the layer's fan back).\n    Relative-E is used inside the block and reset with M83/G92 so it composes with\n    Orca's own extrusion accounting.\n    \"\"\"\n    if not polylines:\n        return []\n    e_per_mm = cfg.e_per_mm()\n    print_f = int(round(cfg.print_speed * 60))\n    travel_f = int(round(cfg.travel_speed * 60))\n    out = [\"; ==== WAVE OVERHANG BEGIN ====\",\n           \"M83\",                                   # relative extrusion for our block\n           f\"M106 S{int(round(max(0.0, min(1.0, cfg.fan)) * 255))}\"]\n    for pts in polylines:\n        if len(pts) < 2:\n            continue\n        x0, y0 = pts[0]\n        out.append(f\"G0 F{travel_f} X{x0:.3f} Y{y0:.3f} Z{z:.3f}\")\n        out.append(f\"G1 F{print_f}\")\n        px, py = x0, y0\n        for (x, y) in pts[1:]:\n            seg = math.hypot(x - px, y - py)\n            if seg <= 1e-9:\n                continue\n            out.append(f\"G1 X{x:.3f} Y{y:.3f} E{seg * e_per_mm:.5f}\")\n            px, py = x, y\n    if restore_fan is not None:\n        out.append(f\"M106 S{int(restore_fan)}\")\n    out.append(\"; ==== WAVE OVERHANG END ====\")\n    return out\n\n\n# ---------------------------------------------------------------------------------\n# One-call convenience\n# ---------------------------------------------------------------------------------\n\n\n@dataclass\nclass LayerWaveResult:\n    z: float\n    polylines: list = field(default_factory=list)\n    overhang_area: float = 0.0\n    n_tracks: int = 0\n\n\ndef plan_layer(layer: Polygon, support: Polygon, z: float, cfg: WaveConfig):\n    \"\"\"Full per-layer plan: detect overhang, propagate waves, order them.\"\"\"\n    ov = overhang_region(layer, support, cfg)\n    if ov.is_empty:\n        return LayerWaveResult(z=z)\n    tracks = wave_tracks(support, ov, cfg)\n    polylines = order_tracks(tracks, support, cfg)\n    return LayerWaveResult(z=z, polylines=polylines,\n                           overhang_area=float(ov.area), n_tracks=len(tracks))\n\n\n# ---------------------------------------------------------------------------------\n# G-code layer parsing, self-calibration and splicing (pure text; unit tested)\n# ---------------------------------------------------------------------------------\n\nZ_KEYS = (\";Z:\", \";HEIGHT:\", \";LAYER_Z:\")\n\n\ndef parse_layer_z(line: str):\n    \"\"\"The layer height a G-code line announces, or None.\n\n    Handles Orca/Prusa comment markers (;Z: / ;HEIGHT: / ;LAYER_Z:) and a bare\n    layer-change move (`G1 Z.. F..` with no X/Y).\n    \"\"\"\n    s = line.strip()\n    for k in Z_KEYS:\n        if s.startswith(k):\n            try:\n                return float(s[len(k):].strip().split()[0])\n            except Exception:\n                return None\n    if s[:2] in (\"G0\", \"G1\") and \"Z\" in s and \" X\" not in (\" \" + s) and \" Y\" not in (\" \" + s):\n        for tok in s.split():\n            if tok.startswith(\"Z\"):\n                try:\n                    return float(tok[1:])\n                except Exception:\n                    return None\n    return None\n\n\ndef _extruding_xy(line: str):\n    \"\"\"(x, y) for an extruding G1 move (has X, Y and an E token), else None.\"\"\"\n    s = line.strip()\n    if not s.startswith(\"G1\"):\n        return None\n    x = y = None\n    has_e = False\n    for tok in s.split():\n        if tok.startswith(\"X\"):\n            try:\n                x = float(tok[1:])\n            except Exception:\n                return None\n        elif tok.startswith(\"Y\"):\n            try:\n                y = float(tok[1:])\n            except Exception:\n                return None\n        elif tok.startswith(\"E\"):\n            has_e = True\n    if x is not None and y is not None and has_e:\n        return (x, y)\n    return None\n\n\ndef layer_extrusion_min(lines, target_z, tol=1e-3):\n    \"\"\"Min (x, y) corner of extruding moves on the layer nearest `target_z`.\"\"\"\n    minx = miny = None\n    cur = None\n    for line in lines:\n        z = parse_layer_z(line)\n        if z is not None:\n            cur = z\n            continue\n        if cur is not None and abs(cur - target_z) <= tol:\n            xy = _extruding_xy(line)\n            if xy is not None:\n                minx = xy[0] if minx is None else min(minx, xy[0])\n                miny = xy[1] if miny is None else min(miny, xy[1])\n    return (minx, miny)\n\n\ndef _match_z(z, plans, tol=1e-3):\n    for pz in plans:\n        if abs(pz - z) <= tol:\n            return pz\n    return None\n\n\ndef splice_gcode(text, layer_plans, cfg: WaveConfig, calibration):\n    \"\"\"Insert wave moves into exported G-code. Pure text in / out.\n\n    layer_plans : {round(z,3): [polyline_in_object_frame, ...]}\n    calibration : (\"manual\", dx, dy)                     -> use this XY offset, or\n                  (\"auto\", calib_z, obj_min_x, obj_min_y) -> derive the offset by\n                    aligning Orca's own printed outline on layer `calib_z` to the\n                    object-frame outline min corner (a pure translation).\n\n    Wave moves for a layer are inserted just before the NEXT layer marker, i.e.\n    after Orca has printed that layer's own perimeters/infill.\n    Returns (new_text, inserted_layer_count, (dx, dy)).\n    \"\"\"\n    lines = text.splitlines(keepends=True)\n\n    if calibration and calibration[0] == \"manual\":\n        dx, dy = float(calibration[1]), float(calibration[2])\n    elif calibration and calibration[0] == \"auto\":\n        _, cz, omx, omy = calibration\n        gmin = layer_extrusion_min(lines, cz)\n        if gmin[0] is None or omx is None:\n            dx, dy = 0.0, 0.0\n        else:\n            dx, dy = gmin[0] - omx, gmin[1] - omy\n    else:\n        dx, dy = 0.0, 0.0\n\n    out = []\n    inserted = 0\n    pending = None  # (z, polylines) waiting to be flushed at the next layer marker\n\n    def flush():\n        nonlocal inserted\n        if pending is None:\n            return\n        z, polys = pending\n        shifted = [[(x + dx, y + dy) for (x, y) in pts] for pts in polys]\n        for ln in emit_layer_gcode(shifted, z, cfg):\n            out.append(ln + \"\\n\")\n        inserted += 1\n\n    for line in lines:\n        z = parse_layer_z(line)\n        if z is not None:\n            flush()\n            pending = None\n            key = _match_z(z, layer_plans)\n            if key is not None:\n                pending = (z, layer_plans[key])\n        out.append(line)\n    flush()\n\n    return \"\".join(out), inserted, (dx, dy)\n\n"
# Re-entrant: pressing Refresh in the Plugins dialog re-runs discovery and
# imports this module AGAIN in the same interpreter, so this block runs more
# than once per session.
#
# The new module MUST be published in sys.modules BEFORE it is exec'd:
# wave_core uses `from __future__ import annotations` with @dataclass, and
# dataclasses resolves those string annotations through
# `sys.modules[cls.__module__].__dict__`. Exec'ing into an unpublished module
# dies with "AttributeError: 'NoneType' object has no attribute '__dict__'".
#
# What changed in 0.0.34 is the failure path: it used to POP "wave_core",
# which on a second pass could leave a working engine replaced by nothing and
# the plugin reporting "engine MISSING". Now the previous module is put back.
_ENGINE_ERROR = None
_wc_prev = _sys.modules.get("wave_core")
wc = _types.ModuleType("wave_core")
_sys.modules["wave_core"] = wc
try:
    exec(compile(_WAVE_CORE_SRC, "wave_core (inlined)", "exec"), wc.__dict__)
except BaseException as _e:  # pragma: no cover - surfaced via Check setup
    # BaseException, not Exception: the audit hook may raise a non-Exception.
    _ENGINE_ERROR = f"{type(_e).__name__}: {_e}"
    if _wc_prev is not None and hasattr(_wc_prev, "plan_layer"):
        _sys.modules["wave_core"] = _wc_prev          # keep what already worked
        wc = _wc_prev
    else:
        _sys.modules.pop("wave_core", None)
        wc = None


# ---------------------------------------------------------------------------
#  The settings
#
#  Same three rules as Unlayered Infill, and for the same reason -- with 33
#  settings an unordered wall of keys is unusable:
#
#  1. ORGANISED. `_SECTIONS` is the running order and the grouping, and both
#     the Config panel and the "Check setup" guide are built from it.
#  2. ONE LINE each. A JSON editor renders "\n" as two literal characters, so
#     notes are short sentences shaped `values -- what it does`. The full
#     discussion lives in plugins/wave-overhangs/README.md.
#  3. "auto" wherever the export can answer the question -- see the AUTO_*
#     tables above. The factors reproduce the old constants exactly on a
#     stock 0.4 mm profile.
# ---------------------------------------------------------------------------
_DEFAULTS = {
    "enabled": True,
    "time_budget": "auto",          # auto: 30s + 45s per MB, capped at 300s

    "overhang_tol": 0.05,           # mm of support forgiveness
    "wave_internal_bridges": False,  # Internal Bridge sits on infill: leave it
    "straight_bridge_span": "auto",  # mm a plain bridge can cross unaided
    "min_overhang_area": "auto",    # auto: ~3 line widths squared
    "propagation_mode": "auto",     # "auto" | "obstacle" | "legacy"
    "wake_blend": 0.0,              # x line_spacing; EXPERIMENTAL, 0 = off
    "smooth_creases": True,         # round off the V kinks in a front

    "line_spacing": "auto",         # auto: 0.875 x the Wave line width
    "line_width": 0.40,             # mm fallback; the export's Bridge width wins
    "perimeter_overlap": "auto",    # auto: 0.25 x the Wave line width
    "max_iterations": "auto",       # auto: enough fronts to cross the region

    "pattern": "smart",             # "smart" | "monotonic" | "zigzag"
    "start_policy": "supported",    # supported/consistent/min|max-x/min|max-y
    "component_order": "support",   # "support" | "nearest"

    "min_wave_length": "auto",      # auto: 2.5 x the Wave line width
    "min_wave_segment": "auto",     # auto: 0.75 x the Wave line width
    "simplify_tolerance": "auto",   # auto: 4 x your profile's resolution
    "keep_uncovered_bridge": True,  # reprint bridge the waves did not cover
    "min_bridge_fragment": 0.5,     # x line width

    "contour_finish": False,        # EXPERIMENTAL, unverified: see CHANGELOG
    "wall_snap": True,
    "wall_last": True,              # print the overhanging wall AFTER the waves
    "wall_reach": "auto",           # auto: 1.5 line widths
    "wall_overlap": 0.25,           # fraction of line width
    "gap_fill": True,
    "gap_fill_min_area": 0.05,      # mm^2
    "edge_snap_distance": "auto",
    "edge_clearance": 0.0,          # mm

    "edge_taper_distance": "auto",  # auto: 1.5 x the Wave line width
    "edge_taper_min_flow": 0.55,    # fraction of normal flow at the wall
    "edge_taper_segment": 0.0,      # mm; 0 = no extra micro-moves
    "adaptive_flow": False,         # Arachne-style: widen a bead to fill a gap
    "adaptive_flow_max": 1.5,       # hard cap on that widening
    "flow_ratio": 1.0,

    "print_speed": "orca",          # follow the profile's bridge speed
    "travel_speed": "auto",         # auto: your profile's travel speed
    "fan": "auto",                  # auto: your profile's bridge fan (falls back to 100%)

    "auto_restore_settings": True,  # put settings back if an update wipes them
    "restore_backup": False,        # one-shot: put my remembered settings back
    "arc_fitting": False,           # false | "auto" | true
    "arc_tolerance": "auto",        # auto: your profile's resolution
}


# One line each. Shape: `accepted values -- what it does`.
_NOTES = {
    "_READ_ME": (
        "Keys starting with _ are notes and section headings; the plugin "
        "ignores them. \"auto\" means the value is taken from your own Orca "
        "profile or measured from the export -- run \"Wave Overhangs - "
        "Settings guide & check\" to see what each one resolved to."),

    "_enabled": (
        "true | false -- master switch; false leaves your G-code exactly as "
        "Orca wrote it."),
    "_time_budget": (
        "\"auto\", seconds, or 0 for no limit -- if the pass is still running "
        "when this runs out it gives up and returns Orca's file untouched. "
        "auto is 30s plus 45s per MB of G-code, capped at 300s."),

    "_overhang_tol": (
        "mm -- slack when deciding what is unsupported. Bigger is more "
        "forgiving, so fewer waves."),
    "_wave_internal_bridges": (
        "true | false -- Orca's \"Internal Bridge\" is the solid layer over "
        "sparse infill, anchored every few mm by the infill under it. A "
        "straight bridge is the right tool there, so it is left alone."),
    "_straight_bridge_span": (
        "\"auto\" or mm -- an unsupported patch that a plain bridge can cross "
        "is left to the plain bridge. auto is 10 mm. Raise it to wave less, "
        "lower it to wave more. 0 waves everything unsupported."),
    "_min_overhang_area": (
        "\"auto\" or mm2 -- ignore unsupported patches smaller than this. "
        "auto is about three line widths squared."),
    "_propagation_mode": (
        "auto | obstacle | legacy -- auto routes around holes only when the "
        "overhang has one. legacy is the old behaviour and can print across "
        "holes."),
    "_wake_blend": (
        "x line_spacing, 0 = off -- EXPERIMENTAL. Rounds the sharp V where "
        "the wave rejoins behind a hole; currently costs ~4% coverage."),

    "_smooth_creases": (
        "true | false -- rounds off the hard V kinks where a front folds "
        "back on itself. Those read as a chevron seam across the waves, "
        "worst on curved walls. Costs points: about twice as many on a "
        "crease-heavy layer."),
    "_line_spacing": (
        "\"auto\" or mm -- centre-to-centre gap between wave lines, the main "
        "quality/time dial. auto is 0.875 x the Wave line width."),
    "_line_width": (
        "mm -- only a fallback. If the export states a bridge width, that "
        "wins. Change this only if your export has no width information."),
    "_perimeter_overlap": (
        "\"auto\" or mm -- how far the first wave line starts back inside "
        "solid material so it is anchored. auto is a quarter line width."),
    "_max_iterations": (
        "\"auto\" or a count -- runaway guard on fronts per region. auto works "
        "out how many it takes to cross the region and adds headroom, so a "
        "big overhang is not cut off and a small one wastes nothing."),

    "_pattern": (
        "smart | monotonic | zigzag -- smart starts each line at its "
        "better-supported end; zigzag has fewer travels but more stringing."),
    "_start_policy": (
        "supported | consistent | min-x | max-x | min-y | max-y -- which end "
        "of a wave line to start from. supported is safest."),
    "_component_order": (
        "support | nearest -- when two wave areas are equally far along, "
        "print the one nearest the supported edge, or nearest the nozzle."),

    "_min_wave_length": (
        "\"auto\" or mm -- drop whole wave lines shorter than this as "
        "blob-prone. auto is 2.5 line widths."),
    "_min_wave_segment": (
        "\"auto\" or mm -- merge away stubs shorter than this at the ends of "
        "a line. auto is 0.75 of a line width."),
    "_simplify_tolerance": (
        "\"auto\" or mm -- how much jitter may be smoothed out of a wave "
        "line. auto scales with the line width, with your profile's "
        "Resolution as a floor."),
    "_keep_uncovered_bridge": (
        "true | false -- go back after the waves and print the bits of "
        "original bridge they did not cover. false skips those trips, "
        "leaving the bits unprinted."),
    "_min_bridge_fragment": (
        "x line width -- original bridge the waves did not cover is still "
        "printed if it is at least this long."),

    "_wall_last": (
        "true | false -- print the overhanging part of the wall AFTER the "
        "waves, not before. Orca's walls-first order lays that wall into open "
        "air with nothing under it; the waves give it something to land on."),
    "_contour_finish": (
        "true | false -- EXPERIMENTAL. Adds one pass along the far boundary "
        "after the fronts, so the waved area ends on a curved wall. Adds "
        "nothing where the fronts already reach it."),
    "_wall_snap": (
        "true | false -- stretch waves out to the real wall instead of "
        "stopping at the ragged edge of Orca's bridge lines. Leave on."),
    "_wall_reach": (
        "\"auto\" or mm -- how far that stretch may reach. auto is one and a "
        "half line widths."),
    "_wall_overlap": (
        "fraction of line width -- how far a wave end buries itself in the "
        "wall. Higher bonds better but can bulge."),
    "_gap_fill": (
        "true | false -- fill the slivers left where a wave runs out against "
        "an angled boundary, typically a corner tip."),
    "_gap_fill_min_area": (
        "mm2 -- leave slivers smaller than this alone rather than putting a "
        "tiny blob in them."),
    "_edge_snap_distance": (
        "\"auto\" or mm -- how far an endpoint may be nudged to land exactly "
        "on a wall or hole edge."),
    "_edge_clearance": (
        "mm -- hold back from walls. Normally 0; raising it leaves visible "
        "gaps at the edges."),

    "_edge_taper_distance": (
        "\"auto\" or mm, 0 = off -- distance over which flow eases off "
        "approaching a wall so ends do not blob. auto is 1.5 line widths."),
    "_edge_taper_min_flow": (
        "fraction -- the reduced flow right at the wall. 0.55 = 55%. Lower "
        "it if ends still look over-extruded."),
    "_edge_taper_segment": (
        "mm, 0 recommended -- 0 tapers using the moves that already exist; "
        "above 0 adds extra tiny moves and a bigger file."),
    "_adaptive_flow": (
        "true | false -- EXPERIMENTAL, Arachne-style: widen a front's FLOW to "
        "absorb the leftover strip beside it instead of leaving a sliver. "
        "Paths never move, only extrusion."),
    "_adaptive_flow_max": (
        "multiplier -- hard cap on adaptive_flow widening. 1.5 = a front may "
        "be asked for at most one and a half beads' worth of plastic."),
    "_flow_ratio": (
        "multiplier -- extrusion for wave lines only. Below 1 gives thinner, "
        "cooler lines that sag less."),

    "_print_speed": (
        "\"orca\" or mm/s -- orca follows your profile's bridge speed, read "
        "from the export section by section. The biggest factor in wave print "
        "time; drop to ~5 or ~2 if overhangs droop."),
    "_travel_speed": (
        "\"auto\" or mm/s -- auto follows your profile's travel speed. Only "
        "affects non-printing moves between waves."),
    "_fan": (
        "0 to 1, or \"auto\" -- cooling during wave printing; auto follows "
        "your profile's bridge fan. Full cooling is strongly recommended."),

    "_auto_restore_settings": (
        "true | false -- if an update leaves this panel at factory defaults, "
        "put back the values you had. Only after a version change, so the "
        "Config tab's own Restore defaults still works."),
    "_restore_backup": (
        "true | false -- a one-shot undo. Set true and slice once: every "
        "setting this plugin remembers from before is put back, and this "
        "returns to false. Check setup prints what is remembered."),
    "_arc_fitting": (
        "false | \"auto\" | true -- whether WAVE's own lines are written as "
        "G2/G3. Keep false (OrcaSlicer bug #7433). This is NOT Orca's own arc "
        "fitting, which lives in Print Settings > Quality > Precision and is "
        "unaffected either way."),
    "_arc_tolerance": (
        "\"auto\" or mm -- how far an arc may stray from the true path. auto "
        "follows your profile's resolution. Only used if arc_fitting is on."),
}


# Running order AND grouping for both the Config panel and the guide.
_SECTIONS = [
    ("_1_BASICS", "===== 1. BASICS =====",
     ["enabled", "time_budget"]),
    ("_2_DETECTION", "===== 2. WHAT COUNTS AS AN OVERHANG =====",
     ["overhang_tol", "wave_internal_bridges", "straight_bridge_span",
      "min_overhang_area", "propagation_mode", "wake_blend",
      "smooth_creases"]),
    ("_3_WAVE", "===== 3. THE WAVE ITSELF (mostly auto) =====",
     ["line_spacing", "line_width", "perimeter_overlap", "max_iterations"]),
    ("_4_ORDER", "===== 4. PRINT ORDER =====",
     ["pattern", "start_policy", "component_order"]),
    ("_5_CLEANUP", "===== 5. CLEANUP (mostly auto) =====",
     ["min_wave_length", "min_wave_segment", "simplify_tolerance",
      "keep_uncovered_bridge", "min_bridge_fragment"]),
    ("_6_WALLS", "===== 6. MEETING THE WALL =====",
     ["wall_last", "contour_finish", "wall_snap", "wall_reach",
      "wall_overlap", "gap_fill",
      "gap_fill_min_area", "edge_snap_distance", "edge_clearance"]),
    ("_7_ENDS", "===== 7. LINE ENDS AND FLOW =====",
     ["edge_taper_distance", "edge_taper_min_flow", "edge_taper_segment",
      "flow_ratio", "adaptive_flow", "adaptive_flow_max"]),
    ("_8_SPEED", "===== 8. SPEED AND COOLING (from your profile) =====",
     ["print_speed", "travel_speed", "fan"]),
    ("_9_ARCS", "===== 9. ARC MOVES =====",
     ["arc_fitting", "arc_tolerance"]),
    ("_10_RECOVERY", "===== 10. SETTINGS RECOVERY =====",
     ["auto_restore_settings", "restore_backup"]),
]


def settings_guide_lines(cfg=None, width=72):
    """The notes as readable text, for printing inside OrcaSlicer.

    Same order and same groups as the Config panel, because both are built
    from `_SECTIONS`. A JSON editor is an awkward place to read prose, and
    the owner should not have to open a README on GitHub to find out what a
    setting does. `cfg` is the live config, so the guide shows the value
    actually in force rather than the default.
    """
    live = cfg or _DEFAULTS
    out = ["--- what every setting means ---",
           "Your current value is shown first; (default X) follows when you",
           'have changed it. "auto" means Wave takes the number from your own',
           "Orca profile or measures it from the export; the report above",
           "says what each one resolved to on your last slice.",
           ""]

    def wrapped(note):
        # Wrap by hand: Orca shows this in a plain message box, so long
        # lines would be clipped rather than reflowed.
        line = "   "
        for word in note.split():
            if len(line) + len(word) + 1 > width:
                yield line
                line = "   "
            line += (" " if line.strip() else "") + word
        if line.strip():
            yield line

    for heading, title, keys in _SECTIONS:
        out.append(title)
        out.append("")
        for key in keys:
            note = _NOTES.get("_" + key)
            if not note:
                continue
            default = _DEFAULTS.get(key)
            value = live.get(key, default)
            head = f"{key} = {json.dumps(value)}"
            if value != default:
                head += f"   (default {json.dumps(default)})"
            out.append(head)
            out.extend(wrapped(note))
            out.append("")
    return out


def annotated_defaults():
    """`_DEFAULTS` as the user sees it in OrcaSlicer: grouped, note then key.

    Built fresh every time -- never hand out `_DEFAULTS` itself to be
    mutated -- and ordered by `_SECTIONS`, because 33 settings in dictionary
    order is not something a person can navigate.
    """
    out = {"_READ_ME": preset_safe(_NOTES["_READ_ME"])}
    placed = set()
    for heading, title, keys in _SECTIONS:
        out[heading] = preset_safe(title)
        for key in keys:
            if key not in _DEFAULTS:
                continue
            note = _NOTES.get("_" + key)
            if note:
                out["_" + key] = preset_safe(note)
            out[key] = _DEFAULTS[key]
            placed.add(key)
    leftover = [k for k in _DEFAULTS if k not in placed]
    if leftover:
        # Defensive: a setting added to _DEFAULTS and forgotten in _SECTIONS
        # must still be editable, not silently invisible.
        out["_99_OTHER"] = "===== OTHER ====="
        for key in leftover:
            note = _NOTES.get("_" + key)
            if note:
                out["_" + key] = preset_safe(note)
            out[key] = _DEFAULTS[key]
    return out


# The version this file was built as. Kept in lockstep with the PEP 723 header
# at the top (tests/test_installer.py fails if they drift), so everything that
# reports a version at runtime reports the one actually running.
PLUGIN_VERSION = "0.0.50"

# --- BEGIN changelog (generated by tools/sync_changelog.py) ---
CHANGELOG_RECENT = """\
v0.0.50  (2026-10-02)
   * Settings now come back by themselves after an update. If the Config
     panel reappears at factory defaults and this plugin remembers
     values you had set under an earlier version, they are put back on
     the next slice and the log says what was restored. New
     auto_restore_settings (true). Why it needed more than the manual
     switch added in 0.4.5/0.0.37. That switch worked, but only for
     someone who knew it existed -- which is no use when the symptom is
     "my settings are gone". The rule is deliberately narrow, so it can
     never fight the Config tab's own Restore defaults button:
   * the saved config must be pristine -- every value at this build's
     default, which is what a wipe looks like;
   * the newest remembered snapshot holding non-default values must come
     from a DIFFERENT build than the one running. Press Restore defaults
     without updating and the newest snapshot is from the running build,
     so nothing happens and the button means what it says. Update, and
     the snapshot is from the older build, so your values return. Tested
     in tests/test_plugin_runtime.py as all three cases: restore after a
     version change, Restore defaults sticking inside one version, and
     the switch turning it off. Also, on the curved perimeters. A clean
     export of that model finally arrived, and the answer is that the
     waves already reach the wall: | | | | --- | --- | | median
     distance, wall to nearest wave | 0.157 mm (= wall_overlap x line
     width) | | wall more than one line width away | 5% of its length |
     | those stretches that are actually overhang | 0 of 22 | Every
     place the waves fall short of that wall is somewhere the overhang
     does not reach -- Orca prints those itself. So contour_finish
     correctly finds nothing to add and stays off. One real bug came out
     of looking: contour_finish could never have done anything, because
     it called linemerge() on the wall geometry and that raises outright
     when the walls merge to a single LineString, which the surrounding
     except then swallowed. Fixed, and it now does add a bead along a
     boundary the fronts never reached -- there is a test for that. The
     visual evidence predates the fixes. The waved export in
     test-prints/t2-curved-perimeters/ is Wave 0.0.39 output: before the
     point-density fix (0.0.40), before internal bridges stopped being
     waved (0.0.44), and before crease rounding (0.0.45), which took the
     90th-percentile turn from 90 degrees to 22. Worth re-slicing that
     part before chasing it further.

v0.0.49  (2026-10-02)
   * New, EXPERIMENTAL and off by default: contour_finish. Adds one pass
     along the far boundary after the fronts, half a line width inside
     it, so the waved area ends ON a curved wall instead of wherever the
     outermost front happened to be pointing. Only the stretches no
     front already covers are added. Why it is off. The owner reported
     that on rounded perimeters the waves "curve back inwards into area
     that is already printed instead of following the contour". The
     diagnosis is sound in principle -- wavefronts are contours of
     distance from the SUPPORTED edge, and near a curved wall that is
     not the same shape as the wall, so the last front is not parallel
     to it. But it could not be reproduced on any export available here.
     On t3: | | | | --- | --- | | wave ends within 0.3 mm of the wall |
     99% | | median end distance | 0.157 mm (= wall_overlap x line
     width) | | median gap from wall to wave material | 0.000 mm | |
     paths contour_finish finds to add | 0 | So on that part the fronts
     already reach the wall and the pass is a no-op. Turning it on by
     default would be shipping a change whose benefit cannot be
     demonstrated, so it ships as a switch to try on the part that
     actually shows the problem. The curved-perimeter export in
     test-prints/t2-curved-perimeters/ is Wave 0.0.39 output, so it
     cannot be re-run: the original bridge moves are already gone. A
     clean export of that model -- same part, plugin switched off -- is
     what is needed to finish this.

v0.0.48  (2026-10-02)
   * A relocated overhang wall now takes its travel-in, unretract,
     retract and WIPE block with it, instead of leaving them stranded at
     the old position. A wall run that cannot take that block with it is
     no longer relocated at all. The owner asked whether 0.0.47 really
     dealt with what was in the tail of the waved layer. It had not.
     Relocating only the EXTRUDING moves left each wall's plumbing
     behind, and on t3 six of them ended up chained together: `` G0
     F7200 X100.440 Y91.979 G1 E-1.75 F1800 <- retract ;WIPE_START ...
     ;WIPE_END G1 X119.932 Y120.252 F7200 G1 E1.75 F3600 <- unretract ;
     wave-overhangs moved this overhanging wall after the waves `
     Travel, retract, wipe, travel, unretract, repeat -- with nothing
     printed. That is the "goes back through the layer stopping at
     random points", and it survived the previous two attempts because
     both were measuring travel distance, which this barely changes,
     rather than reading the output. Counting retract/wipe cycles that
     print nothing, on t3: | | cycles | | --- | --- | | unprocessed
     export (Orca's own) | 32 | | waves, no wall relocation | 37 | |
     0.0.47 | 41 | | 0.0.48 | 38 | Absorbing the plumbing is only safe
     when the extrusion inside it nets to zero -- an unretract matched
     by its retract. Where it does not, the span is refused and the run
     stays where it is, because taking half of a retract pair would
     shift every E value after it. Total extrusion is identical with
     relocation on or off, and there is a test for that. Still
     outstanding, and measured rather than guessed: 5 of those cycles
     come from the wave replacement itself, not the wall -- removing a
     covered bridge move can leave the wipe that belonged to it.
     wall_last: false` takes the count to 37, which isolates the two.
"""
# --- END changelog ---

# Stamped into the exported G-code, so the file itself says which build made
# the waves -- no need to open OrcaSlicer to find out.
WAVE_STAMP_PREFIX = "; wave-overhangs"
WAVE_STAMP = f"{WAVE_STAMP_PREFIX} v{PLUGIN_VERSION} (wave overhang toolpaths)\n"

# The active implementation is intentionally G-code-only. These state helpers
# record diagnostics for Check setup; they do not carry geometry between steps.

def _plugin_storage_dir():
    """No-prompt plugin storage folder, with a local fallback for tests."""
    try:
        storage = orca.host.plugin.storage()
        if storage:
            return storage
    except BaseException:
        pass
    return os.path.dirname(os.path.abspath(__file__))


def _state_path():
    return os.path.join(_plugin_storage_dir(), "wave_overhangs_state.json")


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


# --- keeping the Config panel up to date across upgrades -------------------
#
# Orca stores each capability's settings globally in
# `data_dir()/orca_plugins/config.json`, keyed by capability identity (see
# docs/ORCA-PLUGIN-FACTS.md, "Capability configuration"). `get_default_config()`
# is consulted only when nothing is saved or the user presses "Restore
# defaults", so a config written by an older release keeps that release's
# settings forever and everything added since is invisible in the panel.
#
# The wiki documents the cure: `get_config_version()` to spot an older schema,
# `save_config()` to write a migrated one back, from the
# `migrate_config_if_needed()` hook. The user's own values are always kept;
# only missing keys are added and the notes are refreshed.
_MIGRATED = set()


# ---------------------------------------------------------------------------
#  Keep the config safe to store IN A PRESET
#
#  There are two places this configuration can live. The global store
#  (data_dir()/orca_plugins/config.json) is a real JSON file and tolerates
#  anything. A PRESET override is not: a preset is a flat key=value record,
#  and `;` is its reference separator -- that is already why a capability
#  name may not contain one (docs/ORCA-PLUGIN-FACTS.md, "Misc"). A note
#  containing a semicolon, a double quote or a newline can therefore come
#  back out of a preset mangled, and OrcaSlicer reports exactly what it then
#  sees: "The preset stores invalid plugin capability configuration JSON."
#
#  So every string this plugin writes is put through here first, and the
#  JSON is written on ONE line. Nothing is left to the preset serializer's
#  tolerance.
# ---------------------------------------------------------------------------
_PRESET_UNSAFE = {
    ";": ",",      # preset reference separator
    '"': "'",      # quoting inside a quoted preset value
    "\n": " ",
    "\r": " ",
    "\t": " ",
}


def preset_safe(text):
    """A note string that cannot corrupt a preset that stores it."""
    if not isinstance(text, str):
        return text
    for bad, good in _PRESET_UNSAFE.items():
        text = text.replace(bad, good)
    # Control characters have no business in a settings note either.
    return "".join(ch for ch in text if ch >= " " or ch == " ")


def dump_config(cfg):
    """The exact text written back to Orca: one line, preset-safe."""
    return json.dumps({k: (preset_safe(v) if isinstance(v, str) else v)
                       for k, v in cfg.items()}, separators=(", ", ": "))


def merge_for_panel(saved, template):
    """`template` (the annotated defaults) with every value the user set kept.

    Settings the user changed survive. Settings this build added appear at
    their default. Notes come from the running build. Keys we no longer
    recognise are kept rather than discarded.
    """
    out = {}
    for key, value in template.items():
        if not key.startswith("_") and key in saved:
            out[key] = saved[key]
        else:
            out[key] = value
    for key, value in saved.items():
        if key not in out and not key.startswith("_"):
            out[key] = value
    return out


def _migrate_config(cap, template, name):
    """Add this build's new settings to an older saved config, in place.

    Returns the merged dict, or None when nothing was saved (the good case:
    Orca already shows `get_default_config()`). Never raises -- a settings
    panel that cannot be refreshed must not be able to stop a slice.
    """
    try:
        saved = json.loads(cap.get_config() or "{}")
    except BaseException:
        return None
    if not isinstance(saved, dict) or not saved:
        return None
    merged = merge_for_panel(saved, template)
    # Back up here as well as on every run. This hook is the config
    # lifecycle: it sees a value the owner typed into the Config panel even
    # if they never slice afterwards, which a run-time-only backup would
    # miss entirely. (_backup_settings keeps only keys this build has, so
    # the Check capability's own tiny config snapshots to nothing.)
    _backup_settings(merged, name)
    # An explicit, one-shot "put my settings back". Deliberately not
    # automatic -- see the comment on _backup_settings.
    restored = []
    if _as_bool_cfg(merged.get("restore_backup", False)):
        restored = _apply_backup(merged)
    else:
        restored = _auto_restore(merged, name)
        if restored:
            _log("Wave Overhangs v{v}: your settings were missing after an "
                 "update, so {n} of them were restored from the copy this "
                 "plugin keeps: {k}".format(
                     v=PLUGIN_VERSION, n=len(restored),
                     k=", ".join(sorted(restored))))
    if merged == saved:
        return merged
    try:
        ok = cap.save_config(dump_config(merged))
    except BaseException:
        return merged
    if restored:
        backup = _settings_backup() or {}
        _log(f"Wave Overhangs v{PLUGIN_VERSION}: restored {len(restored)} "
             f"setting(s) from the backup taken by "
             f"v{backup.get('version', '?')} on {backup.get('saved_at', '?')}: "
             + ", ".join(sorted(restored)))
    added = [k for k in merged if k not in saved and not k.startswith("_")]
    _log(f"Wave Overhangs v{PLUGIN_VERSION}: config for {name!r} was saved by "
         f"an older build; "
         + (f"added {', '.join(added)}" if added else "refreshed its notes")
         + (" and saved it" if ok else " but Orca refused to save it"))
    return merged


def _migrate_once(cap, template, name):
    """Migrate at most once per session, so a per-step call stays cheap.

    The BACKUP is not rate-limited with it: migration only has to happen
    once, but a value the owner typed five minutes later still has to be
    remembered, so the snapshot is taken on every call.
    """
    if name in _MIGRATED:
        try:
            saved = json.loads(cap.get_config() or "{}")
            if isinstance(saved, dict) and saved:
                _backup_settings(saved, name)
        except BaseException:
            pass
        return
    _MIGRATED.add(name)
    _migrate_config(cap, template, name)



# ---------------------------------------------------------------------------
#  Never lose the owner's settings
#
#  OrcaSlicer keeps a capability's settings in one global file. That copy can
#  go away for reasons the plugin does not control -- "Restore defaults", a
#  reinstall, a profile or data-directory change, an Orca upgrade. When it
#  does, the panel comes back as factory defaults and the tuning is gone.
#
#  So every time a capability runs, the current settings are copied into this
#  plugin's own state file. Nothing is restored automatically: silently
#  putting old settings back would make "Restore defaults" impossible, and a
#  plugin that overrules an explicit user action is worse than one that loses
#  a value. Instead the backup is always there, Check setup prints it, and
#  setting `restore_backup: true` and slicing once puts it back.
#
#  Settings that no longer exist in this build are dropped on the way in --
#  which is exactly the one case where a value genuinely cannot carry over.
# ---------------------------------------------------------------------------
# How many settings snapshots to keep. Five is enough to get
# behind an accidental wipe without the state file growing.
BACKUP_HISTORY = 5


def _looks_pristine(cfg):
    """True when every setting in `cfg` is this build's default."""
    return all(cfg.get(k) == v for k, v in _DEFAULTS.items())


def _auto_restore(merged, name):
    """Put settings back after an UPDATE wiped them. Returns what was restored.

    The owner's case: change settings in the Plugins menu, update the
    plugin, and the settings are gone. A manual `restore_backup` switch was
    not enough -- it only helps if you know it exists.

    The rule is narrow on purpose, so it can never fight the Config tab's
    own "Restore defaults" button:

      * the saved config must be PRISTINE -- every value at this build's
        default, which is what a wipe looks like;
      * the newest remembered snapshot holding non-default values must come
        from a DIFFERENT build than the one running now.

    Press "Restore defaults" without updating and the newest snapshot is
    from the running build, so nothing happens and the button means what it
    says. Update, and the snapshot is from the older build, so the settings
    come back.
    """
    if not _as_bool_cfg(merged.get("auto_restore_settings", True)):
        return []
    if not _looks_pristine(merged):
        return []
    backup = _settings_backup()
    if not backup:
        return []
    if str(backup.get("version") or "") == str(PLUGIN_VERSION):
        return []
    return _apply_backup(merged)



def _as_bool_cfg(value):
    """A JSON editor can hand back the string "false"; treat it as false."""
    if isinstance(value, str):
        return value.strip().lower() not in ("", "0", "false", "no", "off")
    return bool(value)


def _backup_settings(cfg, name):
    """Remember the values in force, so they can survive the config going."""
    try:
        values = {k: v for k, v in cfg.items()
                  if not k.startswith("_") and k in _DEFAULTS
                  and k != "restore_backup"}
        if not values:
            return
        state = _load_state()
        history = state.get("settings_backups")
        if not isinstance(history, list):
            history = []
        if history and history[0].get("values") == values:
            return                       # unchanged; skip the write
        history.insert(0, {
            "version": PLUGIN_VERSION,
            "saved_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "capability": name,
            "values": values,
        })
        # A HISTORY, not a single slot. The event we are insuring against --
        # the config being wiped -- is itself followed by a run, which would
        # otherwise overwrite the only copy with the factory defaults that
        # just replaced the owner's settings. Keeping the last few snapshots
        # means the wipe cannot destroy the thing that undoes it.
        state["settings_backups"] = history[:BACKUP_HISTORY]
        _save_state(state)
    except BaseException:
        pass                             # a backup must never break a slice


def _settings_backup():
    """The most recent remembered settings worth restoring, or None.

    "Worth restoring" means the newest snapshot that holds at least one
    non-default value. A snapshot of pure defaults is what a wipe looks
    like, and restoring that would achieve nothing.
    """
    try:
        history = _load_state().get("settings_backups")
        if not isinstance(history, list):
            return None
        usable = [b for b in history
                  if isinstance(b, dict) and isinstance(b.get("values"), dict)]
        for backup in usable:
            if any(k in _DEFAULTS and _DEFAULTS[k] != v
                   for k, v in backup["values"].items()):
                return backup
        return usable[0] if usable else None
    except BaseException:
        pass
    return None


def _apply_backup(merged):
    """Put the remembered settings back into `merged`, in place.

    Returns the list of settings restored. Anything the backup holds that
    this build no longer has is dropped: a removed setting is the one thing
    that genuinely cannot carry over.
    """
    backup = _settings_backup()
    if not backup:
        return []
    restored = []
    for key, value in backup["values"].items():
        if key not in _DEFAULTS or key == "restore_backup":
            continue
        if merged.get(key) != value:
            merged[key] = value
            restored.append(key)
    merged["restore_backup"] = False     # one-shot: never loop on next slice
    return restored


def _cfg(self):
    try:
        src = json.loads(self.get_config() or "{}")
    except (AttributeError, TypeError, ValueError):
        src = {}
    cfg = dict(_DEFAULTS)
    for k, v in src.items():
        if k in cfg:
            cfg[k] = v
    # Insurance: keep our own copy of whatever is in force right now.
    _backup_settings(cfg, getattr(self, "_backup_name", "settings"))
    return cfg


def _wave_config(cfg, layer_height):
    """A WaveConfig from the raw config.

    Every length here may legitimately be the string "auto": the factors it
    stands for are multiples of the WAVE LINE WIDTH, which is measured from
    the export per section and is not known yet. So this builds a config with
    safe placeholders and `_resolve_autos()` overwrites them a moment later,
    once the real width is in hand. Nothing downstream ever sees "auto".
    """
    return wc.WaveConfig(
        overhang_tol=_float_cfg(cfg, "overhang_tol", 0.05),
        min_overhang_area=_float_cfg(cfg, "min_overhang_area", 0.5),
        line_spacing=_float_cfg(cfg, "line_spacing", 0.35),
        line_width=_float_cfg(cfg, "line_width", 0.40),
        perimeter_overlap=_float_cfg(cfg, "perimeter_overlap", 0.10),
        pattern=str(cfg["pattern"]),
        layer_height=float(layer_height),
        flow_ratio=_float_cfg(cfg, "flow_ratio", 1.0),
        print_speed=_print_speed_fallback(cfg["print_speed"]),
        travel_speed=_float_cfg(cfg, "travel_speed", AUTO_TRAVEL_FALLBACK),
        fan=_float_cfg(cfg, "fan", 1.0),
        max_iterations=int(_float_cfg(cfg, "max_iterations", 400)),
    )


_ORCA_SPEED_WORDS = ("orca", "bridge", "auto")

# Wave print speed of 2 mm/s is the safe default, and it is also the single
# biggest cost in a Wave print. Setting print_speed to "orca" instead reuses
# the feedrate Orca already put on the bridge moves being replaced, which is
# read back out of the exported G-code rather than guessed.


def _wants_orca_print_speed(value):
    """True when print_speed asks to follow Orca's own bridge speed."""
    return (isinstance(value, str)
            and value.strip().lower() in _ORCA_SPEED_WORDS)


def _print_speed_fallback(value, default=2.0):
    """The numeric print speed, in mm/s.

    When print_speed is "orca" there is no number yet -- the real value comes
    from each bridge section. This returns the conservative default so the
    plugin still has something safe to fall back on if a section turns out to
    carry no feedrate at all.
    """
    if _wants_orca_print_speed(value):
        return float(default)
    try:
        speed = float(value)
    except (TypeError, ValueError):
        return float(default)
    return speed if speed > 0 else float(default)


def _section_print_speed(sec, wave_speed=None):
    """The feedrate Orca itself used for this bridge section, in mm/s.

    Takes the most common feedrate across the section's moves rather than the
    first, so one odd move cannot speak for the section. Any move already
    sitting at the Wave speed is ignored: a file produced by a plugin version
    older than 0.0.28 can carry the leaked Wave feedrate on untouched bridge
    moves, and reading that back would pin the speed at 2 mm/s for ever.
    """
    counts = {}
    for seg in sec.get("segments", ()):
        feed = seg.get("feed")
        if not feed or feed <= 0:
            continue
        if wave_speed and abs(feed - wave_speed * 60.0) < 1e-6:
            continue
        counts[feed] = counts.get(feed, 0) + 1
    if not counts:
        return None
    # Ties go to the faster feedrate, which is the one Orca normally states
    # at the start of a bridge section.
    best = max(counts.items(), key=lambda kv: (kv[1], kv[0]))
    return best[0] / 60.0


# The old slice-object planning path was removed. Orca's exported G-code is the
# source of truth for both support and bridge geometry; keeping a second object
# frame would reintroduce the alignment and cross-callback failures this plugin
# is designed to avoid.

# ---------------------------------------------------------------------------------
# G-code-only replacement
# ---------------------------------------------------------------------------------

_GWORD = re.compile(r"([A-Z])(-?(?:\d+(?:\.\d*)?|\.\d+))")
_BRIDGE_TYPES = ("bridge", "internal bridge")
# Wall/perimeter sections. These are the real visible boundary of the part on
# this layer: the edge a Wave line should finish on. Orca exports the first
# three names; the PrusaSlicer-style names are accepted so a differently
# labelled export still finds its walls instead of silently finding none.
_WALL_TYPES = ("outer wall", "inner wall", "overhang wall",
               "external perimeter", "perimeter", "overhang perimeter")


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


_MOVE_CODE = re.compile(r"^G([0123])(?![0-9])")


def _arc_move_points(start, end, words, clockwise, step=0.2):
    """Expand one G2/G3 arc into points, in I/J or R form.

    Falls back to the straight chord if the command is malformed, which is
    the safe reading: a missing curve can only make Wave see less overhang,
    never more.
    """
    sx, sy = start
    ex, ey = end
    cx = cy = None
    if "I" in words or "J" in words:
        cx, cy = sx + words.get("I", 0.0), sy + words.get("J", 0.0)
    elif "R" in words:
        radius = words["R"]
        mx, my = (sx + ex) * 0.5, (sy + ey) * 0.5
        dx, dy = ex - sx, ey - sy
        chord = math.hypot(dx, dy)
        if chord < 1e-9 or abs(radius) < chord * 0.5:
            return [start, end]
        offset = math.sqrt(max(0.0, radius * radius - chord * chord * 0.25))
        # A positive R is the short way round, a negative R the long way.
        sign = 1.0 if ((not clockwise) == (radius > 0)) else -1.0
        cx = mx + sign * offset * (-dy / chord)
        cy = my + sign * offset * (dx / chord)
    if cx is None:
        return [start, end]
    radius = math.hypot(sx - cx, sy - cy)
    if radius < 1e-9:
        return [start, end]
    a0 = math.atan2(sy - cy, sx - cx)
    a1 = math.atan2(ey - cy, ex - cx)
    sweep = (a1 - a0) % (2.0 * math.pi)
    if clockwise:
        sweep -= 2.0 * math.pi
    if abs(sweep) < 1e-9:
        sweep = -2.0 * math.pi if clockwise else 2.0 * math.pi
    count = max(2, min(720, int(abs(sweep) * radius / max(0.02, step))))
    points = [(cx + radius * math.cos(a0 + sweep * k / count),
               cy + radius * math.sin(a0 + sweep * k / count))
              for k in range(count + 1)]
    points[0], points[-1] = start, end
    return points


def _bridge_layer_indices(lines):
    """Layer numbers that have an exported Bridge section, plus the one below.

    A plain text scan, no geometry. Only these layers need their toolpaths
    turned into shapely objects: a 134-layer export typically has three or
    four, so building geometry for all of them is about twenty times the work
    for nothing.
    """
    wanted = set()
    index = -1
    for line in lines:
        s = line.strip()
        if s.startswith(";Z:"):
            try:
                float(s[3:])
            except ValueError:
                continue
            index += 1
        elif s.startswith(";TYPE:") and index >= 0:
            if s[6:].strip().lower() in _BRIDGE_TYPES:
                wanted.add(index)
                if index > 0:
                    wanted.add(index - 1)   # the layer that holds it up
    return wanted


def _parse_gcode_geometry(lines, geometry_for=None):
    """Return layers and bridge sections using actual exported toolpaths.

    `geometry_for` limits which layer numbers get their moves turned into
    geometry. Extrusion bookkeeping is tracked for every line regardless, so
    the layers that are built come out exactly as they would have otherwise.
    """
    layers = []
    layer = None
    section = None
    wanted = True
    x = y = z = None
    width = 0.4
    relative_e = True
    e_position = 0.0
    fan = None
    feed = None
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith(";Z:"):
            try:
                layer = {"z": float(s[3:]), "all": [], "sections": [],
                         "walls": [], "marker": i}
                layers.append(layer)
                section = None
                wanted = (geometry_for is None
                          or (len(layers) - 1) in geometry_for)
            except ValueError:
                layer = None
            continue
        if s.startswith(";TYPE:"):
            name = s[6:].strip().lower()
            section = {"type": name, "segments": [], "marker": i, "fan": fan,
                       "feed": feed, "relative_e": relative_e,
                       "e_start": e_position}
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
        move = _MOVE_CODE.match(s)
        if move is None:
            continue
        code = int(move.group(1))
        words = _gwords(s)
        if "F" in words:
            # G-code feedrates are modal: the last F stays in force until
            # something changes it. Track it so replaced moves can be
            # re-emitted at their original speed instead of inheriting the
            # Wave print speed.
            feed = words["F"]
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
        if (layer is not None and wanted and extruding
                and None not in (x, y, nx, ny) and (x, y) != (nx, ny)):
            path = [(x, y), (nx, ny)]
            if code in (2, 3):
                # An arc move. With arc fitting switched on in the print
                # profile a curved wall arrives as G2/G3, and reading it as a
                # straight chord would hide the hole it goes around.
                path = _arc_move_points((x, y), (nx, ny), words, code == 2)
            seg = {"line": i, "a": (x, y), "b": (nx, ny), "z": nz,
                   "e": e_delta, "e_start": e_start, "e_end": e_position,
                   "relative_e": relative_e, "width": width, "feed": feed,
                   "geom": shapely.geometry.LineString(path)}
            layer["all"].append(seg)
            if section is not None and section["type"] in _BRIDGE_TYPES:
                section["segments"].append(seg)
            if section is not None and section["type"] in _WALL_TYPES:
                layer["walls"].append(seg)
        x, y, z = nx, ny, nz
    return layers


def _footprint(segments):
    """The plastic a set of moves lays down.

    Moves of the same width are buffered in one call rather than one call
    each: on a layer with twenty thousand moves that is the difference
    between one GEOS operation and twenty thousand of them.
    """
    if not segments:
        return shapely.geometry.Polygon()
    by_width = {}
    for seg in segments:
        by_width.setdefault(round(float(seg["width"]), 4), []).append(seg["geom"])
    polys = []
    for width, geoms in by_width.items():
        shape = (geoms[0] if len(geoms) == 1
                 else shapely.geometry.MultiLineString(geoms))
        polys.append(shape.buffer(max(0.0, width) * 0.5, cap_style=2,
                                  join_style=2))
    return polys[0] if len(polys) == 1 else shapely.ops.unary_union(polys)


def _polygon_parts(geom):
    """Every Polygon inside any shapely geometry (empty list if none)."""
    if geom is None or geom.is_empty:
        return []
    gt = getattr(geom, "geom_type", "")
    if gt == "Polygon":
        return [geom]
    if gt in ("MultiPolygon", "GeometryCollection"):
        out = []
        for part in geom.geoms:
            out.extend(_polygon_parts(part))
        return out
    return []


def _wall_material(layer):
    """The plastic this layer's wall/perimeter moves actually lay down.

    The moves are joined into loops before being given their width. Buffering
    each move on its own leaves a hairline slit at every vertex of a curved
    wall -- a circular hole is exported as dozens of short straight moves --
    and a Wave end can slip through one of those slits and finish on the
    visible surface of the hole.
    """
    segments = layer.get("walls") or []
    if not segments:
        return shapely.geometry.Polygon()
    by_width = {}
    for seg in segments:
        by_width.setdefault(round(float(seg["width"]), 3), []).append(seg["geom"])
    polys = []
    for width, lines in by_width.items():
        try:
            merged = shapely.ops.linemerge(lines) if len(lines) > 1 else lines[0]
        except (TypeError, ValueError):  # pragma: no cover - defensive
            merged = shapely.ops.unary_union(lines)
        polys.append(merged.buffer(max(0.01, width) * 0.5,
                                   cap_style=2, join_style=1))
    return shapely.ops.unary_union(polys) if polys else shapely.geometry.Polygon()


def _layer_outline(layer):
    """Outer silhouette of everything this layer prints.

    Used only as a hard safety net: nothing Wave generates may end up outside
    the part, whatever the wall geometry looks like.
    """
    material = _footprint(layer.get("all") or [])
    filled = [shapely.geometry.Polygon(p.exterior)
              for p in _polygon_parts(material)]
    return shapely.ops.unary_union(filled) if filled else material


def _wall_reach(cfg, swcfg):
    """How far the overhang region may be stretched to meet a wall."""
    raw = cfg.get("wall_reach", "auto")
    if isinstance(raw, str) and raw.strip().lower() == "auto":
        return max(swcfg.line_width * 1.5, swcfg.line_spacing * 2.0)
    return max(0.0, _float_cfg(cfg, "wall_reach", 0.0))


def _wall_overlap(cfg, swcfg):
    """How far a Wave end may reach into the wall bead, in millimetres."""
    fraction = max(0.0, min(0.9, _float_cfg(cfg, "wall_overlap", 0.25)))
    return fraction * swcfg.line_width


def _wall_limit(outline, wall_width, overlap):
    """Hard cap: no Wave may sit deeper than `overlap` inside the outer wall.

    A wall loop is not always a sealed band -- seams, wipes and short corner
    moves leave gaps -- so without this a corridor can leak around a wall end
    and put a Wave end on the visible outside skin of the part.
    """
    if outline is None or outline.is_empty or wall_width <= 0.0:
        return None
    inset = max(0.0, float(wall_width) - max(0.0, float(overlap)))
    if inset <= 1e-9:
        return None
    limit = outline.buffer(-inset)
    return None if limit.is_empty else limit


def _wall_bounded_region(bridge, walls, outline, wall_width, swcfg, cfg):
    """Replace a bridge footprint's battlement edge with the real wall edge.

    Orca exports bridge infill as individual lines, so the union of those
    line footprints has a castellated edge: alternating in and out by about
    half a line width, and usually stopping short of the perimeter. Clipping
    wavefronts to that edge is what makes Wave ends look frayed.

    This grows the footprint only inside the narrow corridor beside real wall
    material, so the wall-side boundary becomes the wall's own smooth edge
    (plus an optional overlap into the bead for bonding), while every boundary
    that is not next to a wall -- the supported side, and the sparse-infill
    side of an internal bridge -- is left exactly as it was.

    Returns (region, changed).
    """
    if not bool(cfg.get("wall_snap", True)):
        return bridge, False
    if bridge is None or bridge.is_empty or walls is None or walls.is_empty:
        return bridge, False
    reach = _wall_reach(cfg, swcfg)
    if reach <= 1e-9:
        return bridge, False
    overlap = _wall_overlap(cfg, swcfg)
    try:
        limit = _wall_limit(outline, wall_width, overlap)
        composite = shapely.ops.unary_union([bridge, walls])
        # Close the bridge footprint against the wall bead. A closing fills a
        # narrow channel and the notches between line ends -- exactly the gap
        # Orca leaves between its last bridge line and the wall -- while a
        # wide open space, such as the inside of a hole or an unbridged part
        # of the layer, is far too big to be closed and is left alone.
        closed = composite.buffer(reach).buffer(-reach)
        gap = closed.difference(composite).intersection(walls.buffer(reach))
        # Only a gap the bridge itself borders is this section's to fill.
        near = [p for p in _polygon_parts(gap) if p.intersects(bridge)]
        # Cut the footprint back to the wall's inner edge first: the tips of
        # the battlements can already poke into the bead, and leaving them
        # there would keep a jagged edge even after the gaps are filled.
        base = bridge.difference(walls)
        if near:
            base = shapely.ops.unary_union([base] + near)
        if limit is not None:
            base = base.intersection(limit)
        base = base.difference(walls).buffer(0)
        keep = [p for p in _polygon_parts(base)
                if p.intersection(bridge).area > 1e-9]
        if not keep:
            return bridge, False
        region = shapely.ops.unary_union(keep)
        if overlap > 1e-9:
            bonded = region.buffer(overlap, join_style=2).intersection(walls)
            if limit is not None:
                bonded = bonded.intersection(limit)
            if not bonded.is_empty:
                region = shapely.ops.unary_union([region, bonded])
        region = region.buffer(0)
    except Exception:
        # Geometry trouble must never cost the owner the whole Wave pass.
        return bridge, False
    if region.is_empty or region.area < bridge.area * 0.5:
        # A pathological wall reading should not shrink the field away.
        return bridge, False
    return region, True


def _backed_parts(region, core, share=0.2):
    """Keep only the parts of a region that the original Wave area sits in.

    Reaching for a wall can also reach past the end of one, and the support
    footprint is made of separate infill lines, so the gap between two of them
    can look unsupported. Neither is a place to print a Wave. Stretching the
    field to the perimeter is allowed to improve the shape of an overhang area
    Orca really exported; it is never allowed to invent a new one.
    """
    if region is None or region.is_empty or core is None or core.is_empty:
        return region
    keep = [p for p in _polygon_parts(region)
            if p.intersection(core).area >= max(1e-9, share * p.area)]
    if not keep:
        return shapely.geometry.Polygon()
    return shapely.ops.unary_union(keep)


# How far a simplified wave path may stray outside the region it was grown
# in, in millimetres. Straying is what puts plastic in a hole, so this is a
# hard safety allowance -- 0.02 mm is a twentieth of a 0.4 mm line.
#
# It is deliberately a FIXED constant. It used to be `tolerance * 0.4`, which
# was self-defeating: _simplify_attempts walks down a ladder of tolerances to
# find a gentler simplification, but scaling the margin with the tolerance
# tightened the guard by the same factor at every rung. A front hugging a
# hole therefore failed every rung and fell back to `list(original.coords)`
# -- every raster point shapely's buffer produced. On the owner's part that
# left 60% of all wave moves under 0.1 mm long, carrying 0.9% of the
# distance: thousands of G-code lines to draw a tiny chunk of curve.
_MAX_STRAY_MM = 0.02

# Two consecutive path points closer together than this are treated as one.
# A 0.02 mm move is a twentieth of a line width: the printer cannot resolve
# it, it costs a whole G-code line, and at 2 mm/s it also costs real time.
_MIN_POINT_GAP_MM = 0.02


def _clean_guards(allowed, tolerance=None):
    """Guard shapes for cleanup, built once and reused for every front.

    `allowed.buffer(...)` is expensive on a real overhang region, and the
    result only depends on the region -- not on which front is being cleaned,
    nor on the tolerance -- so building it per front was doing the same heavy
    operation a hundred times over.

    `tolerance` is accepted and ignored. It used to scale the margin, which
    was a mistake: see _MAX_STRAY_MM.
    """
    margin = _MAX_STRAY_MM
    guard = allowed.buffer(margin)
    voids = _interior_voids(allowed)
    body = voids.buffer(-margin) if not voids.is_empty else voids
    return guard, body


def _thin_points(points, gap):
    """Collapse consecutive points closer together than `gap`.

    Douglas-Peucker keeps a point whenever it lies far from the chord, so at
    a cusp -- which is exactly what a wavefront forms where it wraps around a
    hole -- it retains two points that may be only microns apart. Every such
    pair is a G-code move that prints nothing anyone can see. On the owner's
    part 60% of all wave moves were under 0.1 mm long and together carried
    0.9% of the distance printed.

    Collapsing a pair can never move the path further than `gap`, and callers
    re-check the result against the hole guard before accepting it.
    """
    if len(points) < 3:
        return list(points)
    out = [points[0]]
    for pt in points[1:-1]:
        if math.hypot(pt[0] - out[-1][0], pt[1] - out[-1][1]) >= gap:
            out.append(pt)
    out.append(points[-1])
    # The final point is never dropped, so thin the one before it instead
    # when the two have ended up within a gap of each other.
    while len(out) > 2 and math.hypot(
            out[-1][0] - out[-2][0], out[-1][1] - out[-2][1]) < gap:
        out.pop(-2)
    return out


def _thinned_fallback(original, tolerance, allowed, guards):
    """Thin a front that could not be simplified, without moving it.

    A front hugging a hole can fail every rung of the simplification ladder:
    any chord long enough to be worth taking cuts into the void. That used to
    mean keeping *every* raster point shapely's buffer produced -- thousands
    of G-code lines to draw a small piece of curve.

    Simplifying is not the only option. The points can still be thinned where
    they are piled on top of each other, which moves the path by less than
    the gap rather than cutting a corner. Each gap is tried against the same
    hole guard, largest first, and the untouched original is the last resort.
    """
    coords = list(original.coords)
    if len(coords) < 3:
        return coords
    guard = body = None
    if allowed is not None:
        if guards is not None and "guard" in guards:
            guard, body = guards["guard"]
        else:
            guard, body = _clean_guards(allowed)
    for gap in (tolerance, _MIN_POINT_GAP_MM):
        if gap <= 0:
            continue
        thinned = _thin_points(coords, gap)
        if len(thinned) >= len(coords):
            continue
        if guard is not None:
            candidate = shapely.geometry.LineString(thinned)
            if not guard.covers(candidate):
                continue
            if body is not None and not body.is_empty \
                    and candidate.intersects(body):
                continue
        return thinned
    return coords


def _locally_refined(original, tolerance, allowed, guards):
    """A valid-by-construction simplification, or None if it is no better.

    `_simplify_attempts` accepts or rejects a whole front at a time, so one
    tight corner forces the entire front to keep its raster points. This
    checks each chord on its own and splits only the ones that fail, which
    is what keeps a long gentle curve cheap while a squeeze past a hole
    stays accurate.
    """
    coords = list(original.coords)
    if len(coords) < 3:
        return None
    guard = body = None
    if allowed is not None:
        if guards is not None and "guard" in guards:
            guard, body = guards["guard"]
        else:
            guard, body = _clean_guards(allowed)

    def chord_ok(a, b):
        if guard is None:
            return True
        try:
            chord = shapely.geometry.LineString([a, b])
            if not guard.covers(chord):
                return False
            if body is not None and not body.is_empty \
                    and chord.intersects(body):
                return False
            return True
        except Exception:
            return False

    try:
        refined = _simplify_locally(coords, tolerance, chord_ok)
    except Exception:  # pragma: no cover - geometry never breaks an export
        return None
    if len(refined) < 2 or len(refined) >= len(coords):
        return None
    refined = _thin_points(refined, _MIN_POINT_GAP_MM)
    if len(refined) < 2:
        return None
    # Belt and braces: the per-chord checks should make this impossible,
    # but a front that escapes its region is the one failure that puts
    # plastic in a hole, so it is verified as a whole as well.
    if guard is not None:
        candidate = shapely.geometry.LineString(refined)
        if not guard.covers(candidate):
            return None
        if body is not None and not body.is_empty \
                and candidate.intersects(body):
            return None
    return refined



# How sharp a turn has to be before it is treated as a crease rather than
# curvature, and how far back from it the rounding reaches.
SHARP_TURN_DEGREES = 30.0
AUTO_ROUND_X_WIDTH = 0.6         # x line width
ROUND_PASSES = 3


def _smooth_and_thin(points, radius, chord_ok, tolerance):
    """Round the creases off a front.

    Thinning the result afterwards was tried and dropped: re-simplifying at
    a quarter of the tolerance took the 90th-percentile turn back from 22
    to 36 degrees and only saved 265 points of 3456. The points a chamfer
    adds are the roundness -- removing them removes the fix.
    """
    return _round_sharp_turns(points, radius, chord_ok)


def _round_sharp_turns(points, radius, chord_ok, limit_deg=SHARP_TURN_DEGREES,
                       passes=ROUND_PASSES):
    """Round off the hard V kinks in a front, leaving real curvature alone.

    A wavefront that flows around an obstacle rejoins behind it in a sharp
    V, and every later front inherits the kink -- a chevron seam running
    across the field. On a curved perimeter the effect is strongest,
    because the fronts are already turning: measured on the owner's t3
    export, the median turn at a vertex was 15 degrees but the 90th
    percentile was 90, which is not faceting, it is hairpins.

    Simplification cannot fix this and neither can a finer tolerance: the
    kink is really in the geometry. So it is chamfered -- the vertex is
    replaced by two points set back along each arm plus one pulled slightly
    into the turn, which is a one-segment approximation of a fillet. Each
    replacement is checked against the same region guard, so rounding can
    never push a front into a hole, and a turn under `limit_deg` is left
    exactly as it was.
    """
    if radius <= 0.0 or len(points) < 3:
        return points
    # One chamfer only halves a kink: a 104 degree hairpin becomes two 52
    # degree turns. Three passes at a shrinking setback turn it into a
    # readable curve, and because each pass only touches what is still
    # sharper than the limit, a front with no creases is untouched after
    # the first.
    for depth in range(max(1, int(passes))):
        points = _round_once(points, radius * (0.6 ** depth), chord_ok,
                             limit_deg)
    return points


def _round_once(points, radius, chord_ok, limit_deg):
    cos_limit = math.cos(math.radians(limit_deg))
    out = [points[0]]
    for a, b, c in zip(points, points[1:], points[2:]):
        v1 = (b[0] - a[0], b[1] - a[1])
        v2 = (c[0] - b[0], c[1] - b[1])
        n1 = math.hypot(*v1)
        n2 = math.hypot(*v2)
        if n1 < 1e-9 or n2 < 1e-9:
            out.append(b)
            continue
        cosang = (v1[0] * v2[0] + v1[1] * v2[1]) / (n1 * n2)
        if cosang >= cos_limit:          # gentle: genuine curvature, keep it
            out.append(b)
            continue
        back = min(radius, 0.4 * n1, 0.4 * n2)
        if back <= 1e-6:
            out.append(b)
            continue
        p = (b[0] - v1[0] / n1 * back, b[1] - v1[1] / n1 * back)
        q = (b[0] + v2[0] / n2 * back, b[1] + v2[1] / n2 * back)
        # One point into the turn, so the chamfer reads as a round rather
        # than as a cut corner.
        mid = ((p[0] + q[0]) * 0.5, (p[1] + q[1]) * 0.5)
        mid = ((mid[0] + b[0]) * 0.5, (mid[1] + b[1]) * 0.5)
        if chord_ok is not None and not (chord_ok(out[-1], p)
                                         and chord_ok(p, mid)
                                         and chord_ok(mid, q)):
            out.append(b)
            continue
        out.extend([p, mid, q])
    out.append(points[-1])
    return out


def _clean_wave_polyline(points, line_width, tolerance=0.05,
                         min_segment=0.30, allowed=None, guards=None,
                         smooth_creases=True):
    """Remove point noise without shortcutting across a hole or concavity."""
    if len(points) < 2:
        return []
    tolerance = max(0.01, min(0.12, float(tolerance)))
    original = shapely.geometry.LineString(points)
    rounding = max(0.0, AUTO_ROUND_X_WIDTH * float(line_width or 0.0))
    if not smooth_creases:
        rounding = 0.0
    guard = body = None
    if allowed is not None:
        if guards is not None and "guard" in guards:
            guard, body = guards["guard"]
        else:
            guard, body = _clean_guards(allowed)

    def chord_ok(a, b):
        if guard is None:
            return True
        try:
            chord = shapely.geometry.LineString([a, b])
            if not guard.covers(chord):
                return False
            if body is not None and not body.is_empty \
                    and chord.intersects(body):
                return False
            return True
        except Exception:
            return False

    for attempt in _simplify_attempts(original, tolerance, allowed, guards):
        return _smooth_and_thin(attempt, rounding, chord_ok, tolerance)
    # Nothing on the ladder was accepted whole. Rather than keep the raw
    # raster -- which is where "hundreds of lines when a couple dozen would
    # do" came from -- refine locally: every chord is checked individually
    # against the same guard, so the result is valid BY CONSTRUCTION and
    # only the genuinely difficult stretches keep their points.
    refined = _locally_refined(original, tolerance, allowed, guards)
    if refined is not None:
        return _smooth_and_thin(refined, rounding, chord_ok, tolerance)
    return _thinned_fallback(original, tolerance, allowed, guards)


def _chord_deviation(coords, i, j):
    """(worst distance from the chord i->j, index of the point at it)."""
    ax, ay = coords[i]
    bx, by = coords[j]
    dx, dy = bx - ax, by - ay
    span = math.hypot(dx, dy)
    worst, at = 0.0, i
    if span <= 1e-12:
        for k in range(i + 1, j):
            d = math.hypot(coords[k][0] - ax, coords[k][1] - ay)
            if d > worst:
                worst, at = d, k
        return worst, at
    for k in range(i + 1, j):
        px, py = coords[k]
        # Perpendicular distance to the infinite line is enough: the points
        # between i and j are, by construction, between them along the front.
        d = abs(dy * (px - ax) - dx * (py - ay)) / span
        if d > worst:
            worst, at = d, k
    return worst, at


def _simplify_locally(coords, tolerance, chord_ok):
    """Douglas-Peucker that refines ONLY where the geometry needs it.

    The problem this solves: a front that runs 30 mm along a gentle curve and
    then squeezes past a hole used to be simplified at ONE tolerance. If the
    coarse chord near the hole failed the safety check, the whole front was
    re-simplified finer -- so 28 mm of harmless curve came out as 130-odd
    moves to protect one tight corner. The owner saw it as "hundreds of lines
    where a couple dozen should have sufficed", and they were right.

    Here a chord is accepted when it is both within `tolerance` of the points
    it replaces AND passes `chord_ok`. Where it is not, the split happens at
    the worst point and only those halves are refined. Difficult geometry
    costs points where it is difficult and nowhere else.
    """
    keep = []

    def rec(i, j, depth=0):
        if j - i < 2:
            keep.append(i)
            return
        worst, at = _chord_deviation(coords, i, j)
        if worst <= tolerance and (chord_ok is None
                                   or chord_ok(coords[i], coords[j])):
            keep.append(i)
            return
        if depth > 64:                   # pathological input: stop splitting
            keep.append(i)
            return
        rec(i, at, depth + 1)
        rec(at, j, depth + 1)

    rec(0, len(coords) - 1)
    keep.append(len(coords) - 1)
    return [coords[i] for i in keep]


def _simplify_attempts(original, tolerance, allowed, guards=None):
    """Yield the first safe simplification of a front, tightening if needed.

    A front that hugs a hole can lose its curve to a chord that cuts the
    corner. Rather than give up and keep every raster point -- a 10 mm front
    can arrive with 980 of them -- try again with a tighter tolerance first.
    """
    if guards is None:
        guards = {}
    # One guard serves every rung now that the margin is a fixed safety
    # allowance rather than a multiple of the tolerance.
    if allowed is not None and "guard" not in guards:
        guards["guard"] = _clean_guards(allowed)
    guard = guards.get("guard")
    for factor in (1.0, 0.4, 0.15):
        step = tolerance * factor
        result = _simplify_once(original, step, allowed, guards=guard)
        if result is not None:
            yield result
            return


def _simplify_once(original, tolerance, allowed, min_segment=0.30,
                   guards=None):
    """One simplification pass, or None when it would shortcut the geometry.

    Deliberately still a whole-front simplification at one tolerance. Doing
    the per-chord refinement HERE was tried and made things worse: against a
    castellated bridge footprint (wall_snap=false) almost every chord leaves
    the region, so the recursion splits down to the raster and the Cube
    export went from 455 wave moves to 2350. Per-chord refinement is the
    right tool only once this has failed outright -- see _locally_refined.
    """
    geom = original.simplify(tolerance, preserve_topology=False)
    coords = list(geom.coords)
    if len(coords) < 2:
        return []
    minimum = max(0.08, float(min_segment))
    # Shapely has already removed harmless interior noise. Keep its remaining
    # curve points so a circular wall stays smooth; only remove short stubs at
    # the two ends, where disconnected fragments otherwise look frayed.
    clean = list(coords)
    while len(clean) > 2 and math.hypot(
            clean[1][0] - clean[0][0], clean[1][1] - clean[0][1]) < minimum:
        clean.pop(1)
    while len(clean) > 2 and math.hypot(
            clean[-1][0] - clean[-2][0], clean[-1][1] - clean[-2][1]) < minimum:
        clean.pop(-2)
    if len(clean) < 2 or _polyline_length(clean) < minimum:
        return []

    clean = _thin_points(clean, _MIN_POINT_GAP_MM)

    # Simplifying a curved front can replace an arc with a straight chord.
    # With a hole, that chord can cross empty space.
    if allowed is not None:
        candidate = shapely.geometry.LineString(clean)
        # How far the cleaned path may stray outside the region it was grown
        # in. Deliberately much tighter than the simplification tolerance:
        # straying is what puts plastic in a hole.
        guard, body = guards if guards is not None else _clean_guards(
            allowed, tolerance)
        if not guard.covers(candidate):
            return None
        # Touching a hole is not the same as cutting across one. A Wave end is
        # supposed to finish on a hole's wall, so it sits on the void boundary,
        # and a chord along a curved wall clips the void by a hair. What must
        # never happen is a shortcut that reaches into the body of the void,
        # so the void is shrunk by the same margin before testing.
        if body is not None and not body.is_empty and candidate.intersects(body):
            return None
    return clean


def _line_parts(geom):
    """Every LineString inside any shapely geometry (empty list if none)."""
    if geom is None or geom.is_empty:
        return []
    kind = getattr(geom, "geom_type", "")
    if kind == "LineString":
        return [geom]
    if kind in ("MultiLineString", "GeometryCollection"):
        out = []
        for part in geom.geoms:
            out.extend(_line_parts(part))
        return out
    return []


def _sliver_path(sliver, line_width):
    """A single path down the middle of a small leftover sliver."""
    try:
        box = sliver.minimum_rotated_rectangle
        corners = list(box.exterior.coords)[:4]
        if len(corners) < 4:
            return None
        edges = [(corners[i], corners[(i + 1) % 4]) for i in range(4)]
        edges.sort(key=lambda e: math.hypot(e[1][0] - e[0][0], e[1][1] - e[0][1]))
        short_a, short_b = edges[0], edges[1]
        mid_a = ((short_a[0][0] + short_a[1][0]) * 0.5,
                 (short_a[0][1] + short_a[1][1]) * 0.5)
        mid_b = ((short_b[0][0] + short_b[1][0]) * 0.5,
                 (short_b[0][1] + short_b[1][1]) * 0.5)
        spine = shapely.geometry.LineString([mid_a, mid_b]).intersection(sliver)
        parts = sorted(_line_parts(spine), key=lambda p: -p.length)
        if not parts or parts[0].length < line_width * 0.5:
            return None
        return [(float(x), float(y)) for x, y in parts[0].coords]
    except Exception:  # pragma: no cover - geometry is never allowed to throw
        return None


def _gap_fill_fronts(region, polylines, swcfg, cfg):
    """Short paths that fill slivers the wavefronts could not reach.

    A Wave front is a contour of equal distance from the supported edge, and
    the contours step outward one line spacing at a time. Where the far
    boundary runs at an angle to that march -- the tip of a corner is the
    usual case -- the last contour stops short and leaves a sliver with
    nothing in it. This fills such a sliver with one short path down its
    middle, and only ever adds material where there is currently none.
    """
    if not polylines or str(cfg.get("gap_fill", True)).strip().lower() in (
            "0", "false", "no", "off"):
        return []
    line_width = max(0.05, swcfg.line_width)
    minimum_area = max(0.005, _float_cfg(cfg, "gap_fill_min_area", 0.05))
    try:
        covered = shapely.ops.unary_union([
            shapely.geometry.LineString(p).buffer(
                line_width * 0.5, cap_style=2, join_style=2)
            for p in polylines if len(p) >= 2])
        if covered.is_empty:
            return []
        out = []
        for sliver in _polygon_parts(region.difference(covered)):
            if sliver.area < minimum_area:
                continue
            # It has to touch plastic that is already down, or it would be
            # printed into thin air.
            if sliver.distance(covered) > line_width * 0.1:
                continue
            path = _sliver_path(sliver, line_width)
            if path is not None:
                out.append(path)
        return out
    except Exception:  # pragma: no cover - geometry is never allowed to throw
        return []


def _polyline_length(points):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1])
               for a, b in zip(points, points[1:]))


def _as_bool(value):
    """Config values arrive from a JSON editor, so false can be a string."""
    if isinstance(value, str):
        return value.strip().lower() not in ("false", "0", "no", "off", "")
    return bool(value)


def _float_cfg(cfg, key, fallback):
    try:
        return float(cfg.get(key, fallback))
    except (TypeError, ValueError):
        return float(fallback)


def _smoothstep(value):
    """Gentle 0..1 ramp: flat at both ends, like slicer width blending."""
    value = max(0.0, min(1.0, float(value)))
    return value * value * (3.0 - 2.0 * value)


def _detail_boundary(unsupported, support, swcfg):
    """Walls/holes where Wave endpoints should taper.

    The support-side boundary is removed because the first Wave front needs
    full-strength anchoring. What remains is the visible outside edge, holes,
    and concave detail edges where full-width endpoint blobs look jagged.
    """
    if unsupported is None or unsupported.is_empty:
        return shapely.geometry.GeometryCollection()
    boundary = unsupported.boundary
    if support is not None and not support.is_empty:
        anchor_band = support.buffer(max(
            swcfg.line_spacing, swcfg.line_width, swcfg.overhang_tol))
        boundary = boundary.difference(anchor_band)
    return boundary if boundary is not None else shapely.geometry.GeometryCollection()


def _endpoint_touches_detail(point, detail_boundary, line_width):
    if detail_boundary is None or detail_boundary.is_empty:
        return False
    tolerance = max(0.025, float(line_width) * 0.75)
    return detail_boundary.distance(shapely.geometry.Point(point)) <= tolerance


def _cumulative_lengths(points):
    lengths = [0.0]
    for a, b in zip(points, points[1:]):
        lengths.append(lengths[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return lengths


def _interpolate_at(points, cumulative, target):
    if target <= 0.0:
        return points[0]
    total = cumulative[-1]
    if target >= total:
        return points[-1]
    for index in range(len(cumulative) - 1):
        start, end = cumulative[index], cumulative[index + 1]
        if target <= end + 1e-9:
            span = end - start
            if span <= 1e-12:
                return points[index + 1]
            ratio = (target - start) / span
            ax, ay = points[index]
            bx, by = points[index + 1]
            return (ax + (bx - ax) * ratio, ay + (by - ay) * ratio)
    return points[-1]


def _taper_samples(points, start_taper, end_taper, distance, max_segment):
    """Return [(x, y, arclength)], optionally subdividing taper zones.

    A max_segment of 0 keeps only the original Wave vertices. That is the
    default because Orca preview can show many tiny endpoint subdivisions as a
    rectangular/grid texture near walls.
    """
    cumulative = _cumulative_lengths(points)
    total = cumulative[-1]
    if total <= 1e-9:
        return []
    max_segment = float(max_segment)
    if max_segment <= 1e-9:
        return [(x, y, s) for (x, y), s in zip(points, cumulative)]

    breaks = {0.0, total, *cumulative}
    distance = min(max(0.0, distance), total)
    max_segment = max(0.03, max_segment)

    def add_zone(start, end):
        length = max(0.0, end - start)
        if length <= 1e-9:
            return
        steps = max(1, int(math.ceil(length / max_segment)))
        for i in range(steps + 1):
            breaks.add(start + length * i / steps)

    if start_taper:
        add_zone(0.0, distance)
    if end_taper:
        add_zone(max(0.0, total - distance), total)

    ordered = sorted(breaks)
    samples = []
    last = None
    for target in ordered:
        if last is not None and abs(target - last) <= 1e-7:
            continue
        x, y = _interpolate_at(points, cumulative, target)
        samples.append((x, y, target))
        last = target
    return samples


def _edge_flow_scale(arclength, total, start_taper, end_taper,
                     distance, min_flow):
    scale = 1.0
    if distance <= 1e-9:
        return scale
    if start_taper:
        scale = min(scale, min_flow + (1.0 - min_flow) *
                    _smoothstep(arclength / distance))
    if end_taper:
        scale = min(scale, min_flow + (1.0 - min_flow) *
                    _smoothstep((total - arclength) / distance))
    return max(min_flow, min(1.0, scale))


def _edge_flow_average(start, end, total, start_taper, end_taper,
                       distance, min_flow):
    """Average endpoint-flow scale across one emitted move.

    This preserves straight point-to-point moves while still lowering E for the
    portion of a move that lies inside an endpoint taper zone.
    """
    if end <= start + 1e-9:
        return 1.0
    # Sample a few midpoints. This is stable, simple, and avoids inserting any
    # extra G-code vertices just to calculate the E amount.
    samples = 5
    acc = 0.0
    for i in range(samples):
        t = start + (end - start) * (i + 0.5) / samples
        acc += _edge_flow_scale(
            t, total, start_taper, end_taper, distance, min_flow)
    return acc / samples


def _wave_taper_settings(cfg, unsupported, support, swcfg):
    distance = max(0.0, _float_cfg(cfg, "edge_taper_distance", 0.60))
    min_flow = max(0.05, min(1.0, _float_cfg(
        cfg, "edge_taper_min_flow", 0.55)))
    segment = max(0.0, _float_cfg(cfg, "edge_taper_segment", 0.0))
    if distance <= 1e-9 or min_flow >= 0.999:
        return None
    detail = _detail_boundary(unsupported, support, swcfg)
    if detail is None or detail.is_empty:
        return None
    return {"distance": distance, "min_flow": min_flow,
            "segment": segment, "detail": detail}


def _edge_snap_distance(cfg, swcfg):
    """How far a Wave endpoint may reach to land exactly on a wall/hole."""
    raw = cfg.get("edge_snap_distance", "auto")
    if isinstance(raw, str) and raw.strip().lower() == "auto":
        return max(0.04, swcfg.line_width * 0.75)
    return max(0.0, _float_cfg(cfg, "edge_snap_distance", 0.0))


def _points_from_geom(geom):
    if geom is None or geom.is_empty:
        return []
    gt = getattr(geom, "geom_type", "")
    if gt == "Point":
        return [(float(geom.x), float(geom.y))]
    if gt == "MultiPoint":
        return [(float(p.x), float(p.y)) for p in geom.geoms]
    if gt == "LineString":
        coords = list(geom.coords)
        return [(float(x), float(y)) for x, y in coords]
    if gt in ("MultiLineString", "GeometryCollection"):
        pts = []
        for part in geom.geoms:
            pts.extend(_points_from_geom(part))
        return pts
    return []


def _snap_endpoint(points, endpoint_index, detail, allowed, max_distance):
    """Extend an endpoint along its own segment until it reaches the boundary."""
    if max_distance <= 1e-9 or detail is None or detail.is_empty:
        return points
    if len(points) < 2:
        return points
    endpoint = points[endpoint_index]
    neighbor = points[1] if endpoint_index == 0 else points[-2]
    dx = endpoint[0] - neighbor[0]
    dy = endpoint[1] - neighbor[1]
    length = math.hypot(dx, dy)
    if length <= 1e-9:
        return points
    ux, uy = dx / length, dy / length
    ray_end = (endpoint[0] + ux * max_distance,
               endpoint[1] + uy * max_distance)
    ray = shapely.geometry.LineString([endpoint, ray_end])
    hit = ray.intersection(detail)
    choices = []
    for x, y in _points_from_geom(hit):
        along = (x - endpoint[0]) * ux + (y - endpoint[1]) * uy
        off = abs((x - endpoint[0]) * uy - (y - endpoint[1]) * ux)
        if along > 1e-6 and along <= max_distance + 1e-6 and off <= 1e-4:
            choices.append((along, (x, y)))
    if not choices:
        return points
    snapped = min(choices, key=lambda item: item[0])[1]
    candidate = list(points)
    candidate[endpoint_index] = snapped
    line = shapely.geometry.LineString(candidate)
    guard = allowed.buffer(max(0.002, min(0.08, max_distance * 0.5)))
    voids = _interior_voids(allowed)
    if not guard.covers(line):
        return points
    if not voids.is_empty and line.intersects(voids):
        return points
    return candidate


def _snap_wave_polylines(polylines, unsupported, support, swcfg, cfg):
    """Make Wave endpoints land on the perimeter instead of stopping short."""
    max_distance = _edge_snap_distance(cfg, swcfg)
    if max_distance <= 1e-9 or unsupported is None or unsupported.is_empty:
        return polylines
    detail = _detail_boundary(unsupported, support, swcfg)
    if detail is None or detail.is_empty:
        return polylines
    snapped = []
    for polyline in polylines:
        pts = [(float(x), float(y)) for x, y in polyline]
        if len(pts) < 2:
            continue
        pts = _snap_endpoint(pts, 0, detail, unsupported, max_distance)
        pts = _snap_endpoint(pts, -1, detail, unsupported, max_distance)
        snapped.append(pts)
    return snapped


def _edge_clearance(cfg, swcfg):
    """How far Wave centerlines stay away from non-support detail boundaries."""
    raw = cfg.get("edge_clearance", "auto")
    if isinstance(raw, str) and raw.strip().lower() == "auto":
        # A little under half a line width keeps the bead inside the bridge
        # region while still letting the overhang wall/perimeter cover the seam.
        return max(0.0, swcfg.line_width * 0.45)
    return max(0.0, _float_cfg(cfg, "edge_clearance", 0.0))


def _clearance_domain(unsupported, support, swcfg, cfg):
    clearance = _edge_clearance(cfg, swcfg)
    if clearance <= 1e-9 or unsupported is None or unsupported.is_empty:
        return unsupported, 0.0
    detail = _detail_boundary(unsupported, support, swcfg)
    if detail is None or detail.is_empty:
        return unsupported, 0.0
    blocked = detail.buffer(clearance, cap_style=2, join_style=2)
    domain = unsupported.difference(blocked)
    if domain.is_empty:
        # Bad settings should not silently delete the whole Wave field.
        return unsupported, 0.0
    return domain, clearance


def _clip_polyline_to_domain(points, domain, min_length):
    """Clip one ordered polyline to a domain and keep fragment order."""
    if len(points) < 2 or domain is None or domain.is_empty:
        return []
    source = shapely.geometry.LineString(points)
    clipped = source.intersection(domain.buffer(0.001))
    pieces = []
    for part in _line_parts(clipped):
        if part.length < min_length:
            continue
        coords = list(part.coords)
        if len(coords) < 2:
            continue
        start = source.project(shapely.geometry.Point(coords[0]))
        end = source.project(shapely.geometry.Point(coords[-1]))
        if end < start:
            coords = list(reversed(coords))
            start, end = end, start
        pieces.append((start, coords))
    return [coords for _start, coords in sorted(pieces, key=lambda item: item[0])]


def _inset_wave_polylines(polylines, unsupported, support, swcfg, cfg):
    """Return emitted Wave paths inset from perimeter/hole detail edges.

    Coverage removal still uses the original cleaned paths. This function only
    changes the G-code that is emitted, so no old straight bridge fragments are
    kept at the perimeter just because the visible Wave bead was trimmed back.
    """
    domain, clearance = _clearance_domain(unsupported, support, swcfg, cfg)
    if clearance <= 1e-9:
        return polylines
    minimum = max(0.08, swcfg.line_width * 0.35)
    out = []
    for polyline in polylines:
        out.extend(_clip_polyline_to_domain(polyline, domain, minimum))
    return out if out else polylines


def _slicer_setting(text, key):
    """Read one '; key = value' line Orca writes into the exported file."""
    match = re.search(r"^;\s*" + re.escape(key) + r"\s*=\s*(.+?)\s*$",
                      text, re.MULTILINE)
    return match.group(1).strip() if match else None


# ---------------------------------------------------------------------------
#  "auto": take the value from the print instead of a constant
#
#  Most of the numbers in this plugin are not really numbers, they are
#  multiples of something the export already states. line_spacing 0.35 is
#  "seven eighths of a 0.4 line"; min_wave_length 1.0 is "two and a half
#  lines"; simplify_tolerance 0.05 is "four times a stock Orca resolution".
#  Written as constants they are correct for one profile and quietly wrong
#  for every other.
#
#  Every factor below is chosen so that "auto" on a stock 0.4 mm / 0.0125 mm
#  resolution profile reproduces the constant this plugin shipped with, to
#  the digit. So "auto" is not a new behaviour for the common case -- it is
#  the same behaviour, finally expressed in terms of the thing it depends on.
#
#  Note which settings are NOT in here: amplitude-like choices with no
#  equivalent in the slicer (pattern, start_policy, flow_ratio, fan) stay
#  constants, because no amount of reading the G-code tells you what the
#  user wants a part to look like.
# ---------------------------------------------------------------------------
AUTO_WORDS = ("", "auto", "orca")

#   setting               x line width   value at 0.40 mm
AUTO_WIDTH_FACTORS = {
    "line_spacing":        0.875,       # 0.350 mm
    "perimeter_overlap":   0.25,        # 0.100 mm
    "min_wave_length":     2.5,         # 1.000 mm
    "min_wave_segment":    0.75,        # 0.300 mm
    "edge_taper_distance": 1.5,         # 0.600 mm
}
AUTO_MIN_OVERHANG_AREA = 3.125          # x line_width^2 -> 0.50 mm^2 at 0.40
# Smoothing is a geometric operation on a path one bead wide, so it scales
# with the bead -- 0.125 x width is the old 0.05 mm at a 0.40 line. Your
# profile's Resolution acts as a FLOOR instead of a multiplier: there is no
# point smoothing by less than the jitter the slicer itself can produce, but
# a deliberately coarse Resolution should not start eating wave detail.
AUTO_SIMPLIFY_X_WIDTH = 0.125           # x line width -> 0.050 mm at 0.40
AUTO_SIMPLIFY_RANGE = (0.02, 0.12)      # mm, clamp either way
AUTO_TRAVEL_FALLBACK = 120.0            # mm/s
AUTO_FAN_FALLBACK = 1.0
# Iteration cap: fronts needed to cross the region, plus headroom. A cap is
# only a runaway guard, so it may be generous -- but a FIXED 400 is both too
# small for a big overhang (it stops half way) and pointless for a small one.
AUTO_ITERATION_HEADROOM = 16
AUTO_ITERATION_RANGE = (64, 20000)


def _is_auto(value):
    return value is None or (isinstance(value, str)
                             and value.strip().lower() in AUTO_WORDS)


def _profile_float(profile, key):
    """One numeric setting from the export's config block, or None."""
    raw = profile.get(key)
    if raw is None:
        return None
    try:
        return float(str(raw).split(",")[0])
    except ValueError:
        return None


def _orca_profile(text):
    """The settings Orca wrote into the exported file that Wave can follow.

    Read once per export. Everything here is a real Orca setting the user
    already tuned in their own profile -- the point is that they should not
    have to type it a second time into a plugin panel.
    """
    keys = (
        "resolution",            # Quality > Precision > Resolution
        "travel_speed",          # Speed > Travel
        "bridge_fan_speed",      # Cooling > Bridges fan speed
        "overhang_fan_speed",
        "enable_arc_fitting",
        "nozzle_diameter",
        "bridge_speed",
    )
    found = {}
    for key in keys:
        value = _slicer_setting(text, key)
        if value is not None:
            found[key] = value
    return found


def _auto_iterations(region, spacing):
    """Enough fronts to cross `region` once, plus headroom."""
    try:
        minx, miny, maxx, maxy = region.bounds
        span = math.hypot(maxx - minx, maxy - miny)
    except Exception:
        return AUTO_ITERATION_RANGE[0]
    if spacing <= 0.0:
        return AUTO_ITERATION_RANGE[0]
    need = int(span / spacing) + AUTO_ITERATION_HEADROOM
    return max(AUTO_ITERATION_RANGE[0], min(AUTO_ITERATION_RANGE[1], need))


def _resolve_autos(cfg, line_width, profile):
    """`cfg` with every "auto" replaced by what it means for THIS print.

    Returns a new dict -- the user's config is never modified -- plus the
    numbers are recorded under `_auto_notes` so the log can say what was
    decided instead of echoing the word "auto" back at them.
    """
    out = dict(cfg)
    notes = {}
    width = max(0.05, float(line_width or 0.4))

    for key, factor in AUTO_WIDTH_FACTORS.items():
        if _is_auto(cfg.get(key)):
            out[key] = factor * width
            notes[key] = (f"{out[key]:.3f} mm (auto: {factor:g} x the "
                          f"{width:.2f} mm Wave line)")
        else:
            out[key] = _float_cfg(cfg, key, AUTO_WIDTH_FACTORS[key] * width)

    if _is_auto(cfg.get("min_overhang_area")):
        out["min_overhang_area"] = AUTO_MIN_OVERHANG_AREA * width * width
        notes["min_overhang_area"] = (
            f"{out['min_overhang_area']:.3f} mm2 (auto: about three "
            f"{width:.2f} mm lines squared)")
    else:
        out["min_overhang_area"] = _float_cfg(cfg, "min_overhang_area", 0.5)

    if _is_auto(cfg.get("simplify_tolerance")):
        res = _profile_float(profile, "resolution") or 0.0
        scaled = AUTO_SIMPLIFY_X_WIDTH * width
        value = max(AUTO_SIMPLIFY_RANGE[0],
                    min(AUTO_SIMPLIFY_RANGE[1], max(scaled, res)))
        how = (f"auto: {AUTO_SIMPLIFY_X_WIDTH:g} x the {width:.2f} mm Wave "
               f"line")
        if res and res > scaled:
            how = (f"auto: raised to your profile's {res:g} mm resolution, "
                   f"which is coarser than the line would ask for")
        notes["simplify_tolerance"] = f"{value:.3f} mm ({how})"
        out["simplify_tolerance"] = value
    else:
        out["simplify_tolerance"] = _float_cfg(cfg, "simplify_tolerance", 0.05)

    if _is_auto(cfg.get("travel_speed")):
        speed = _profile_float(profile, "travel_speed") or AUTO_TRAVEL_FALLBACK
        out["travel_speed"] = speed
        notes["travel_speed"] = (
            f"{speed:.0f} mm/s (auto: your profile's travel speed)"
            if _profile_float(profile, "travel_speed")
            else f"{speed:.0f} mm/s (auto, but the export states no travel "
                 f"speed)")
    else:
        out["travel_speed"] = _float_cfg(cfg, "travel_speed", AUTO_TRAVEL_FALLBACK)

    if _is_auto(cfg.get("fan")):
        # bridge_fan_speed ONLY. overhang_fan_speed is about sloped walls and
        # is commonly set lower; substituting it would quietly under-cool a
        # wave, which is unsupported extrusion in open air and needs all the
        # cooling it can get. No bridge fan stated -> 100%, not a guess.
        raw = _profile_float(profile, "bridge_fan_speed")
        value = max(0.0, min(1.0, raw / 100.0)) if raw is not None else AUTO_FAN_FALLBACK
        out["fan"] = value
        notes["fan"] = (f"{value * 100:.0f}% (auto: your profile's bridge fan)"
                        if raw is not None else
                        "100% (auto, but the export states no bridge fan)")
    else:
        out["fan"] = _float_cfg(cfg, "fan", AUTO_FAN_FALLBACK)

    out["_auto_notes"] = notes
    return out


def _arc_limits(text, cfg, swcfg):
    """Whether to emit G2/G3 arcs, and how far they may stray if we do.

    Wave runs after Orca has written the file, so Orca's own arc fitter never
    sees these moves. "auto" follows the export's own `enable_arc_fitting`
    setting: if arc fitting is on in the print profile the firmware
    understands G2/G3, and if it is off nothing here starts emitting commands
    the printer might reject. Returns None when arcs are off.
    """
    raw = cfg.get("arc_fitting", "auto")
    if isinstance(raw, str):
        choice = raw.strip().lower()
        if choice in ("", "auto"):
            flag = _slicer_setting(text or "", "enable_arc_fitting")
            enabled = str(flag).lower() in ("1", "true", "yes", "on")
        else:
            enabled = choice in ("1", "true", "yes", "on")
    else:
        enabled = bool(raw)
    if not enabled:
        return None
    raw_tolerance = cfg.get("arc_tolerance", "auto")
    if isinstance(raw_tolerance, str) and \
            raw_tolerance.strip().lower() in ("", "auto"):
        try:
            tolerance = float(_slicer_setting(text or "", "resolution"))
        except (TypeError, ValueError):
            tolerance = 0.05
        # Never looser than the print profile's own curve tolerance, and
        # never loose enough to matter next to a wall or a hole.
        tolerance = min(max(tolerance, 0.005), 0.05)
    else:
        tolerance = max(0.0, _float_cfg(cfg, "arc_tolerance", 0.05))
    if tolerance <= 1e-9:
        return None
    return {"tolerance": tolerance,
            "min_length": max(1.0, swcfg.line_width * 2.0),
            "min_radius": max(0.4, swcfg.line_width),
            "max_radius": 200.0,
            "max_sweep": math.radians(150.0),
            # How far an arc may bow away from the straight moves it replaces.
            # Kept to a quarter of the rung spacing: each Wave rung has to
            # stay close enough to the previous one to fuse to it.
            "max_bulge": max(tolerance, swcfg.line_spacing * 0.25),
            "max_points": 120}


def _circle_through(p0, p1, p2):
    """Centre and radius of the circle through three points, or None."""
    (x0, y0), (x1, y1), (x2, y2) = p0, p1, p2
    d = 2.0 * (x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1))
    if abs(d) < 1e-12:
        return None                      # collinear: a straight move
    s0, s1, s2 = x0 * x0 + y0 * y0, x1 * x1 + y1 * y1, x2 * x2 + y2 * y2
    cx = (s0 * (y1 - y2) + s1 * (y2 - y0) + s2 * (y0 - y1)) / d
    cy = (s0 * (x2 - x1) + s1 * (x0 - x2) + s2 * (x1 - x0)) / d
    return cx, cy, math.hypot(x0 - cx, y0 - cy)


def _arc_for_span(points, circle, limits):
    """Describe the arc that can stand in for this run of points, or None.

    Two different errors have to stay inside the tolerance together: how far
    the kept points sit off the circle, and how far the circle bulges away
    from the straight chords it replaces. Both are measured, because a Wave
    end now sits inside the wall bead and an arc that bulged could push it
    through the wall or into a hole.
    """
    if circle is None:
        return None
    cx, cy, radius = circle
    if not (limits["min_radius"] <= radius <= limits["max_radius"]):
        return None
    tolerance = limits["tolerance"]
    angles = []
    worst_radial = 0.0
    for x, y in points:
        worst_radial = max(worst_radial, abs(math.hypot(x - cx, y - cy) - radius))
        if worst_radial > tolerance:
            return None
        angles.append(math.atan2(y - cy, x - cx))
    sweep = 0.0
    direction = 0
    worst_bulge = 0.0
    for a, b in zip(angles, angles[1:]):
        step = (b - a + math.pi) % (2.0 * math.pi) - math.pi
        if abs(step) < 1e-12:
            continue
        sign = 1 if step > 0 else -1
        if direction == 0:
            direction = sign
        elif sign != direction:
            return None                  # the run doubles back on itself
        sweep += step
        # How far the arc bows away from the straight chord it replaces. The
        # chords are themselves a simplification of a smooth front, so this is
        # a sanity cap rather than an accuracy test; whether the arc is
        # actually safe is decided against the real region below.
        worst_bulge = max(worst_bulge, radius * (1.0 - math.cos(abs(step) / 2.0)))
        if worst_bulge > limits["max_bulge"]:
            return None
    if direction == 0 or abs(sweep) > limits["max_sweep"]:
        return None
    return {"cx": cx, "cy": cy, "radius": radius, "ccw": direction > 0,
            "length": radius * abs(sweep)}


def _arc_polyline(start, end, arc, step=0.2):
    """The arc, as points, so it can be tested against real geometry."""
    cx, cy, radius = arc["cx"], arc["cy"], arc["radius"]
    a0 = math.atan2(start[1] - cy, start[0] - cx)
    a1 = math.atan2(end[1] - cy, end[0] - cx)
    sweep = (a1 - a0) % (2.0 * math.pi)
    if not arc["ccw"]:
        sweep -= 2.0 * math.pi
    count = max(2, int(abs(sweep) * radius / max(0.02, step)))
    return [(cx + radius * math.cos(a0 + sweep * i / count),
             cy + radius * math.sin(a0 + sweep * i / count))
            for i in range(count + 1)]


def _arc_is_safe(start, end, arc, limits):
    """An arc may only replace moves if it stays where the Wave is allowed.

    The straight moves were already checked against the region. An arc bows
    away from them, so it is re-checked here: a Wave end sits inside the wall
    bead and this is what stops a bowed arc pushing through the wall or into
    a hole.
    """
    guard = limits.get("guard")
    if guard is None:
        return True
    line = shapely.geometry.LineString(_arc_polyline(start, end, arc))
    if not guard.covers(line):
        return False
    body = limits.get("void_body")
    return not (body is not None and not body.is_empty and line.intersects(body))


def _fit_arc_moves(points, limits):
    """Rewrite one Wave front as straight moves and circular arcs.

    Returns [("line", (x, y), length), ("arc", (x, y), arc), ...] starting
    from points[0], which is not emitted itself.
    """
    moves = []
    index = 0
    count = len(points)
    while index < count - 1:
        best = None
        end = index + 3                  # an arc needs at least four points
        while end < count and (end - index) <= limits["max_points"]:
            span = points[index:end + 1]
            arc = _arc_for_span(
                span, _circle_through(span[0], span[len(span) // 2], span[-1]),
                limits)
            if arc is None:
                break
            if arc["length"] >= limits["min_length"] and \
                    _arc_is_safe(span[0], span[-1], arc, limits):
                best = (end, arc)
            end += 1
        if best is None:
            nxt = points[index + 1]
            moves.append(("line", nxt, math.hypot(nxt[0] - points[index][0],
                                                  nxt[1] - points[index][1])))
            index += 1
        else:
            end, arc = best
            moves.append(("arc", points[end], arc))
            index = end
    return moves


def _adaptive_flow_scales(polylines, region, swcfg, cfg):
    """One flow multiplier per polyline, Arachne-style.

    Arachne's idea is that a fixed line width cannot tile an arbitrary shape:
    wherever the remaining space is not a whole number of beads wide, a
    fixed-width fill either leaves a sliver empty or overlaps itself. Its
    answer is to VARY the width of the beads so the space is filled exactly.

    Wave has the same problem in one dimension. Fronts step outward a fixed
    `line_spacing` at a time, so the strip between the last front and the far
    boundary is whatever is left over -- usually not a whole spacing.

    The paths are not moved here; only flow changes, which is the half of
    Arachne that can be done safely after the fact. Each front is given the
    area of the uncovered region that is nearer to it than to any other
    front, and asked to extrude the material that area needs, spread along
    its own length:

        extra width = uncovered area assigned to this front / its length
        scale       = (line width + extra width) / line width

    Clamped, because a front must never be asked to put down several beads'
    worth of plastic in one pass. Returns None when nothing needs widening,
    so the common case keeps the existing single-flow fast path.
    """
    if not _as_bool(cfg.get("adaptive_flow", False)):
        return None
    usable = [p for p in polylines if len(p) >= 2]
    if not usable or region is None or region.is_empty:
        return None
    width = max(0.05, swcfg.line_width)
    cap = max(1.0, min(3.0, _float_cfg(cfg, "adaptive_flow_max", 1.5)))
    try:
        lines = [shapely.geometry.LineString(p) for p in usable]
        covered = shapely.ops.unary_union(
            [ln.buffer(width * 0.5, cap_style=2, join_style=2)
             for ln in lines])
        leftover = region.difference(covered)
        if leftover.is_empty or leftover.area <= 0.0:
            return None
        # Assign each uncovered patch to the front it sits against. A patch
        # touching nothing is not this mechanism's problem -- gap_fill exists
        # for material that needs a path of its own.
        extra = [0.0] * len(usable)
        for patch in _polygon_parts(leftover):
            if patch.area <= 0.0:
                continue
            distances = [ln.distance(patch) for ln in lines]
            nearest = min(range(len(lines)), key=distances.__getitem__)
            if distances[nearest] > width:
                continue
            extra[nearest] += patch.area
        scales = []
        widened = 0
        for area, ln in zip(extra, lines):
            if area <= 0.0 or ln.length <= 1e-9:
                scales.append(1.0)
                continue
            scale = min(cap, (width + area / ln.length) / width)
            if scale > 1.001:
                widened += 1
            scales.append(scale)
        return scales if widened else None
    except Exception:  # pragma: no cover - geometry never breaks an export
        return None



def _contour_finish_paths(unsupported, support, swcfg, cfg, polylines,
                          walls=None):
    """Fill the stretches along the wall that the fronts never reached.

    Wavefronts are contours of equal distance from the SUPPORTED edge. Near
    a curved wall that is a different shape from the wall, so the outermost
    front runs at an angle to it and the field can fall short in places.
    Measured on the owner's curved-perimeter export: the wall is 0.157 mm
    from the nearest wave for most of its length -- exactly the intended
    overlap -- but 5% of it is more than a line width away and the worst
    stretch is 3.8 mm short. That is the scallop.

    So the wall is sampled, the stretches that are short are found, and a
    bead is laid along the region boundary beside them -- which follows the
    wall, because the region is wall-bounded. Nothing is added where the
    fronts already arrive.
    """
    if not _as_bool(cfg.get("contour_finish", True)):
        return []
    if unsupported is None or unsupported.is_empty or not walls:
        return []
    width = max(0.05, swcfg.line_width)
    overlap = max(0.0, min(0.9, _float_cfg(cfg, "wall_overlap", 0.25))) * width
    short_of = width                      # "short" = more than one bead away
    try:
        joined = shapely.ops.unary_union([w["geom"] for w in walls])
        # linemerge raises outright on a single LineString, which silently
        # turned this whole pass into a no-op through the except below.
        if joined.geom_type == "MultiLineString":
            joined = shapely.ops.linemerge(joined)
        pieces = (list(joined.geoms)
                  if joined.geom_type == "MultiLineString" else [joined])
        pieces = [p for p in pieces if p.geom_type == "LineString"]
        covered = shapely.ops.unary_union(
            [shapely.geometry.LineString(p) for p in polylines if len(p) >= 2])
        if covered.is_empty:
            return []
        inner = unsupported.buffer(-overlap)
        if inner.is_empty:
            return []
        inner_edge = inner.boundary
        minimum = max(2.0 * width, _float_cfg(cfg, "min_wave_length", 1.0))
        out = []
        for piece in pieces:
            if piece.length < minimum:
                continue
            step = max(0.2, width * 0.5)
            count = int(piece.length / step)
            run = []
            for i in range(count + 1):
                point = piece.interpolate(min(i * step, piece.length))
                if point.distance(covered) > short_of:
                    # The matching point just inside the region, which is
                    # bounded by this same wall.
                    near = inner_edge.interpolate(inner_edge.project(point))
                    if near.distance(point) <= width * 2.5:
                        run.append((float(near.x), float(near.y)))
                        continue
                if len(run) >= 2:
                    line = shapely.geometry.LineString(run)
                    if line.length >= minimum and unsupported.covers(line):
                        out.append(run)
                run = []
            if len(run) >= 2:
                line = shapely.geometry.LineString(run)
                if line.length >= minimum and unsupported.covers(line):
                    out.append(run)
        return out
    except Exception:  # pragma: no cover - geometry never breaks an export
        return []


def _emit_wave_gcode(polylines, z, swcfg, cfg, unsupported=None,
                     support=None, restore_fan=None, arcs=None,
                     print_speed=None, flow_scales=None):
    """Emit Wave G-code, tapering endpoint flow near walls/holes.

    The path geometry is unchanged. Only E per millimetre is reduced over a
    short distance when a Wave endpoint touches a non-support detail boundary.
    """
    if not polylines:
        return []
    base_e_per_mm = e_per_mm = swcfg.e_per_mm()
    # print_speed overrides swcfg when the section carried Orca's own
    # bridge feedrate and the user asked to follow it.
    speed = print_speed if print_speed and print_speed > 0 else swcfg.print_speed
    print_f = int(round(speed * 60))
    travel_f = int(round(swcfg.travel_speed * 60))
    taper = _wave_taper_settings(cfg, unsupported, support, swcfg)
    if arcs is not None and unsupported is not None and not unsupported.is_empty:
        arcs = dict(arcs)
        arcs["guard"] = unsupported.buffer(arcs["tolerance"])
        voids = _interior_voids(unsupported)
        arcs["void_body"] = (voids.buffer(-arcs["tolerance"])
                             if not voids.is_empty else voids)
    _domain, clearance = _clearance_domain(unsupported, support, swcfg, cfg)
    out = ["; ==== WAVE OVERHANG BEGIN ====",
           "M83",
           f"M106 S{int(round(max(0.0, min(1.0, swcfg.fan)) * 255))}"]
    if clearance > 1e-9:
        out.append("; wave-overhangs edge clearance "
                   f"centerline={clearance:.3f}")
    if taper is not None:
        out.append("; wave-overhangs edge taper "
                   f"distance={taper['distance']:.3f} "
                   f"min_flow={taper['min_flow']:.3f}")
    if flow_scales:
        out.append("; wave-overhangs adaptive flow "
                   f"max={max(flow_scales):.3f}")
    for index, pts in enumerate(polylines):
        if len(pts) < 2:
            continue
        # Arachne-style: this front may be asked to lay down a wider bead to
        # absorb the uncovered strip beside it. Only E changes; the path is
        # exactly where it was.
        e_per_mm = base_e_per_mm
        if flow_scales and index < len(flow_scales):
            e_per_mm = base_e_per_mm * flow_scales[index]
        points = [(float(x), float(y)) for x, y in pts]
        x0, y0 = points[0]
        out.append(f"G0 F{travel_f} X{x0:.3f} Y{y0:.3f} Z{z:.3f}")
        out.append(f"G1 F{print_f}")
        # Coordinates are written to three decimals, so a step shorter than a
        # micron rounds to the same X/Y as the move before it and becomes a
        # literal no-op line -- usually "E0.00000" too. Remember what was
        # actually written and roll the skipped extrusion into the next real
        # move, so the file loses the junk without losing any material.
        emitted = [(round(x0, 3), round(y0, 3))]
        pending = [0.0]

        def put(x, y, e):
            pending[0] += e
            key = (round(x, 3), round(y, 3))
            if key == emitted[0]:
                return
            out.append("G1 X%.3f Y%.3f E%.5f" % (x, y, pending[0]))
            pending[0] = 0.0
            emitted[0] = key
        if taper is None:
            samples = [(x, y, s) for (x, y), s in zip(
                points, _cumulative_lengths(points))]
            start_taper = end_taper = False
            distance = 0.0
            min_flow = 1.0
        else:
            start_taper = _endpoint_touches_detail(
                points[0], taper["detail"], swcfg.line_width)
            end_taper = _endpoint_touches_detail(
                points[-1], taper["detail"], swcfg.line_width)
            distance = taper["distance"]
            min_flow = taper["min_flow"]
            samples = _taper_samples(
                points, start_taper, end_taper, distance, taper["segment"])
        if len(samples) < 2:
            continue
        total = samples[-1][2]
        # Arcs are only used where the front prints at full flow. A tapered
        # end ramps E along its own moves and a single arc cannot express
        # that, so those zones stay as straight moves.
        first, last = 0, len(samples) - 1
        if arcs is not None:
            head = distance if start_taper else 0.0
            tail = total - distance if end_taper else total
            while first < last and samples[first][2] < head - 1e-9:
                first += 1
            while last > first and samples[last][2] > tail + 1e-9:
                last -= 1
        else:
            first = last = 0

        def straight(index_from, index_to):
            px, py, ps = samples[index_from]
            for x, y, arclength in samples[index_from + 1:index_to + 1]:
                seg = math.hypot(x - px, y - py)
                if seg <= 1e-9:
                    px, py, ps = x, y, arclength
                    continue
                scale = _edge_flow_average(
                    ps, arclength, total, start_taper, end_taper,
                    distance, min_flow)
                put(x, y, seg * e_per_mm * scale)
                px, py, ps = x, y, arclength

        if arcs is None or last - first < 3:
            straight(0, len(samples) - 1)
            continue
        straight(0, first)
        for kind, (x, y), extra in _fit_arc_moves(
                [(s[0], s[1]) for s in samples[first:last + 1]], arcs):
            if kind == "line":
                put(x, y, extra * e_per_mm)
            else:
                # I and J are offsets from where the machine actually is,
                # which is the last coordinate written -- not the unrounded
                # geometric point, which can sit half a micron away from it.
                ax, ay = emitted[0]
                out.append(
                    "%s X%.3f Y%.3f I%.3f J%.3f E%.5f"
                    % ("G3" if extra["ccw"] else "G2", x, y,
                       extra["cx"] - ax, extra["cy"] - ay,
                       extra["length"] * e_per_mm + pending[0]))
                pending[0] = 0.0
                emitted[0] = (round(x, 3), round(y, 3))
        straight(last, len(samples) - 1)
    if restore_fan is not None:
        out.append(f"M106 S{int(restore_fan)}")
    out.append("; ==== WAVE OVERHANG END ====")
    return out


_VOID_CACHE = {}
_VOID_CACHE_MAX = 64


def _interior_voids(geom):
    """Return polygon holes that a simplified front must never enter.

    Memoised on the region object. This is a property of the region, but it
    was being recomputed once per ENDPOINT -- 696 calls on a 36-hole stress
    case, each doing a buffer and a union, which was 40% of the whole pass.
    A part with many holes is exactly where that hurt most, and a pass that
    runs out of its time budget hands the file back unwaved, which is what
    "the plugin just did nothing" looks like from the outside.
    """
    if geom is None or geom.is_empty:
        return shapely.geometry.GeometryCollection()
    key = id(geom)
    hit = _VOID_CACHE.get(key)
    if hit is not None and hit[0] is geom:
        return hit[1]
    polygons = ([geom] if geom.geom_type == "Polygon" else
                list(geom.geoms) if geom.geom_type == "MultiPolygon" else [])
    holes = []
    for polygon in polygons:
        for ring in polygon.interiors:
            hole = shapely.geometry.Polygon(ring).buffer(-0.001)
            if not hole.is_empty:
                holes.append(hole)
    result = (shapely.ops.unary_union(holes) if holes
              else shapely.geometry.GeometryCollection())
    if len(_VOID_CACHE) > _VOID_CACHE_MAX:
        # id() is only unique while the object is alive, so the cache keeps a
        # reference to the key object and is checked with `is`. Bounded, and
        # cleared wholesale rather than tracking an LRU for 64 entries.
        _VOID_CACHE.clear()
    _VOID_CACHE[key] = (geom, result)
    return result


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


def _order_wave_tracks(tracks, support, pattern="smart", start_policy="supported",
                       component_order="support", start_xy=None):
    """Order and orient fronts while keeping travel moves non-extruding.

    `monotonic` uses one global endpoint direction for all fronts. It does not
    extrude across the gap between fronts: those connecting moves remain G0
    travels so a warm perimeter is never scored by an accidental line.
    `nearest` component order chooses the next same-distance front closest to
    the current endpoint, reducing long visible G0 returns around holes.
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
    component_order = str(component_order or "support").lower()
    if component_order not in ("support", "nearest"):
        component_order = "support"
    if component_order == "nearest" and mode != "monotonic":
        # Keep distance layers intact for support anchoring, but reorder
        # disconnected components within each layer to avoid a long diagonal
        # travel across a hole or across freshly printed material.
        groups = []
        for track in ordered:
            if not groups or abs(track.distance - groups[-1][0].distance) > 1e-6:
                groups.append([track])
            else:
                groups[-1].append(track)
        out = []
        cursor = start_xy
        for group in groups:
            pending = list(group)
            while pending:
                choices = []
                for track in pending:
                    pts = list(track.points)
                    if cursor is None:
                        pts = _endpoint_order(pts, start_policy, support)
                        score = 0.0
                    else:
                        a, b = pts[0], pts[-1]
                        if math.hypot(cursor[0] - b[0], cursor[1] - b[1]) < math.hypot(
                                cursor[0] - a[0], cursor[1] - a[1]):
                            pts = list(reversed(pts))
                        score = math.hypot(cursor[0] - pts[0][0],
                                           cursor[1] - pts[0][1])
                    choices.append((score, track, pts))
                _, selected, pts = min(choices, key=lambda item: item[0])
                pending.remove(selected)
                if mode == "zigzag" and len(out) % 2:
                    pts = list(reversed(pts))
                out.append(pts)
                cursor = pts[-1]
        return out
    out = []
    for index, track in enumerate(ordered):
        pts = _endpoint_order(list(track.points), start_policy, support)
        if mode == "zigzag" and index % 2:
            pts = list(reversed(pts))
        out.append(pts)
    return out


class _WaveBudgetExceeded(Exception):
    """The G-code pass ran past its wall-clock ceiling and gave up."""


# The time budget, when it is "auto". A flat 30 seconds was fine for a test
# cube and far too short for a real part: the pass costs roughly what the
# geometry costs, so a 4 MB export of something with dozens of holes can run
# out of budget, hand the file back unwaved, and look exactly like "the
# plugin did nothing". Scaling with the size of the export is crude but it
# is the right shape, and the cap keeps a pathological file from hanging an
# export forever.
AUTO_BUDGET_BASE = 30.0          # seconds
AUTO_BUDGET_PER_MB = 45.0        # seconds per megabyte of G-code
AUTO_BUDGET_CAP = 300.0          # seconds, hard ceiling


def _auto_budget(text):
    megabytes = len(text) / 1048576.0
    return min(AUTO_BUDGET_CAP,
               AUTO_BUDGET_BASE + AUTO_BUDGET_PER_MB * megabytes)


def _budget_deadline(cfg, started, text=""):
    """Absolute time after which the G-code pass must stop, or None."""
    raw = cfg.get("time_budget", "auto")
    if _is_auto(raw):
        return started + _auto_budget(text)
    try:
        budget = float(raw)
    except (TypeError, ValueError):
        budget = _auto_budget(text)
    if budget <= 0.0:
        return None
    return started + budget



# ---------------------------------------------------------------------------
#  Print order: waves first, overhanging wall last
#
#  Orca prints a layer walls-first. On an overhanging layer that is exactly
#  backwards: the overhang wall is laid into open air with nothing to sit on,
#  so it droops before the waves that were supposed to carry it even exist.
#  The waves bridge their way out from supported material and support each
#  other as they go; print them first and the wall then lands on something.
#
#  So on any layer where Wave actually did something, the overhanging part of
#  the wall is lifted out of its original position and re-emitted after the
#  wave block.
#
#  Three rules keep this safe:
#   * Only RELATIVE-E files. In absolute E the numbers are positions, so
#     moving a run of moves would make the extruder jump. Relative E deltas
#     are position-independent and can be relocated untouched.
#   * Where a run is cut out, a travel to its end point is left behind, so
#     every move that followed still starts from the coordinate it expected.
#   * The run is re-emitted verbatim -- same coordinates, same E, same width
#     -- with its original feedrate restored first, because the wave block
#     will have left a different F in force. Nothing about the wall changes
#     except WHEN it is printed.
# ---------------------------------------------------------------------------
# M-codes that change state but never move the nozzle, so they can sit
# inside a run of travels without ending it.
_PASSIVE_MCODES = frozenset((
    "M73",      # progress / time remaining
    "M117",     # LCD message
    "M204", "M205",   # acceleration, jerk
    "M106", "M107",   # fan
    "M900",     # linear advance
))

WALL_LAST_BEGIN = "; ==== WAVE: OVERHANGING WALL MOVED AFTER THE WAVES ===="
WALL_LAST_END = "; ==== END MOVED WALL ===="


def _overhanging_wall_runs(layer, support, tol, lines):
    """Contiguous runs of wall moves to relocate after the waves.

    A wall loop is grouped FIRST and judged second. Judging each move on
    its own looked right and printed badly: on a loop that is partly over
    air and partly over solid material, only the hanging moves were lifted
    out, so the loop came apart into pieces and the nozzle went back and
    forth across the layer finishing it off. The owner saw exactly that --
    "it kind of goes back through the layer stopping at random points".

    So a run is every wall move that follows on from the last without some
    other motion in between, and the whole run moves if enough of it hangs
    in open air. A few supported millimetres travelling with it costs
    nothing: they print on solid ground either way.
    """
    grown = support.buffer(max(0.0, tol)) if support is not None else None
    if grown is None or grown.is_empty:
        return []
    walls = sorted(layer.get("walls") or [], key=lambda s: s["line"])
    if not walls:
        return []
    for seg in walls:
        if not seg.get("relative_e", True):
            return []                      # absolute E: never relocate

    # 1. group into runs of consecutive wall moves
    runs = []
    current = [walls[0]]
    for seg in walls[1:]:
        previous = current[-1]
        between = lines[previous["line"] + 1:seg["line"]]
        if not any(_MOVE_CODE.match(t.strip()) or t.strip().startswith("G92")
                   for t in between):
            current.append(seg)
        else:
            runs.append(current)
            current = [seg]
    runs.append(current)

    # 2. keep the runs that are meaningfully in the air
    out = []
    for run in runs:
        hanging = 0.0
        total = 0.0
        for seg in run:
            geom = seg["geom"]
            if geom.length <= 1e-9:
                continue
            total += geom.length
            try:
                hanging += geom.difference(grown).length
            except Exception:
                return []
        if total > 0.0 and hanging > 0.25 * total:
            out.append(run)
    return out


def _last_xy(block):
    """The XY the toolhead is left at by a block of emitted G-code lines."""
    x = y = None
    for line in block:
        s = line.strip()
        if not _MOVE_CODE.match(s):
            continue
        words = _gwords(s)
        x = words.get("X", x)
        y = words.get("Y", y)
    return None if x is None or y is None else (x, y)



def _e_of(line):
    """The E word of a move line, or None."""
    stripped = line.strip()
    if not _MOVE_CODE.match(stripped):
        return None
    return _gwords(stripped).get("E")


def _scaffolding_span(lines, first, last):
    """Widen a wall run to include the retract/wipe/travel that belong to it.

    Relocating only the extruding moves left the feature's plumbing behind:
    the travel in, the unretract before it, and the retract and WIPE block
    after it. Six walls moved meant six of those stranded at the old
    positions, chained together -- travel, retract, wipe, travel, unretract,
    with nothing printed. That is the nozzle "going back through the layer
    stopping at random points"; the owner spotted it in the tail of the
    layer after the previous fix.

    Returns (start, end) line indices, or the original pair when the
    surrounding lines cannot be absorbed safely. The absorbed extrusion
    must net to zero -- an unretract matched by its retract -- or the span
    is refused, because taking half of a retract pair would shift every E
    value after it.
    """
    start = first
    while start > 0:
        candidate = lines[start - 1].strip()
        if not candidate or candidate.startswith((";TYPE:", ";WIDTH:")):
            start -= 1
            continue
        if candidate.startswith(";"):
            # ;WIPE_END closes the PREVIOUS feature: stop before it.
            if candidate.startswith(";WIPE"):
                break
            start -= 1
            continue
        if candidate.split()[0] in _PASSIVE_MCODES:
            start -= 1
            continue
        move = _MOVE_CODE.match(candidate)
        if move is None:
            break
        words = _gwords(candidate)
        has_xy = "X" in words or "Y" in words
        e = words.get("E")
        if has_xy and e is None:          # the travel in
            start -= 1
            continue
        if not has_xy and (e is None or e > 0):   # feedrate or unretract
            start -= 1
            continue
        break

    end = last
    saw_wipe = False
    while end + 1 < len(lines):
        candidate = lines[end + 1].strip()
        if not candidate:
            end += 1
            continue
        if candidate.startswith(";WIPE_START"):
            saw_wipe = True
            end += 1
            continue
        if candidate.startswith(";WIPE_END"):
            end += 1
            break
        if candidate.startswith(";"):
            if candidate.startswith(";TYPE:"):
                break                      # the next feature begins
            end += 1
            continue
        if candidate.split()[0] in _PASSIVE_MCODES:
            end += 1
            continue
        move = _MOVE_CODE.match(candidate)
        if move is None:
            break
        words = _gwords(candidate)
        e = words.get("E")
        if e is not None and e < 0:        # the retract
            end += 1
            continue
        if saw_wipe and e is None:         # wipe moves carry no E
            end += 1
            continue
        if e is None and "X" not in words and "Y" not in words:
            end += 1                       # bare feedrate
            continue
        break

    net = 0.0
    for i in list(range(start, first)) + list(range(last + 1, end + 1)):
        e = _e_of(lines[i])
        if e:
            net += e
    if abs(net) > 1e-6:
        return first, last
    return start, end


def _relocate_wall_runs(lines, runs, replacements, travel_f,
                        after_line, resume_xy):
    """Cut `runs` out of the layer and return the text to re-emit after it.

    The block is inserted directly after the layer's last wave output and
    ends by travelling back to `resume_xy`, which is where that wave output
    left the toolhead -- so everything after this point in the file, wipes
    and retracts included, still starts from the coordinate it expects.

    Returns (lines_to_insert, moves_relocated). `replacements` is updated in
    place: the first line of each run becomes a travel to where that run
    ended, and the rest of the run is dropped.
    """
    moved = []
    count = 0
    # Order the runs into a route instead of emitting them in file order.
    # A wall that is split into six pieces, printed where the slicer
    # happened to put them, has the nozzle crossing the part between each
    # one -- which is what "it goes back through the layer stopping at
    # random points" looks like. Greedy nearest-neighbour from where the
    # waves left off is enough: these are a handful of pieces, not a
    # travelling-salesman problem.
    here = resume_xy
    ordered = []
    remaining = list(runs)
    while remaining:
        best = min(remaining, key=lambda r: math.hypot(r[0]["a"][0] - here[0],
                                                       r[0]["a"][1] - here[1]))
        remaining.remove(best)
        ordered.append(best)
        here = best[-1]["b"]
    for run in ordered:
        first, last = run[0]["line"], run[-1]["line"]
        widened = _scaffolding_span(lines, first, last)
        if widened == (first, last):
            # Its travel-in and retract/wipe could not be taken with it.
            # Moving the extrusion alone strands that plumbing at the old
            # position -- a retract, a wipe and a travel with nothing
            # printed, which is the stop-start motion this was supposed to
            # remove. Whole feature or nothing.
            continue
        first, last = widened
        if first <= after_line <= last:
            continue                        # would move the waves themselves
        if any(i in replacements for i in range(first, last + 1)):
            continue                        # overlaps a wave rewrite
        start = run[0]["a"]
        end = run[-1]["b"]
        if first <= after_line <= last:
            continue
        feed = run[0].get("feed")
        moved.append(WALL_LAST_BEGIN + "\n")
        moved.append(f"G0 F{travel_f:.0f} X{start[0]:.3f} Y{start[1]:.3f}\n")
        if feed:
            # F is modal and the wave block left its own speed in force, so
            # without this the wall would print at the wave speed.
            moved.append(f"G1 F{feed:.0f}\n")
        for i in range(first, last + 1):
            moved.append(lines[i])
            count += 1
        moved.append(WALL_LAST_END + "\n")
        # Leave the toolhead where the removed run left it, so the moves that
        # followed it in the original file are unaffected.
        replacements[first] = [
            "; wave-overhangs moved this overhanging wall after the waves\n",
            f"G0 F{travel_f:.0f} X{end[0]:.3f} Y{end[1]:.3f}\n"]
        for i in range(first + 1, last + 1):
            replacements[i] = []
    if moved:
        moved.append(
            f"G0 F{travel_f:.0f} X{resume_xy[0]:.3f} Y{resume_xy[1]:.3f}\n")
    return moved, count



def _collapse_wave_travels(lines):
    """Remove the pointless travels left where covered bridge moves were cut.

    Every original bridge move the waves covered is replaced by a comment and
    a travel to where that move started. One after another, that is the
    nozzle tracing the whole original bridge raster in mid-air after the
    waves are already down -- 63 travels and 11 extrusions on the Cube
    export, which the owner saw as the nozzle "scanning its way across the
    print". It also looks alarming in preview and costs real print time.

    Consecutive travels are redundant: G0 states absolute X and Y, so only
    the last one in a run has any effect. This keeps that last one -- folding
    any Z from the ones it replaces into it, since Z IS cumulative in effect
    -- and drops the rest.

    Deliberately narrow: a run is only touched when it contains one of this
    plugin's own comments, so G-code Orca wrote (wipes, retract sequences,
    anything with its own meaning) is never rewritten.
    """
    out = []
    run = []            # buffered consecutive travel lines + our comments
    run_is_ours = False
    replaced_prefix = "; wave-overhangs replaced covered bridge move "

    def flush():
        nonlocal run, run_is_ours
        if not run:
            return
        travels = [ln for ln in run if _MOVE_CODE.match(ln.strip())]
        others = [ln for ln in run if not _MOVE_CODE.match(ln.strip())]
        if run_is_ours:
            # 390 of these on one layer of t3. They are worth keeping -- they
            # say which original move a wave covered -- but one line each is
            # noise, so a run becomes a single line naming the range.
            numbers = []
            kept_others = []
            for line in others:
                if line.strip().startswith(replaced_prefix):
                    numbers.append(line.strip()[len(replaced_prefix):])
                else:
                    kept_others.append(line)
            if len(numbers) > 1:
                kept_others.insert(
                    0, f"; wave-overhangs replaced covered bridge moves "
                       f"{numbers[0]}-{numbers[-1]} ({len(numbers)})\n")
            elif numbers:
                kept_others.insert(0, replaced_prefix + numbers[0] + "\n")
            others = kept_others
        if run_is_ours and len(travels) > 1:
            # Keep the last travel; carry a Z from the ones being dropped.
            last = travels[-1]
            if " Z" not in last and "Z" not in _gwords(last.strip()):
                for earlier in reversed(travels[:-1]):
                    z = _gwords(earlier.strip()).get("Z")
                    if z is not None:
                        last = last.rstrip("\r\n") + f" Z{z:.3f}\n"
                        break
            out.extend(others)
            out.append(last)
        else:
            out.extend(run)
        run = []
        run_is_ours = False

    for line in lines:
        stripped = line.strip()
        move = _MOVE_CODE.match(stripped)
        if move and int(move.group(1)) == 0 and "E" not in _gwords(stripped):
            run.append(line)
            continue
        # A bare feedrate -- "G1 F420" with no coordinates and no E. NOT an
        # extruding move that happens to carry an F, which also starts with
        # "G1 F" and must never be dropped.
        bare_feed = False
        if stripped.startswith("G1"):
            words = _gwords(stripped)
            bare_feed = ("X" not in words and "Y" not in words
                         and "Z" not in words and "E" not in words)
        # M-codes that do not move the toolhead must not break a run of
        # travels. Orca sprinkles M73 progress lines through the file, and
        # letting those end a run is why the collapse added in 0.0.41 did
        # almost nothing on a real export: 59 travels survived on t3,
        # 729 mm of jumping to print 250 mm.
        passive = stripped.split()[0] if stripped else ""
        passive_mcode = passive in _PASSIVE_MCODES
        # One of our own comments may also OPEN a run: the first comment of
        # a replaced-move sequence arrives before its travel, and leaving it
        # outside meant it escaped the folding.
        opens_run = stripped.startswith("; wave-overhangs")
        if (run or opens_run) and (not stripped or stripped.startswith(";")
                                   or bare_feed or passive_mcode):
            # Comments and bare feedrate settings sit inside a travel run
            # without ending it.
            if stripped.startswith("; wave-overhangs"):
                run_is_ours = True
            if bare_feed and run_is_ours:
                # Only matters before an extrusion, and the extrusion that
                # follows states its own feedrate.
                continue
            run.append(line)
            continue
        flush()
        out.append(line)
    flush()
    return out




# ---------------------------------------------------------------------------
#  Where a wave is actually WORTH it
#
#  Waves exist for extrusion that has nothing under it and nothing to span
#  between -- printing into thin air. Orca marks several things as "bridge"
#  that are nothing of the sort:
#
#   * `;TYPE:Internal Bridge` is the solid layer laid over SPARSE INFILL, at
#     the top of a part. It is anchored every few millimetres by the infill
#     below it. A straight bridge is the right tool and is faster and
#     cleaner; a wave there is pure cost.
#   * A small unsupported pocket -- the owner's "little divots on the
#     underside" -- is anchored all the way round. Anything a straight
#     bridge can cross should be left to a straight bridge.
#
#  Both are decided here rather than in the geometry: by the time a region
#  has been turned into wavefronts the decision has already cost most of
#  what it was going to cost.
# ---------------------------------------------------------------------------
AUTO_STRAIGHT_BRIDGE_SPAN = 10.0     # mm a plain bridge is assumed to manage


def _straight_bridge_span(cfg):
    raw = cfg.get("straight_bridge_span", "auto")
    if _is_auto(raw):
        return AUTO_STRAIGHT_BRIDGE_SPAN
    try:
        return max(0.0, float(raw))
    except (TypeError, ValueError):
        return AUTO_STRAIGHT_BRIDGE_SPAN


def _straight_bridge_would_do(region, support, span):
    """True when a plain bridge spans every part of `region`.

    `span` is the longest unsupported straight run a bridge is assumed to
    manage. Growing the supported area by half of that and asking whether it
    now covers the region is exactly the question "is every point of this
    region within half a bridge of something solid" -- one buffer and one
    predicate, rather than measuring a distance transform.
    """
    if span <= 0.0 or region is None or region.is_empty:
        return False
    if support is None or support.is_empty:
        return False
    try:
        return support.buffer(span * 0.5).covers(region)
    except Exception:  # pragma: no cover - geometry never breaks an export
        return False



def _gcode_wave_rewrite(text, cfg):
    """Replace covered bridge extrusion and retain every uncovered fragment.

    Planning, insertion and subtraction happen in this one in-memory operation.
    Any exception returns the original text unchanged.
    """
    if WAVE_STAMP_PREFIX in text:
        return text, {"replaced_sections": 0, "wave_layers": 0,
                      "already_processed": True}
    lines = text.splitlines(keepends=True)
    started = time.time()
    deadline = _budget_deadline(cfg, started, text)
    timings = {}
    # Everything Wave can inherit from the user's own Orca profile, read once.
    profile = _orca_profile(text)
    auto_notes = {}
    try:
        interesting = _bridge_layer_indices(lines)
        if not interesting:
            # No Bridge section anywhere: nothing to do, and no reason to
            # build a single piece of geometry. This is the cheap path for
            # the great majority of exports.
            return text, {"replaced_sections": 0, "wave_layers": 0,
                          "removed_moves": 0, "kept_fragments": 0,
                          "tiny_fragments_dropped": 0,
                          "short_wave_paths_dropped": 0,
                          "wall_bounded_sections": 0, "arc_moves": 0,
                          "gap_fills": 0,
                          "seconds": round(time.time() - started, 2),
                          "already_processed": False}
        layers = _parse_gcode_geometry(lines, interesting)
        timings["parse_seconds"] = round(time.time() - started, 2)
        planning = time.time()
        replacements = {}
        walls_moved = wall_layers_reordered = adaptive_sections = 0
        internal_skipped = bridgeable_skipped = 0
        kept_mm = 0.0
        contour_paths = 0
        keep_uncovered = _as_bool(cfg.get("keep_uncovered_bridge", True))
        bridge_span = _straight_bridge_span(cfg)
        sections_done = wave_layers = removed = kept = dropped = paths_dropped = 0
        wall_sections = 0
        arc_moves = 0
        gap_fills = 0
        # Decided once per file: the export's own arc-fitting setting applies
        # to the whole print, and the Wave width is the same everywhere.
        arc_limits = None
        arc_decided = False
        for li, layer in enumerate(layers):
            if deadline is not None and time.time() > deadline:
                raise _WaveBudgetExceeded(
                    "gave up after %.0fs on layer %d of %d"
                    % (time.time() - started, li + 1, len(layers)))
            if li == 0 or not layer["sections"]:
                continue
            support = _footprint(layers[li - 1]["all"])
            if support.is_empty:
                continue
            walls = _wall_material(layer)
            outline = _layer_outline(layer)
            wall_widths = sorted(s["width"] for s in (layer.get("walls") or []))
            wall_width = (wall_widths[len(wall_widths) // 2]
                          if wall_widths else 0.0)
            layer_changed = False
            last_wave_line = None
            last_wave_xy = None
            for sec in layer["sections"]:
                if (sec.get("type") == "internal bridge"
                        and not _as_bool(cfg.get("wave_internal_bridges",
                                                 False))):
                    # Solid layer over sparse infill: anchored every few mm by
                    # the infill below, so a straight bridge is the right tool.
                    internal_skipped += 1
                    continue
                if deadline is not None and time.time() > deadline:
                    raise _WaveBudgetExceeded(
                        "gave up after %.0fs on layer %d of %d"
                        % (time.time() - started, li + 1, len(layers)))
                if not sec["segments"]:
                    continue
                bridge = _footprint(sec["segments"])
                swcfg = _wave_config(cfg, cfg.get("_lh", 0.2))
                widths = sorted(s["width"] for s in sec["segments"])
                swcfg.line_width = widths[len(widths) // 2]
                # The Wave line width is now known, so every "auto" can be
                # turned into the number it stands for. `rcfg` is the config
                # with no "auto" left in it; use it from here down.
                rcfg = _resolve_autos(cfg, swcfg.line_width, profile)
                auto_notes.update(rcfg.pop("_auto_notes", {}))
                swcfg.line_spacing = rcfg["line_spacing"]
                swcfg.perimeter_overlap = rcfg["perimeter_overlap"]
                swcfg.min_overhang_area = rcfg["min_overhang_area"]
                swcfg.travel_speed = rcfg["travel_speed"]
                swcfg.fan = rcfg["fan"]
                swcfg.propagation_mode = str(
                    cfg.get("propagation_mode", "auto"))
                try:
                    # Clamped: past about 1.5 spacings the closing stops
                    # rounding the crease and starts swallowing whole fronts,
                    # which breaks them into stubs.
                    swcfg.wake_blend = min(
                        1.5, max(0.0, float(cfg.get("wake_blend", 1.0))))
                except (TypeError, ValueError):
                    swcfg.wake_blend = 1.0
                if not arc_decided:
                    arc_limits = _arc_limits(text, cfg, swcfg)
                    arc_decided = True
                # Where a Wave belongs at all: the bridge Orca exported, minus
                # what the layer below holds up. This decision is unchanged.
                core = bridge.difference(support.buffer(swcfg.overhang_tol))
                if core.is_empty or core.area < swcfg.min_overhang_area:
                    continue
                # Anything a plain bridge can cross is left to the plain
                # bridge: faster, cleaner, and what the slicer already
                # planned. Waves are for what cannot be spanned.
                if _straight_bridge_would_do(core, support, bridge_span):
                    bridgeable_skipped += 1
                    continue
                # Now fix that area's *shape*. Clipping fronts to the outline
                # of Orca's bridge lines is what frays the ends, so stretch
                # the area out to the wall the layer actually printed.
                region, wall_bounded = _wall_bounded_region(
                    bridge, walls, outline, wall_width, swcfg, rcfg)
                if wall_bounded:
                    wall_sections += 1
                    unsupported = _backed_parts(
                        region.difference(support.buffer(swcfg.overhang_tol)),
                        core)
                else:
                    unsupported = core
                if unsupported.is_empty or unsupported.area < swcfg.min_overhang_area:
                    continue
                # Iteration cap. "auto" asks how many fronts it actually
                # takes to cross THIS region and adds headroom, so a big
                # overhang is no longer cut off half way and a small one does
                # not carry a cap meant for something else. It is a runaway
                # guard, not a quality dial: it never adds a front that the
                # geometry did not ask for, so it cannot bloat the file.
                if _is_auto(cfg.get("max_iterations")):
                    swcfg.max_iterations = _auto_iterations(
                        support.union(unsupported), swcfg.line_spacing)
                    auto_notes["max_iterations"] = (
                        f"{swcfg.max_iterations} fronts (auto: enough to cross "
                        f"the largest region seen, plus headroom)")
                else:
                    swcfg.max_iterations = int(
                        _float_cfg(cfg, "max_iterations", 400))
                tracks = wc.wave_tracks(support, unsupported, swcfg)
                polylines = _order_wave_tracks(
                    tracks, support, cfg.get("pattern"), cfg.get("start_policy"),
                    cfg.get("component_order", "support"),
                    start_xy=sec["segments"][0]["a"])
                guards = {}
                polylines = [
                    _clean_wave_polyline(
                        p, swcfg.line_width,
                        rcfg["simplify_tolerance"],
                        rcfg["min_wave_segment"],
                        allowed=unsupported, guards=guards,
                        smooth_creases=_as_bool(
                            rcfg.get("smooth_creases", True)))
                    for p in polylines]
                minimum_wave = max(0.0, float(rcfg["min_wave_length"]))
                # A short front is only a problem when it is on its own in mid
                # air. One that touches a rung already printed is anchored, and
                # dropping it is what leaves a sliver unfilled in a corner.
                full_length = [p for p in polylines
                               if len(p) >= 2
                               and _polyline_length(p) >= minimum_wave]
                anchor = None
                if full_length and minimum_wave > 0.0:
                    anchor = shapely.ops.unary_union([
                        shapely.geometry.LineString(p).buffer(
                            swcfg.line_width * 0.5, cap_style=2, join_style=2)
                        for p in full_length])
                kept_polylines = []
                for polyline in polylines:
                    if len(polyline) < 2:
                        paths_dropped += 1
                        continue
                    length = _polyline_length(polyline)
                    keep = length >= minimum_wave
                    if not keep and length >= swcfg.line_width * 0.5 and \
                            anchor is not None:
                        # Anchored to a full-length rung, so it is not a speck
                        # printed into thin air. Measured against every rung,
                        # not the ones kept so far, so print order cannot
                        # change which fronts survive.
                        keep = shapely.geometry.LineString(polyline).distance(
                            anchor) <= swcfg.line_spacing * 1.3
                    if keep:
                        kept_polylines.append(polyline)
                    else:
                        paths_dropped += 1
                polylines = kept_polylines
                if not polylines:
                    continue
                polylines = _snap_wave_polylines(
                    polylines, unsupported, support, swcfg, rcfg)
                if not polylines:
                    continue
                filled = _gap_fill_fronts(unsupported, polylines, swcfg, rcfg)
                if filled:
                    polylines = polylines + filled
                    gap_fills += len(filled)
                # Last, so the visible edge of the waved area is a bead that
                # follows the wall rather than the tail of a front that was
                # aimed somewhere else.
                rim_paths = _contour_finish_paths(
                    unsupported, support, swcfg, rcfg, polylines,
                    walls=layer.get("walls"))
                if rim_paths:
                    polylines = polylines + rim_paths
                    contour_paths += len(rim_paths)
                emit_polylines = _inset_wave_polylines(
                    polylines, unsupported, support, swcfg, rcfg)
                if not emit_polylines:
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
                sec_kept_mm = 0.0
                # Every move written below carries an explicit feedrate. The
                # Wave block ends with the (deliberately very slow) Wave print
                # speed in force, and G-code feedrates are modal, so a move
                # emitted without an F would crawl at the Wave speed and the
                # slicer would report it as minutes of travel.
                travel_f = max(1.0, float(swcfg.travel_speed) * 60.0)
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
                        if not keep_uncovered:
                            # The owner's case: once the waves are down, a
                            # trip back across the layer to lay 0.2 mm of
                            # original bridge is not worth the travel or the
                            # blob at either end. 21 such moves totalling
                            # 4.2 mm on one layer of t3.
                            sec_dropped += 1
                            continue
                        min_fragment = max(
                            0.0, float(cfg.get("min_bridge_fragment", 0.5)))
                        if part.length < swcfg.line_width * min_fragment:
                            sec_dropped += 1
                            continue
                        sec_kept_mm += part.length
                        ax, ay = coords[0]
                        out.append(f"G0 F{travel_f:.0f} X{ax:.3f} Y{ay:.3f}\n")
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
                            seg_f = seg.get("feed")
                            if pi == 1 and seg_f:
                                # First move after the travel above, so the
                                # original bridge speed has to be restated.
                                out.append(f"G1 F{seg_f:.0f} X{bx:.3f} "
                                           f"Y{by:.3f} E{e_word:.5f}\n")
                            else:
                                out.append(f"G1 X{bx:.3f} Y{by:.3f} "
                                           f"E{e_word:.5f}\n")
                        sec_kept += 1
                    # Covered or discarded pieces may leave the nozzle at a
                    # Wave endpoint. Restore the original segment's endpoint
                    # before the next original G1 move, or that move would
                    # extrude a sharp diagonal through the new Wave field.
                    out.append(f"G0 F{travel_f:.0f} X{seg['b'][0]:.3f} "
                               f"Y{seg['b'][1]:.3f}\n")
                    seg_f = seg.get("feed")
                    if seg_f:
                        # Hand the original feedrate back before the untouched
                        # moves resume. They usually carry no F of their own,
                        # so without this they inherit the Wave print speed.
                        out.append(f"G1 F{seg_f:.0f}\n")
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
                wave_speed = None
                if _wants_orca_print_speed(cfg.get("print_speed")):
                    wave_speed = _section_print_speed(sec, swcfg.print_speed)
                flow_scales = _adaptive_flow_scales(
                    emit_polylines, unsupported, swcfg, rcfg)
                block = _emit_wave_gcode(
                    emit_polylines, actual_z, swcfg, rcfg,
                    unsupported=unsupported, support=support,
                    restore_fan=sec.get("fan"), arcs=arc_limits,
                    print_speed=wave_speed, flow_scales=flow_scales)
                if flow_scales:
                    adaptive_sections += 1
                arc_moves += sum(1 for line in block
                                 if line.startswith(("G2 ", "G3 ")))
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
                # Where this section's wave output ends, so an overhanging
                # wall can be appended straight after it and then hand the
                # toolhead back exactly where the rest of the file expects.
                last_wave_line = max(sec_replacements)
                last_wave_xy = _last_xy(replacements[last_wave_line])
                removed += sec_removed
                kept += sec_kept
                kept_mm += sec_kept_mm
                dropped += sec_dropped
                sections_done += 1
                layer_changed = True
            if layer_changed:
                wave_layers += 1
                # Waves first, overhanging wall last. See the comment on
                # _overhanging_wall_runs: printing the wall into open air
                # before the waves exist is what makes it droop.
                if (_as_bool(cfg.get("wall_last", True))
                        and last_wave_line is not None
                        and last_wave_xy is not None):
                    runs = _overhanging_wall_runs(
                        layer, support, _float_cfg(cfg, "overhang_tol", 0.05),
                        lines)
                    moved, n = _relocate_wall_runs(
                        lines, runs, replacements,
                        max(1.0, float(swcfg.travel_speed) * 60.0),
                        last_wave_line, last_wave_xy)
                    if moved:
                        replacements[last_wave_line].extend(moved)
                        walls_moved += n
                        wall_layers_reordered += 1
        if not sections_done:
            return text, {"replaced_sections": 0, "wave_layers": 0,
                          "removed_moves": 0, "kept_fragments": 0,
                          "tiny_fragments_dropped": 0,
                          "short_wave_paths_dropped": paths_dropped,
                          "wall_bounded_sections": wall_sections,
                          "arc_moves": 0, "gap_fills": 0,
                          "internal_bridges_left_alone": internal_skipped,
                          "bridgeable_regions_left_alone": bridgeable_skipped,
                          "seconds": round(time.time() - started, 2)}
        out = []
        for i, line in enumerate(lines):
            if i in replacements:
                out.extend(replacements[i])
            else:
                out.append(line)
        out = _collapse_wave_travels(out)
        timings["plan_seconds"] = round(time.time() - planning, 2)
        timings["seconds"] = round(time.time() - started, 2)
        timings["geometry_layers"] = len(interesting)
        timings["layers_scanned"] = len(layers)
        return WAVE_STAMP + "".join(out), dict(timings, **{
            "replaced_sections": sections_done, "wave_layers": wave_layers,
            "removed_moves": removed, "kept_fragments": kept,
            "kept_fragment_mm": round(kept_mm, 2),
            "contour_finish_paths": contour_paths,
            "tiny_fragments_dropped": dropped,
            "short_wave_paths_dropped": paths_dropped,
            "wall_bounded_sections": wall_sections,
            "arc_moves": arc_moves,
            "gap_fills": gap_fills,
            "walls_moved": walls_moved,
            "internal_bridges_left_alone": internal_skipped,
            "bridgeable_regions_left_alone": bridgeable_skipped,
            "adaptive_flow_sections": adaptive_sections,
            "wall_layers_reordered": wall_layers_reordered,
            # What every "auto" resolved to on this print. Echoing the word
            # "auto" back at the user tells them nothing; the number does.
            "auto": "; ".join(f"{k}={v}" for k, v in sorted(auto_notes.items()))
                    or "none (every setting has an explicit value)",
            "already_processed": False})
    except _WaveBudgetExceeded as e:
        # Hand back exactly what Orca gave us. A partial pass would make the
        # output depend on how fast the machine is, and the same file would
        # slice differently twice; an untouched file is at least honest.
        return text, {"replaced_sections": 0, "wave_layers": 0,
                      "short_wave_paths_dropped": 0,
                      "wall_bounded_sections": 0,
                      "arc_moves": 0,
                      "gap_fills": 0,
                      "seconds": round(time.time() - started, 2),
                      "timed_out": True,
                      "error": f"time budget exceeded: {e}"}
    except Exception as e:
        return text, {"replaced_sections": 0, "wave_layers": 0,
                      "short_wave_paths_dropped": 0,
                      "wall_bounded_sections": 0,
                      "arc_moves": 0,
                      "gap_fills": 0,
                      "seconds": round(time.time() - started, 2),
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
        # Annotated, so the JSON panel in Orca explains itself. The notes are
        # ignored on the way back in (see _cfg).
        return annotated_defaults()

    def migrate_config_if_needed(self):
        """Orca's documented schema-migration hook.

        A config saved by an older release is missing every setting added
        since, and the Config panel shows that saved copy. Merge the new keys
        in, keep the user's values, write it back.
        """
        merged = _migrate_config(self, annotated_defaults(), self.get_name())
        _MIGRATED.add(self.get_name())
        try:
            version = self.get_config_version()
        except BaseException:
            version = ""
        return version, (merged if merged is not None else annotated_defaults())

    def execute(self, ctx):
        _log_loaded_once()
        # Belt and braces: the host is documented to call
        # migrate_config_if_needed(), but *when* is shown only by example and
        # is not verified on a real build. Doing it here too means one slice
        # is always enough to make new settings appear in the panel.
        _migrate_once(self, annotated_defaults(), self.get_name())
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
            st["last_timed_out"] = bool(log.get("timed_out"))
            st["last_timeout_seconds"] = (log.get("seconds")
                                          if log.get("timed_out") else None)
            st["last_walls_moved"] = log.get("walls_moved", 0)
            st["last_wall_layers"] = log.get("wall_layers_reordered", 0)
            # What every "auto" resolved to on this export, so Check setup can
            # show real numbers instead of the word "auto".
            if log.get("auto"):
                st["last_auto"] = log["auto"]
            _save_state(st)

            log["seconds"] = round(time.time() - log["started"], 3)
            _write_log(log)
            if log.get("timed_out"):
                # Say so plainly. Silently doing nothing after a long wait is
                # exactly the behaviour that looks like a broken export.
                return orca.ExecutionResult.success(
                    f"Wave Overhangs: gave up after "
                    f"{log.get('seconds', 0):.0f}s (time_budget) and left the "
                    f"G-code unchanged; raise time_budget or set "
                    f"enabled=false")
            if n:
                return orca.ExecutionResult.success(
                    f"Wave Overhangs: replaced covered bridge extrusion on "
                    f"{n} layer(s); uncovered fragments were retained")
            return orca.ExecutionResult.success(
                "Wave Overhangs: no unsupported Bridge sections were replaced")

        return orca.ExecutionResult.success()


_CHECK_DEFAULTS = {
    "_READ_ME": (
        "Keys starting with _ are notes, not settings -- the plugin ignores "
        "them. This item reports whether Wave Overhangs is working and then "
        "explains every setting of the main Wave Overhangs capability."
    ),
    "_settings_guide": (
        "Show the full explanation of every Wave Overhangs setting after the "
        "diagnostics. Set false once you know them and you will get just the "
        "short status report."
    ),
    "settings_guide": True,
}


class WaveOverhangsCheck(orca.script.ScriptPluginCapabilityBase):
    def get_name(self):
        return "Wave Overhangs - Settings guide & check"

    def get_default_config(self):
        return dict(_CHECK_DEFAULTS)

    def migrate_config_if_needed(self):
        merged = _migrate_config(self, dict(_CHECK_DEFAULTS), self.get_name())
        _MIGRATED.add(self.get_name())
        try:
            version = self.get_config_version()
        except BaseException:
            version = ""
        return version, (merged if merged is not None else dict(_CHECK_DEFAULTS))

    def _want_guide(self):
        try:
            raw = json.loads(self.get_config() or "{}")
        except (AttributeError, TypeError, ValueError):
            return True
        value = raw.get("settings_guide", True)
        if isinstance(value, str):
            return value.strip().lower() not in ("false", "0", "no", "off")
        return bool(value)

    def execute(self):
        _log_loaded_once()
        _migrate_once(self, dict(_CHECK_DEFAULTS), self.get_name())
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

        cfg = _cfg(self)
        budget = cfg.get("time_budget", 30.0)
        lines.append(f"time budget: {budget}s"
                     if budget else "time budget: off")
        arc_setting = cfg.get("arc_fitting", False)
        if arc_setting is False or str(arc_setting).lower() in ("0", "false"):
            lines.append("arc moves: off (default)")
        else:
            lines.append(f"arc moves: {arc_setting} -- WARNING")
            lines.append("  OrcaSlicer has an open bug where G-code produced")
            lines.append("  by a post-processing script that contains G2/G3")
            lines.append("  arcs can corrupt or crash the preview/export")
            lines.append("  (OrcaSlicer issue #7433, still open). If your")
            lines.append("  export hangs on 'exporting' or crashes, set")
            lines.append("  arc_fitting back to false.")

        st = _load_state()
        lines.append("")
        lines.append("--- what the last export actually did ---")
        if st.get("last_timed_out"):
            lines.append("*** THE LAST EXPORT RAN OUT OF TIME ***")
            lines.append(f"It gave up after "
                         f"{st.get('last_timeout_seconds') or '?'}s and handed")
            lines.append("your G-code back exactly as Orca wrote it, so NO")
            lines.append("waves were added. That is what 'the plugin did")
            lines.append("nothing' looks like. Raise time_budget (it accepts")
            lines.append("a number of seconds), or set it to 0 for no limit.")
            lines.append("")
        if st.get("last_splice_at"):
            lines.append(f"G-code step: ran, {st.get('last_splice_layers', 0)} "
                         "layer(s) replaced")
            lines.append("Covered bridge extrusion was removed; uncovered")
            lines.append("fragments were retained.")
            if st.get("last_walls_moved"):
                lines.append(f"Overhanging wall: {st['last_walls_moved']} move(s) "
                             f"on {st.get('last_wall_layers', 0)} layer(s)")
                lines.append("were moved to print AFTER the waves.")
            elif _as_bool(cfg.get("wall_last", True)):
                lines.append("Overhanging wall: none needed moving (no wall")
                lines.append("hung in open air on a waved layer).")
            if st.get("last_auto"):
                lines.append("")
                lines.append("What each \"auto\" setting worked out to:")
                for item in str(st["last_auto"]).split("; "):
                    lines.append(f"  {item}")
        else:
            lines.append("No export recorded yet -- this plugin has never")
            lines.append("been handed a file to work on.")
            lines.append("")
            lines.append("If you DID slice and nothing changed, the usual")
            lines.append("cause is the selection, not the plugin:")
            lines.append("  Others -> Slicing Pipeline Plugin is ONE field,")
            lines.append("  and both Wave Overhangs and Unlayered Infill want")
            lines.append("  it. If Unlayered Infill is selected there, Wave")
            lines.append("  never runs, and the exported file carries the")
            lines.append("  Unlayered stamp and no wave blocks.")
            lines.append("  Check the top of your exported .gcode: a line")
            lines.append("  reading '; wave-overhangs v...' means this ran.")
            lines.append("  Select Wave Overhangs there and export again.")
        backup = _settings_backup()
        lines.append("")
        lines.append("--- your settings are backed up ---")
        if backup:
            changed = {k: v for k, v in backup["values"].items()
                       if k in _DEFAULTS and _DEFAULTS[k] != v}
            lines.append(f"{len(backup['values'])} setting(s) remembered, "
                         f"saved by v{backup.get('version', '?')} on "
                         f"{backup.get('saved_at', '?')}.")
            if changed:
                lines.append("Non-default values held: "
                             + ", ".join(f"{k}={json.dumps(v)}"
                                         for k, v in sorted(changed.items())))
            else:
                lines.append("All of them are at this build's defaults.")
            lines.append("If the Config panel ever comes back empty or reset,")
            lines.append('set restore_backup to true and slice once; these')
            lines.append("values go back and the flag returns to false.")
        else:
            lines.append("Nothing remembered yet -- a backup is taken every")
            lines.append("time this plugin runs.")
        lines.append("")
        lines.append("--- not seeing all the settings? ---")
        lines.append(f"This build has {len(_DEFAULTS)} settings, all listed")
        lines.append("below. If the Config panel for \"Wave Overhangs\" shows")
        lines.append("fewer, nothing is broken: OrcaSlicer keeps each")
        lines.append("capability's settings in one global file and shows you")
        lines.append("that saved copy, so settings added by a later version")
        lines.append("are absent from a copy an older version wrote. The")
        lines.append("plugin still uses them at their defaults meanwhile.")
        lines.append("Since v0.0.33 it repairs this itself: your saved copy is")
        lines.append("merged with this build's settings, keeping every value")
        lines.append("you set. Reopen the Config tab after this check.")
        lines.append("If they are still missing, press \"Restore defaults\" in")
        lines.append("the Config tab -- that deletes the saved copy, so the")
        lines.append("panel falls back to this build's full defaults.")
        lines.append("Still short? The installed FILE is an older one: run")
        lines.append("Orca-Plugins.bat again, then fully quit and reopen Orca.")
        lines.append("")
        lines.append("--- log ---")
        lines.append("Routine diagnostics are written without approval prompts to:")
        lines.append(f"  {log_path()}")
        lines.append("")
        lines.append("--- what changed recently ---")
        lines.extend(CHANGELOG_RECENT.splitlines())
        lines.append("")
        lines.append("Full history: plugins/wave-overhangs/CHANGELOG.md in the repo.")
        if self._want_guide():
            lines.append("")
            lines.extend(settings_guide_lines(cfg))
            lines.append("To hide this guide, set settings_guide to false in")
            lines.append("this item's own settings.")
        return orca.ExecutionResult.success("\n".join(lines))


# ---------------------------------------------------------------------------
#  Logging -- shared with the other plugins in this repo.
#
#  Default logs live in Orca's plugin storage folder. Orca allows writes there
#  without prompting, so normal slicing should not ask the user to authorize a
#  log or state-file write. ORCA_PLUGIN_LOG_DIR remains as an explicit debug
#  override for local tests or a user who deliberately wants an external log.
# ---------------------------------------------------------------------------
LOG_NAME = "orca-plugins.log"
LOG_MAX_BYTES = 1000000          # roll over at ~1 MB so it cannot grow forever


def log_path():
    """Default no-prompt plugin-storage log path."""
    override = os.environ.get("ORCA_PLUGIN_LOG_DIR")
    if override:
        return os.path.join(override, LOG_NAME)
    return os.path.join(_plugin_storage_dir(), LOG_NAME)


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
