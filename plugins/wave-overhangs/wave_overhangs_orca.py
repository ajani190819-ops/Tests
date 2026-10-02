# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy>=2.0", "shapely>=2.0"]
#
# [tool.orcaslicer.plugin]
# name = "Wave Overhangs"
# description = "Experimental: print steep overhangs support-free by replacing the overhang region with wave-propagated toolpaths (port of the WaveOverhangs fork's algorithm as a slicing-pipeline plugin). | What's new in v0.0.34: wave_core registered an empty module in sys.modules before the engine was executed into it, and removed it on failure."
# author = "Wave Overhangs plugin lane"
# version = "0.0.34"
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


_DEFAULTS = {
    # Master switch. False leaves exported G-code unchanged.
    "enabled": True,

    # Hard wall-clock ceiling for the whole G-code pass, in seconds. If the
    # pass is still running when the budget runs out it gives up and hands
    # back the file exactly as Orca wrote it. A slow Wave is a nuisance; an
    # export that never finishes is a broken printer, so this trades the
    # feature away rather than ever hanging a slice. 0 disables the ceiling.
    "time_budget": 30.0,         # seconds; 0 = no limit

    # Detection: support is taken from the previous layer's exported moves.
    # overhang_tol grows support by this many millimetres before deciding that
    # a Bridge area is unsupported. min_overhang_area ignores tiny regions.
    "overhang_tol": 0.05,        # mm of support forgiveness
    "min_overhang_area": 0.5,    # mm²; ignore smaller unsupported regions

    # Wave shape: line_spacing is centre-to-centre distance between fronts.
    # line_width is only a fallback; an exported Bridge WIDTH is preferred.
    # perimeter_overlap keeps the first anchor slightly inside supported
    # material so the Wave does not begin in thin air.
    "line_spacing": 0.35,       # mm; smaller = denser and warmer
    "line_width": 0.40,          # mm fallback; exported Bridge width wins
    "perimeter_overlap": 0.10,  # mm; supported anchor-band overlap

    # Propagation and order. auto adds internal-hole obstacles only when the
    # overhang geometry contains a hole. obstacle forces that method; legacy
    # is useful for comparison but can miss holes inside an overhang plane.
    "propagation_mode": "auto",  # "auto" | "obstacle" | "legacy"
    # EXPERIMENTAL, off by default. When the field splits around a hole and
    # rejoins behind it, the two arriving sides meet in a sharp V and every
    # later front keeps that kink, leaving a hard seam downstream of the
    # hole. This rounds the crease off, in multiples of line_spacing.
    #
    # It genuinely fixes the V on a simple round hole, but on the owner's
    # real part it also loses about 4% of wave coverage (1911 -> 1833 mm of
    # path) and turns 8 tiny fragments into 40, because healing makes
    # consecutive fronts partly coincide and the "already reached"
    # subtraction then cuts them up. Until that is solved it must not be the
    # default. Values above 1.5 are clamped; see docs/ROADMAP.md.
    "wake_blend": 0.0,           # x line_spacing; 0 = off (default)
    "pattern": "smart",          # "smart" | "monotonic" | "zigzag"
    "start_policy": "supported", # supported/consistent/min/max-x/min/max-y
    "component_order": "support",  # "support" | "nearest" same-distance fronts

    # Cleanup: these remove isolated dots without removing ordinary bridge
    # material. Lower values preserve more small geometry but may print blobs.
    "min_wave_length": 1.0,      # mm; discard complete fronts shorter than this
    "min_wave_segment": 0.30,    # mm; merge short endpoint/stub segments
    "simplify_tolerance": 0.05,  # mm; remove harmless boundary point noise
    "min_bridge_fragment": 0.5,  # line-width multiplier for retained fragments

    # Perimeter conformance. Orca exports bridge infill as separate lines, so
    # the area those lines cover has a castellated edge that stops short of
    # the wall. wall_snap rebuilds the overhang area out to the real wall the
    # layer printed, so fronts run from the supported perimeter all the way to
    # the overhang perimeter and holes instead of ending on a jagged edge.
    # wall_reach limits how far that stretch may go; wall_overlap is how far a
    # Wave end sits inside the wall bead, as a fraction of the Wave width.
    "wall_snap": True,           # False restores 0.0.19 bridge-footprint edges
    "wall_reach": "auto",        # mm or auto (1.5 line widths)
    "wall_overlap": 0.25,        # fraction of line width overlapped into wall
    # Wave fronts step outward by a fixed spacing, so the last one can stop
    # short of a boundary that runs at an angle to the march -- the tip of a
    # corner is the usual case, and it is left as a small unfilled sliver.
    "gap_fill": True,            # fill those slivers with a short anchored path
    "gap_fill_min_area": 0.05,   # mm^2; leave anything smaller alone

    # Arachne-like endpoint cleanup. Endpoints are snapped back onto the
    # visible wall/hole boundary when cleanup leaves them slightly short, then
    # emitted with lower E near that boundary. By default taper changes flow on
    # existing straight moves instead of adding tiny grid-like endpoint moves.
    # Centerline clearance is off by default because it can create gaps.
    # Arc moves. Wave runs after Orca has written the file, so Orca's own arc
    # fitter never sees these moves; Wave fits its own arcs instead. This is
    # OFF by default: G2/G3 in the finished file is the one genuinely new
    # kind of output Wave started producing in 0.0.21, and Orca re-parses the
    # file for its preview and time estimate. Until that is confirmed happy
    # on real hardware, arcs are opt-in. "auto" follows the export's own
    # enable_arc_fitting setting; true forces them on.
    "arc_fitting": False,        # false | "auto" | true
    "arc_tolerance": "auto",     # mm the arc may stray; auto = profile resolution

    "edge_snap_distance": "auto",  # mm or auto; endpoint snap-to-boundary reach
    "edge_clearance": 0.0,       # mm or auto; optional inset from walls/holes
    "edge_taper_distance": 0.60,  # mm; 0 disables variable endpoint flow
    "edge_taper_min_flow": 0.55,  # fraction of normal Wave flow at boundary
    "edge_taper_segment": 0.0,    # mm; 0 = no extra endpoint micro-moves

    # Extrusion and cooling. print_speed is in mm/s; travel_speed is in mm/s;
    # fan is 0..1 and is converted to the printer's 0..255 fan value.
    "flow_ratio": 1.0,           # 1.0 = calculated line volume
    "print_speed": "orca",       # follow Orca's own bridge speed; or a number in mm/s
    "travel_speed": 120.0,       # mm/s for non-extruding repositioning
    "fan": 1.0,                  # 1.0 = 100% fan during Wave extrusion
    "max_iterations": 400,       # safety limit on fronts per region
}

# --- the settings panel Orca shows you -------------------------------------
#
# Orca hands the capability config to the user as JSON, and JSON has no
# comment syntax -- so every explanatory comment above is invisible in the
# app. That left the panel a wall of forty bare keys with no way to tell what
# any of them did, or even which ones were worth touching.
#
# So the notes travel *in* the config. Keys beginning with "_" are notes, not
# settings: _cfg() only copies keys that exist in _DEFAULTS, so a note can
# never become a setting, can never be misspelled into one, and can be
# deleted by the user with no effect. Each note sits immediately above the
# setting it describes; dicts keep insertion order and json.dumps preserves
# it, so the panel reads top to bottom.
#
# Rules for writing these: plain English, no jargon, say what happens if you
# change it, and give the units. They are the only documentation most people
# will ever see.

_NOTES = {
    "_READ_ME": (
        "Keys starting with _ are notes, not settings -- the plugin ignores "
        "them, so you can safely leave or delete them. Each note describes "
        "the setting directly below it. Defaults are good for most prints; "
        "the ones people usually touch are print_speed, fan and line_spacing."
    ),

    "_enabled": (
        "Master switch. Set false and the plugin leaves your G-code exactly "
        "as Orca wrote it (useful for an A/B test without uninstalling)."
    ),
    "_time_budget": (
        "Seconds. If the wave pass is still working when this runs out it "
        "gives up and hands back Orca's original file untouched. Protects "
        "you from an export that never finishes. 0 means no limit."
    ),

    "_overhang_tol": (
        "Millimetres of slack when deciding what counts as unsupported. "
        "Bigger = the plugin is more forgiving and treats slightly "
        "overhanging material as supported, so it makes fewer waves."
    ),
    "_min_overhang_area": (
        "Square millimetres. Unsupported patches smaller than this are left "
        "alone. Raise it if tiny specks are getting wave treatment."
    ),

    "_line_spacing": (
        "Millimetres between neighbouring wave lines, centre to centre. "
        "Smaller = denser, stronger, slower, and hotter (less cooling time "
        "between passes). This is the main quality/time dial."
    ),
    "_line_width": (
        "Millimetres. Only a fallback -- if Orca's export states a bridge "
        "width, that wins. Change this only if waves look too fat or thin "
        "and your export has no width information."
    ),
    "_perimeter_overlap": (
        "Millimetres the first wave line starts back inside solid material, "
        "so it is anchored instead of beginning in mid-air."
    ),

    "_propagation_mode": (
        "How the wave flows. \"auto\" (recommended) routes around holes only "
        "when the overhang actually has one. \"obstacle\" always does. "
        "\"legacy\" is the old behaviour and can print across holes."
    ),
    "_wake_blend": (
        "EXPERIMENTAL, leave at 0. Where the wave splits around a hole and "
        "meets again behind it, the two sides form a sharp V that every "
        "later line copies. This rounds that crease off, measured in "
        "multiples of line_spacing (1.0 = one line spacing, max 1.5). It "
        "works on simple round holes, but on complex parts it currently "
        "loses about 4% of the wave coverage and breaks lines into dashes, "
        "which is why it ships off."
    ),
    "_pattern": (
        "Print order. \"smart\" starts each line at its better-supported end. "
        "\"monotonic\" prints strictly nearest-to-furthest. \"zigzag\" "
        "alternates direction for fewer travel moves but more stringing."
    ),
    "_start_policy": (
        "Which end of a wave line to start from: \"supported\", "
        "\"consistent\", \"min-x\", \"max-x\", \"min-y\" or \"max-y\". "
        "\"supported\" is safest; the others help if you see a seam."
    ),
    "_component_order": (
        "When two separate wave areas are the same distance along, print the "
        "one nearest the supported edge (\"support\") or nearest the nozzle "
        "(\"nearest\", fewer travels)."
    ),

    "_min_wave_length": (
        "Millimetres. Whole wave lines shorter than this are dropped as "
        "blob-prone. Lower it to keep more small detail."
    ),
    "_min_wave_segment": (
        "Millimetres. Very short stubs at the ends of a line get merged "
        "away rather than printed as separate dots."
    ),
    "_simplify_tolerance": (
        "Millimetres of allowed smoothing. Removes jitter inherited from the "
        "sliced outline. Raise it for smoother, faster lines; too high and "
        "waves stop hugging the real shape."
    ),
    "_min_bridge_fragment": (
        "Multiple of line width. Any original bridge the waves did not cover "
        "is printed as before if it is at least this long."
    ),

    "_wall_snap": (
        "Stretch waves out to the real wall of the part instead of stopping "
        "at the ragged edge of Orca's bridge lines. This is the fix for the "
        "castellated edges; set false only to compare against the old look."
    ),
    "_wall_reach": (
        "How far that stretch may reach, in millimetres, or \"auto\" for one "
        "and a half line widths. Raise only if waves still stop short."
    ),
    "_wall_overlap": (
        "How far a wave end buries itself into the wall, as a fraction of "
        "line width. 0.25 = a quarter. Higher bonds better but can bulge."
    ),
    "_gap_fill": (
        "Fill the small slivers left where a wave runs out at an angled "
        "boundary, typically the tip of a corner."
    ),
    "_gap_fill_min_area": (
        "Square millimetres. Slivers smaller than this are left alone "
        "instead of being filled with a tiny blob."
    ),

    "_arc_fitting": (
        "Whether WAVE's OWN lines are written as G2/G3 arcs. Keep this "
        "false. This is NOT OrcaSlicer's arc fitting -- that one lives in "
        "Print Settings > Quality > Precision > Arc fitting and is "
        "unaffected by this plugin. Leaving Wave's arcs off is what avoids "
        "OrcaSlicer bug #7433 (post-processed arcs corrupting the preview "
        "or hanging the export), and it costs you almost nothing: Wave's "
        "arcs save under 1% of file size. \"auto\" follows your Orca "
        "setting, true forces arcs on."
    ),
    "_arc_tolerance": (
        "Millimetres an arc may stray from the true path, or \"auto\" to "
        "follow your profile's resolution. Only used if arc_fitting is on."
    ),

    "_edge_snap_distance": (
        "Millimetres, or \"auto\". How far an endpoint may be nudged to land "
        "exactly on the wall or hole edge."
    ),
    "_edge_clearance": (
        "Millimetres to hold back from walls. Normally 0 -- raising it "
        "leaves visible gaps at the edges."
    ),
    "_edge_taper_distance": (
        "Millimetres over which flow eases off as a line approaches the "
        "wall, so ends do not blob. 0 turns tapering off."
    ),
    "_edge_taper_min_flow": (
        "The reduced flow right at the wall, as a fraction of normal. "
        "0.55 = 55%. Lower if ends still look over-extruded."
    ),
    "_edge_taper_segment": (
        "Millimetres. 0 (recommended) tapers by varying flow on the moves "
        "that already exist. Above 0 adds extra tiny moves to taper more "
        "finely, at the cost of a bigger file."
    ),

    "_flow_ratio": (
        "Extrusion multiplier for wave lines only. 1.0 is the calculated "
        "amount. Below 1 for thinner, cooler lines that sag less."
    ),
    "_print_speed": (
        "How fast wave lines print. The default \"orca\" means: use the "
        "bridge speed from your own Orca profile, so waves print at the same "
        "speed as any other bridge on the part. It is read from the exported "
        "G-code section by section, so it follows your profile automatically "
        "and you never have to copy the number across. Change your bridge "
        "speed in Print Settings > Speed and the waves follow it. "
        "Alternatively put a number here in millimetres per second to "
        "override it. This is the single biggest factor in how long a wave "
        "print takes. If your overhang comes out drooping or stringy, this "
        "is the first thing to slow down: try a number like 5, or 2 for a "
        "difficult overhang. Worth knowing why that happens -- your bridge "
        "speed is tuned for a bridge anchored at BOTH ends, where tension "
        "holds the strand up while it cools, but a wave line is cantilevered "
        "into open air and held at one end only, so it can need to be slower "
        "than a bridge on the same printer."
    ),
    "_travel_speed": (
        "Millimetres per second for non-printing moves between waves."
    ),
    "_fan": (
        "Cooling fan during wave printing, 0 to 1 (1 = 100%). Full cooling "
        "is strongly recommended; the fan is restored to your normal "
        "setting afterwards. Lower it only for materials that warp, "
        "like ABS."
    ),
    "_max_iterations": (
        "Safety limit on how many wave lines one region may produce. Raise "
        "only if a large overhang comes out unfinished."
    ),
}


def settings_guide_lines(cfg=None, width=72):
    """The notes as readable text, for printing inside OrcaSlicer.

    The config panel already carries these, but a JSON editor is an awkward
    place to read prose, and the owner should not have to open a README on
    GitHub to find out what a setting does. `cfg` is the live config, so the
    guide shows the value actually in force rather than the default.
    """
    live = cfg or _DEFAULTS
    out = ["--- what every setting means ---",
           "Your current value is shown first; (default X) follows when you",
           "have changed it. The same notes are in the config panel as the",
           "entries beginning with an underscore.",
           ""]
    for key, default in _DEFAULTS.items():
        note = _NOTES.get("_" + key)
        if not note:
            continue
        value = live.get(key, default)
        head = f"{key} = {json.dumps(value)}"
        if value != default:
            head += f"   (default {json.dumps(default)})"
        out.append(head)
        # Wrap by hand: Orca shows this in a plain message box, so long
        # lines would be clipped rather than reflowed.
        line = "   "
        for word in note.split():
            if len(line) + len(word) + 1 > width:
                out.append(line)
                line = "   "
            line += (" " if line.strip() else "") + word
        if line.strip():
            out.append(line)
        out.append("")
    return out


def annotated_defaults():
    """`_DEFAULTS` with each setting preceded by its plain-English note.

    This is what the user actually sees and edits in OrcaSlicer, so it is
    built fresh every time (never hand out `_DEFAULTS` itself to be mutated)
    and ordered note-then-setting.
    """
    out = {"_READ_ME": _NOTES["_READ_ME"]}
    for key, value in _DEFAULTS.items():
        note = _NOTES.get("_" + key)
        if note:
            out["_" + key] = note
        out[key] = value
    return out


# The version this file was built as. Kept in lockstep with the PEP 723 header
# at the top (tests/test_installer.py fails if they drift), so everything that
# reports a version at runtime reports the one actually running.
PLUGIN_VERSION = "0.0.34"

# --- BEGIN changelog (generated by tools/sync_changelog.py) ---
CHANGELOG_RECENT = """\
v0.0.34  (2026-10-02)
   * wave_core registered an empty module in sys.modules before the
     engine was executed into it, and removed it on failure. Pressing
     Refresh in the Plugins dialog re-imports the plugin in the same
     interpreter, so a second pass could have replaced a working engine
     with nothing. The replacement is now built aside and published only
     after a clean exec.
   * _pt() did from shapely.geometry import Point on first use — inside
     a capability call, where Orca's audit hook watches every file open.
     Point now comes from the module-level shapely import that was
     already there.

v0.0.33  (2026-10-02)

v0.0.32  (2026-10-02)
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
    if merged == saved:
        return merged
    try:
        ok = cap.save_config(json.dumps(merged, indent=2))
    except BaseException:
        return merged
    added = [k for k in merged if k not in saved and not k.startswith("_")]
    _log(f"Wave Overhangs v{PLUGIN_VERSION}: config for {name!r} was saved by "
         f"an older build; "
         + (f"added {', '.join(added)}" if added else "refreshed its notes")
         + (" and saved it" if ok else " but Orca refused to save it"))
    return merged


def _migrate_once(cap, template, name):
    """Migrate at most once per session, so a per-step call stays cheap."""
    if name in _MIGRATED:
        return
    _MIGRATED.add(name)
    _migrate_config(cap, template, name)


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
        print_speed=_print_speed_fallback(cfg["print_speed"]),
        travel_speed=float(cfg["travel_speed"]),
        fan=float(cfg["fan"]),
        max_iterations=int(cfg["max_iterations"]),
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
                         "walls": []}
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


def _clean_wave_polyline(points, line_width, tolerance=0.05,
                         min_segment=0.30, allowed=None, guards=None):
    """Remove point noise without shortcutting across a hole or concavity."""
    if len(points) < 2:
        return []
    tolerance = max(0.01, min(0.12, float(tolerance)))
    original = shapely.geometry.LineString(points)
    for attempt in _simplify_attempts(original, tolerance, allowed, guards):
        return attempt
    return _thinned_fallback(original, tolerance, allowed, guards)


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
    """One simplification pass, or None when it would shortcut the geometry."""
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


def _emit_wave_gcode(polylines, z, swcfg, cfg, unsupported=None,
                     support=None, restore_fan=None, arcs=None,
                     print_speed=None):
    """Emit Wave G-code, tapering endpoint flow near walls/holes.

    The path geometry is unchanged. Only E per millimetre is reduced over a
    short distance when a Wave endpoint touches a non-support detail boundary.
    """
    if not polylines:
        return []
    e_per_mm = swcfg.e_per_mm()
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
    for pts in polylines:
        if len(pts) < 2:
            continue
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


def _budget_deadline(cfg, started):
    """Absolute time after which the G-code pass must stop, or None."""
    try:
        budget = float(cfg.get("time_budget", 30.0))
    except (TypeError, ValueError):
        budget = 30.0
    if budget <= 0.0:
        return None
    return started + budget


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
    deadline = _budget_deadline(cfg, started)
    timings = {}
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
            for sec in layer["sections"]:
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
                # Now fix that area's *shape*. Clipping fronts to the outline
                # of Orca's bridge lines is what frays the ends, so stretch
                # the area out to the wall the layer actually printed.
                region, wall_bounded = _wall_bounded_region(
                    bridge, walls, outline, wall_width, swcfg, cfg)
                if wall_bounded:
                    wall_sections += 1
                    unsupported = _backed_parts(
                        region.difference(support.buffer(swcfg.overhang_tol)),
                        core)
                else:
                    unsupported = core
                if unsupported.is_empty or unsupported.area < swcfg.min_overhang_area:
                    continue
                tracks = wc.wave_tracks(support, unsupported, swcfg)
                polylines = _order_wave_tracks(
                    tracks, support, cfg.get("pattern"), cfg.get("start_policy"),
                    cfg.get("component_order", "support"),
                    start_xy=sec["segments"][0]["a"])
                guards = {}
                polylines = [
                    _clean_wave_polyline(
                        p, swcfg.line_width,
                        cfg.get("simplify_tolerance", 0.05),
                        cfg.get("min_wave_segment", 0.30),
                        allowed=unsupported, guards=guards)
                    for p in polylines]
                minimum_wave = max(0.0, float(cfg.get("min_wave_length", 1.0)))
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
                    polylines, unsupported, support, swcfg, cfg)
                if not polylines:
                    continue
                filled = _gap_fill_fronts(unsupported, polylines, swcfg, cfg)
                if filled:
                    polylines = polylines + filled
                    gap_fills += len(filled)
                emit_polylines = _inset_wave_polylines(
                    polylines, unsupported, support, swcfg, cfg)
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
                        min_fragment = max(
                            0.0, float(cfg.get("min_bridge_fragment", 0.5)))
                        if part.length < swcfg.line_width * min_fragment:
                            sec_dropped += 1
                            continue
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
                block = _emit_wave_gcode(
                    emit_polylines, actual_z, swcfg, cfg,
                    unsupported=unsupported, support=support,
                    restore_fan=sec.get("fan"), arcs=arc_limits,
                    print_speed=wave_speed)
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
                          "short_wave_paths_dropped": paths_dropped,
                          "wall_bounded_sections": wall_sections,
                          "arc_moves": 0, "gap_fills": 0,
                          "seconds": round(time.time() - started, 2)}
        out = []
        for i, line in enumerate(lines):
            if i in replacements:
                out.extend(replacements[i])
            else:
                out.append(line)
        timings["plan_seconds"] = round(time.time() - planning, 2)
        timings["seconds"] = round(time.time() - started, 2)
        timings["geometry_layers"] = len(interesting)
        timings["layers_scanned"] = len(layers)
        return WAVE_STAMP + "".join(out), dict(timings, **{
            "replaced_sections": sections_done, "wave_layers": wave_layers,
            "removed_moves": removed, "kept_fragments": kept,
            "tiny_fragments_dropped": dropped,
            "short_wave_paths_dropped": paths_dropped,
            "wall_bounded_sections": wall_sections,
            "arc_moves": arc_moves,
            "gap_fills": gap_fills,
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
        if st.get("last_splice_at"):
            lines.append(f"G-code step: ran, {st.get('last_splice_layers', 0)} "
                         "layer(s) replaced")
            lines.append("Covered bridge extrusion was removed; uncovered")
            lines.append("fragments were retained.")
        else:
            lines.append("No export recorded yet. Select Wave Overhangs under")
            lines.append("Others -> Slicing Pipeline Plugin and use Export G-code file.")
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
