#!/usr/bin/env python3
"""Unlayered Infill -- standalone G-code tool (no OrcaSlicer plugin needed).

Double-click it, pick the G-code you exported, and it writes a wavy copy next
to it. Same engine as the `unlayered_infill_orca.py` plugin, but it runs on a
finished G-code file, so it works on ANY slicer version and does not depend on
OrcaSlicer's plugin system being wired up correctly.

Three ways to run it:

  1. Double-click            -> a small window opens, choose a file, click the
                                button. Writes `<name>_unlayered.gcode`; your
                                input file is never touched.
  2. python unlayered_infill_post.py file.gcode
                             -> same thing from a terminal.
  3. python unlayered_infill_post.py --inplace file.gcode
                             -> rewrites the file itself. This is the form
                                OrcaSlicer's *Post-processing scripts* setting
                                needs, because Orca hands the script its own
                                temporary file and then exports whatever the
                                script leaves behind.

     In OrcaSlicer: Process preset -> Others -> Post-processing scripts, add
         "C:\\Path\\To\\python.exe" "C:\\Path\\To\\unlayered_infill_post.py" --inplace
     (keep the quotes). Orca appends the G-code path as the last argument.

WHY THE PREVIEW WILL NOT SHOW THE WAVE
  No slicer redraws its preview after post-processing -- Orca included. To see
  the result, export the G-code and then drag that exported .gcode file back
  into OrcaSlicer. Post-processing also only runs on "Export G-code file"; it
  does NOT run when you hit Print or Send.

Engine adapted from nonPlanarInfill.py, Copyright (c) 2025 Roman Tenger
(TenTech), GPL-3.0 -- https://github.com/TengerTechnologies/NonPlanarInfill.
This file is likewise GPL-3.0.
"""
import argparse
import os
import sys

TOOL_VERSION = "0.4.9"

# =============================================================================
# ENGINE -- verbatim copy of the `nonplanar_core` source inlined in
# plugins/unlayered-infill/unlayered_infill_orca.py. Keeping the two byte-
# identical is what makes "one engine, two front ends" true rather than a
# hopeful comment; tests/test_installer.py fails if they drift. If you change
# one, copy it into the other.
# --- BEGIN nonplanar_core ---------------------------------------------------
"""Non-planar infill engine: pure text in, pure text out, stdlib only.

Adapted from `nonPlanarInfill.py`, Copyright (c) 2025 Roman Tenger (TenTech),
GPL-3.0 — https://github.com/TengerTechnologies/NonPlanarInfill — by way of the
"Non-Planar Infill Tool" kept verbatim at
`tools/nonplanar-infill-tool/nonplanar_infill_tool.py`.

The idea is unchanged: inside sparse-infill sections, split each extrusion into
short segments and ride a sine wave in Z, `dz = amplitude * scale * sin(f * x)`,
with `scale` tapering to zero as the infill approaches the solid skin above or
below it. Successive layers then interlock instead of stacking as clean planes.

Five behaviours differ from the reference, each pinned by a test in
`tests/test_post_script.py`:

1. **Extrusion is attached to the right move.** In G-code the `E` on a line
   describes the move that *ends* at that line's coordinates. The reference
   read `(x, y)` off the current line as the segment *start* and `(x, y)` off
   the next line as the end, so every stroke was printed with its neighbour's
   extrusion. On a plain rectilinear layer that under-extrudes the long infill
   strokes by ~44% and over-extrudes the short repositioning moves.

2. **No duplicated points.** `segment_line()` returned both endpoints, so each
   emitted stroke re-stated the previous stroke's final point *with a share of
   the extrusion* — a zero-length extruding move, i.e. a blob, at every
   junction (44 of them in a ten-layer test cube).

3. **Z is restored on the way out.** The reference left the nozzle at whatever
   displaced Z the last wave segment reached. Anything printed afterwards in
   the same layer — gap fill, a top surface, the wipe — ran at that wrong
   height until the next layer change reset it.

4. **Orca's skins are recognised.** Solid layers were matched on
   `"solid infill"` only. OrcaSlicer and Bambu Studio label the outermost
   skins `;TYPE:Top surface` and `;TYPE:Bottom surface`, so the real bottom
   was never found and the taper measured from the build plate instead. That
   makes the wave far too aggressive right next to the bottom skin.

5. **The taper can't inverate.** `next_top_layer` kept a stale value once the
   nozzle rose above the topmost solid layer, which made `d_top` negative and
   flipped the wave's sign. Infill that isn't bracketed by solid above *and*
   below now simply gets no wave.

Section markers are matched on `;TYPE:` lines only, and case-insensitively:
PrusaSlicer writes `;TYPE:Internal infill`, Orca `;TYPE:Sparse infill`.
"""
import bisect
import math
import re
# Imported HERE, at module load, and never lazily inside a capability call.
# OrcaSlicer's audit hook is off while a plugin module is being imported and
# ON during a capability call, where every file open is audited. A first-use
# `import statistics` inside the export step would therefore be an audited
# read of the stdlib from inside the audit scope -- the same shape as the
# numpy failure in OrcaSlicer issue #15944. See docs/ORCA-PLUGIN-FACTS.md,
# "The audit hook".
from statistics import multimode

TYPE_PREFIX = ";type:"
INFILL_MARKERS = ("internal infill", "sparse infill")
# "internal solid infill" contains "solid infill"; Orca/Bambu name the outer
# skins "top surface" / "bottom surface", which the reference missed.
SOLID_MARKERS = ("solid infill", "top surface", "bottom surface")

# Amplitude is a share of the LAYER HEIGHT by default, not a fixed mm value:
# "200%" on a 0.3 mm layer is 0.6 mm, and the same setting still makes sense
# after you change layer height. Plain mm ("-0.2") and multiples ("2x") are
# still accepted. Sign only phase-shifts a sine wave, so it barely matters.
DEFAULT_AMPLITUDE = "200%"
DEFAULT_FREQUENCY = 1.5
DEFAULT_SEGMENT_MM = 1.0
# XY resolution of the solid-column map. "auto" means one column per nozzle
# diameter, read from the G-code -- the finest grid that still corresponds to
# something the printer can actually lay down.
DEFAULT_CELL_MM = "auto"
FALLBACK_CELL_MM = 0.6       # used when the G-code does not name the nozzle
DEFAULT_BLEND_MM = 2.0       # smooth the taper across this radius of columns

# --- the shape of the wave -------------------------------------------------
#
# Up to 0.3.4 the displacement was always `sin(frequency * x)`: a ripple that
# varies along X and along X only. That has one real weakness. An infill line
# running along Y crosses no ripple at all -- every point on it has the same
# X, so it gets one constant Z offset over its whole length. It is lifted,
# not waved, and it keys into nothing. With the usual 45/135-degree infill
# that costs little, but with 0/90-degree infill, or any line that happens to
# run across the ripples, half the infill does no interlocking work.
#
# `pattern` fixes that, `angle` aims it, `shape` changes the profile, and
# `layer_phase` stops every layer from being a copy of the one below.
# Defaults reproduce 0.3.4 output exactly.
PATTERNS = ("linear", "cross")
DEFAULT_PATTERN = "linear"
#   linear  ripples that vary along one axis only -- the 0.3.4 behaviour.
#   cross   an egg-crate: ripples along BOTH axes at once, so a line running
#           in any direction still goes up and down. Costs nothing extra and
#           interlocks in two directions instead of one.

DEFAULT_WAVE_ANGLE = 0.0     # degrees, counter-clockwise, 0 = ripples along X

SHAPES = ("sine", "triangle", "square")
DEFAULT_SHAPE = "sine"
#   sine      the smooth classic.
#   triangle  straight ramps into sharp peaks -- steeper flanks for the same
#             peak height, so the layers key together harder.
#   square    flat crests joined by short ramps: most of the infill sits at
#             full offset instead of passing through it. It is a SATURATED
#             sine, never a true square -- a vertical Z step is not printable.
SQUARE_GAIN = 3.0            # how hard the sine is driven before clipping

DEFAULT_LAYER_PHASE = 0.0    # degrees of extra phase per infill layer
#   0 puts the crest of the wave at the same XY on every layer, so the part
#   keeps a column of crests. Advancing the phase a little each layer makes
#   the crests walk sideways, which is what actually braids the layers.
#   180 puts each layer's crest exactly over the layer below's trough.

DEFAULT_MAX_LIFT_MM = 0.0    # 0 = no clamp
#   A hard ceiling on the Z displacement, in millimetres, whatever the
#   amplitude and taper work out to. This is a safety net: "200%" of a
#   0.3 mm layer is 0.6 mm, and if you then raise layer height or amplitude
#   without thinking, the nozzle can be driven far enough up to hit the skin
#   above or plough through already-printed material.

# Stamped into the output so a second pass is a no-op. Orca can invoke
# psGCodePostProcess more than once for one slice (file export and network
# upload are separate calls), and waving an already-waved file would double
# every displacement.
MARKER_PREFIX = "; unlayered-infill"
MARKER_VERSION = "0.4.9"
MARKER = f"{MARKER_PREFIX} v{MARKER_VERSION} (non-planar sparse infill)\n"

_WORD = re.compile(r"([A-Za-z])\s*([-+]?\d*\.?\d+)")
_Z = re.compile(r"Z([-+]?\d*\.?\d+)")


class NonPlanarError(Exception):
    """A problem worth stopping for, explained in plain English."""


def parse_words(line):
    """`G1 X1 Y2 E.5 ; comment` -> {'G':1.0,'X':1.0,'Y':2.0,'E':0.5}."""
    body = line.split(";", 1)[0]
    if not body.strip():
        return None
    words = {}
    for m in _WORD.finditer(body):
        words.setdefault(m.group(1).upper(), float(m.group(2)))
    return words or None


def section_name(line):
    """The section label if this is a `;TYPE:` line, else None."""
    low = line.lower()
    if low.startswith(TYPE_PREFIX):
        return low[len(TYPE_PREFIX):].strip()
    return None


def is_infill_section(name):
    return any(m in name for m in INFILL_MARKERS)


def is_solid_section(name):
    return any(m in name for m in SOLID_MARKERS)


def detect_extrusion_mode(lines):
    """'relative' (M83), 'absolute' (M82), or 'unknown'."""
    head = "\n".join(lines[:400])
    if "M83" in head:
        return "relative"
    if "M82" in head:
        return "absolute"
    tail = "\n".join(lines[-4000:])  # the slicer config block lives at the end
    if "relative_extrusion = 1" in tail or "use_relative_e_distances = 1" in tail:
        return "relative"
    if "relative_extrusion = 0" in tail or "use_relative_e_distances = 0" in tail:
        return "absolute"
    return "unknown"


def detect_layer_height(lines):
    """The layer height actually being printed: the most common `;HEIGHT:`,
    falling back to the slicer's `layer_height = ` config line."""
    heights = []
    config_lh = None
    for line in lines:
        if line.startswith(";HEIGHT:"):
            try:
                heights.append(float(line[len(";HEIGHT:"):].strip()))
            except ValueError:
                pass
        elif config_lh is None and "layer_height" in line:
            m = re.search(r";\s*layer_height\s*=\s*(\d*\.?\d+)", line)
            if m and "first_layer" not in line:
                config_lh = float(m.group(1))
    if heights:
        return max(multimode(heights))
    return config_lh


def resolve_amplitude(spec, lines):
    """'-0.2' -> mm; '200%' / '-1.5x' -> that fraction of the layer height.

    Returns (millimetres, human description).
    """
    s = str(spec).strip()
    if not s:
        raise NonPlanarError(
            "No amplitude given. Use mm (e.g. -0.2), a percent of layer height "
            "(e.g. -150%), or a multiple (e.g. -1.5x).")
    relative = s.endswith("%") or s.lower().endswith("x")
    try:
        value = float(s[:-1]) if relative else float(s)
    except ValueError:
        raise NonPlanarError(
            f"Could not understand amplitude {s!r}. Use mm (e.g. -0.2), a "
            f"percent of layer height (e.g. -150%), or a multiple (e.g. -1.5x).")
    if not relative:
        return value, f"{value:.3f} mm (fixed value)"
    mult = value / 100.0 if s.endswith("%") else value
    lh = detect_layer_height(lines)
    if lh is None:
        raise NonPlanarError(
            f"You gave the amplitude as {s} of layer height, but this G-code has "
            "no layer height in it (no ';HEIGHT:' comments and no "
            "'layer_height =' config line). Give the amplitude in plain mm "
            "instead, e.g. -0.2.")
    amp = mult * lh
    return amp, f"{s} of layer height {lh:.3f} mm = {amp:.3f} mm"


def already_processed(lines):
    """Has this file already been waved? Then leave it completely alone."""
    return any(line.startswith(MARKER_PREFIX) for line in lines)


def waveform(shape, phase):
    """One cycle of the chosen profile at `phase` radians, in [-1, 1].

    All three share their zero crossings and their peak positions, so
    changing `shape` changes the character of the wave without moving it.
    """
    if shape == "sine":
        return math.sin(phase)
    if shape == "square":
        # A real square wave would ask the nozzle to step in Z instantly,
        # which no printer can do and no extrusion can follow. Overdriving a
        # sine and clipping it gives flat crests with short, printable ramps
        # between them -- the useful part of a square wave, minus the cliff.
        return max(-1.0, min(1.0, SQUARE_GAIN * math.sin(phase)))
    if shape == "triangle":
        t = (phase / (2.0 * math.pi)) % 1.0
        return 4.0 * abs(((t - 0.25) % 1.0) - 0.5) - 1.0
    raise NonPlanarError(
        f"Unknown wave shape {shape!r}. Use one of: {', '.join(SHAPES)}.")


def resolve_shape(spec):
    s = str(spec or DEFAULT_SHAPE).strip().lower()
    if s not in SHAPES:
        raise NonPlanarError(
            f"Unknown wave shape {spec!r}. Use one of: {', '.join(SHAPES)}.")
    return s


def resolve_pattern(spec):
    s = str(spec or DEFAULT_PATTERN).strip().lower()
    if s not in PATTERNS:
        raise NonPlanarError(
            f"Unknown wave pattern {spec!r}. Use one of: "
            f"{', '.join(PATTERNS)}.")
    return s


def displacement(x, y, pattern, shape, frequency, angle_rad, phase_rad):
    """The unit wave at a point: the bit that gets multiplied by amplitude.

    Always within [-1, 1], so `max_lift_mm` and the amplitude mean what they
    say no matter which pattern is chosen.
    """
    cos_a, sin_a = math.cos(angle_rad), math.sin(angle_rad)
    u = x * cos_a + y * sin_a
    if pattern == "linear":
        return waveform(shape, frequency * u + phase_rad)
    # cross: the same ripple along the perpendicular axis as well. Averaging
    # the two keeps the result inside [-1, 1]; adding them would double the
    # amplitude the user asked for wherever two crests happened to meet.
    v = -x * sin_a + y * cos_a
    return 0.5 * (waveform(shape, frequency * u + phase_rad) +
                  waveform(shape, frequency * v + phase_rad))


class SolidGrid:
    """Where the part has solid skin, mapped as a grid of XY columns.

    A single global list of solid Z heights is only correct for a part whose
    skins are flat planes spanning the whole footprint. Give it a ledge, a
    bridge, a chamfered top, or two towers of different heights and it fails
    in a way that matters: every column is told its roof is the *highest*
    skin anywhere in the print, so infill directly under a low ledge thinks
    it has metres of headroom and waves at full amplitude straight into it.

    Recording solid heights per XY column instead gives every infill move a
    floor and roof measured in its own column — the wave fades against the
    skin it is actually about to hit.
    """

    __slots__ = ("cell", "columns", "_raw_cache", "_disc")

    def __init__(self, cell_mm=DEFAULT_CELL_MM):
        self.cell = max(0.05, float(cell_mm))
        self.columns = {}
        self._raw_cache = {}
        self._disc = None

    def key(self, x, y):
        c = self.cell
        return (int(math.floor(x / c)), int(math.floor(y / c)))

    def add_move(self, x0, y0, x1, y1, z):
        """Mark every column the solid extrusion (x0,y0)->(x1,y1) crosses."""
        zr = round(z, 4)
        step = self.cell * 0.5
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / step) + 1)
        cols = self.columns
        for i in range(n + 1):
            t = i / n
            cols.setdefault(self.key(x0 + t * (x1 - x0),
                                     y0 + t * (y1 - y0)), set()).add(zr)

    def finalize(self):
        self.columns = {k: sorted(v) for k, v in self.columns.items()}
        return self

    def _raw(self, key, z, full_strength):
        """Taper for one column: 0 at its own skins, peak mid-span.

        Returns None when the column has no solid at all (nothing to measure
        against), which the blend treats as "no opinion" rather than zero.
        """
        ck = (key, z)
        hit = self._raw_cache.get(ck)
        if hit is not None:
            return hit[0]
        zs = self.columns.get(key)
        if not zs:
            self._raw_cache[ck] = (None,)
            return None
        i = bisect.bisect_left(zs, z - 1e-9)
        if i < len(zs) and abs(zs[i] - z) <= 1e-6:
            val = 0.0                      # this column is solid right here
        else:
            below = zs[i - 1] if i > 0 else None
            j = bisect.bisect_right(zs, z + 1e-9)
            above = zs[j] if j < len(zs) else None
            if below is None or above is None or above - below <= 0:
                val = 0.0                  # unbracketed: no wave, no sign flip
            else:
                val = min(above - z, z - below) / (above - below)
                if full_strength:
                    val = min(1.0, val * 2.0)
        self._raw_cache[ck] = (val,)
        return val

    def _offsets(self, blend_mm):
        """Cell offsets within the blend radius, computed once."""
        if self._disc is None:
            r = min(8, int(math.ceil(blend_mm / self.cell)))
            self._disc = [(dx, dy)
                          for dx in range(-r, r + 1)
                          for dy in range(-r, r + 1)
                          if math.hypot(dx, dy) * self.cell <= blend_mm + 1e-9]
        return self._disc

    def scale(self, x, y, z, full_strength=False, blend_mm=DEFAULT_BLEND_MM):
        """Blended taper at a point.

        Neighbouring columns can have very different floors and roofs — at the
        edge of a ledge, one column's roof is 2 mm up and the next one's is
        20 mm up. Taking each column's answer literally puts a step in the
        wave exactly there. Averaging over a small disc turns that step into a
        ramp, which is what `blend_mm` buys.

        Columns with no solid recorded are skipped rather than counted as
        zero: they are usually just gaps between solid extrusion lines, and
        counting them would damp the wave everywhere.
        """
        z = round(z, 4)
        if blend_mm <= 0:
            return self._raw(self.key(x, y), z, full_strength) or 0.0
        ix, iy = self.key(x, y)
        c = self.cell
        total = weight = 0.0
        for dx, dy in self._offsets(blend_mm):
            val = self._raw((ix + dx, iy + dy), z, full_strength)
            if val is None:
                continue
            # distance from the sample point to that column's centre
            cx = (ix + dx + 0.5) * c
            cy = (iy + dy + 0.5) * c
            d = math.hypot(cx - x, cy - y)
            w = 1.0 - d / blend_mm
            if w <= 0.0:
                continue
            total += w * val
            weight += w
        return total / weight if weight > 0.0 else 0.0


def detect_nozzle_diameter(lines):
    """The nozzle diameter the slicer recorded, in mm, or None.

    Orca writes `; nozzle_diameter = 0.6` into the config block at the end of
    the file (comma-separated per extruder on a multi-tool machine; we take
    the first). This is what makes the column grid match the printer instead
    of being an arbitrary constant.
    """
    for line in lines:
        if "nozzle_diameter" not in line:
            continue
        m = re.search(r";\s*nozzle_diameter\s*=\s*([\d.]+)", line)
        if m:
            try:
                v = float(m.group(1))
            except ValueError:
                continue
            if 0.05 <= v <= 5.0:       # anything else is a misparse
                return v
    return None


# --- "auto": take the value from the print instead of a constant -----------
#
# Every number below that describes a LENGTH is really a multiple of the
# nozzle, and every print has a nozzle. Hard-coding 1.0 mm or 2.0 mm means the
# settings are right for a 0.4 nozzle and quietly wrong for a 0.25 or a 0.8.
# "auto" reads the nozzle out of the exported G-code and scales.
#
# The factors are chosen so that "auto" on a 0.4 mm nozzle reproduces the
# constants this plugin shipped with, exactly. Nothing changes for the common
# case; the settings simply follow the printer when it is not the common case.
AUTO_WORDS = ("", "auto", "orca")

#   setting      x nozzle   value at 0.4 mm   what it is
AUTO_NOZZLE_FACTORS = {
    "segment_mm":  2.5,     # 1.0 mm   how finely a move is chopped
    "blend_mm":    5.0,     # 2.0 mm   taper smoothing radius
}
# Wavelength, as a multiple of the nozzle. frequency = 2*pi / wavelength, so
# 10.5 nozzles on a 0.4 gives 1.496 ~ the 1.5 this plugin shipped with. Below
# about 6 nozzles neighbouring crests start running into each other.
AUTO_WAVELENGTH_NOZZLES = 10.5
# A ceiling for "auto" max_lift_mm, as a multiple of the layer height.
AUTO_MAX_LIFT_LAYERS = 1.5


def is_auto(spec):
    """True for None, "", "auto" and "orca" -- anything meaning 'you decide'."""
    return spec is None or (isinstance(spec, str)
                            and spec.strip().lower() in AUTO_WORDS)


def nozzle_or(lines, fallback=FALLBACK_CELL_MM):
    noz = detect_nozzle_diameter(lines)
    return (noz, True) if noz else (fallback, False)


def resolve_auto_length(spec, key, lines, name):
    """A length in mm: "auto" -> a multiple of the nozzle; a number -> itself.

    Returns (millimetres, human description).
    """
    if is_auto(spec):
        factor = AUTO_NOZZLE_FACTORS[key]
        noz, measured = nozzle_or(lines)
        value = factor * noz
        how = (f"auto: {factor:g} x the {noz:.2f} mm nozzle"
               if measured else
               f"auto, but this G-code names no nozzle, so {noz:.2f} mm was "
               f"assumed")
        return value, f"{value:.3f} mm ({how})"
    try:
        value = float(spec)
    except (TypeError, ValueError):
        raise NonPlanarError(
            f"Could not understand {name} {spec!r}. Use 'auto' to follow the "
            f"nozzle, or a number of millimetres.")
    return value, f"{value:.3f} mm (fixed value)"


def resolve_frequency(spec, lines):
    """'auto' -> a wavelength of AUTO_WAVELENGTH_NOZZLES nozzles."""
    if is_auto(spec):
        noz, measured = nozzle_or(lines)
        value = 2.0 * math.pi / (AUTO_WAVELENGTH_NOZZLES * noz)
        how = (f"auto: one ripple every {AUTO_WAVELENGTH_NOZZLES:g} x "
               f"{noz:.2f} mm nozzle widths"
               if measured else
               f"auto, but this G-code names no nozzle, so {noz:.2f} mm was "
               f"assumed")
        return value, f"{value:.3f} ripples/mm ({how})"
    try:
        value = float(spec)
    except (TypeError, ValueError):
        raise NonPlanarError(
            f"Could not understand the frequency {spec!r}. Use 'auto' to "
            f"follow the nozzle, or a number of ripples per millimetre.")
    return value, f"{value:.3f} ripples/mm (fixed value)"


def resolve_wave_angle(spec, lines):
    """'auto' -> square across the slicer's own infill angle.

    A wave only does interlocking work where an infill line CROSSES it, so
    the useful angle is the infill angle turned by 90 degrees. Orca writes
    `; fill_angle = 45` into the exported file.
    """
    if is_auto(spec):
        raw = slicer_setting(lines, "fill_angle")
        if raw is None:
            return 0.0, "0.0 deg (auto, but this G-code names no fill_angle)"
        try:
            fill = float(str(raw).split(",")[0])
        except ValueError:
            return 0.0, "0.0 deg (auto, but fill_angle could not be read)"
        value = (fill + 90.0) % 180.0
        return value, (f"{value:.1f} deg (auto: square across the "
                       f"{fill:g} deg infill)")
    try:
        value = float(spec)
    except (TypeError, ValueError):
        raise NonPlanarError(
            f"Could not understand the wave angle {spec!r}. Use 'auto' to run "
            f"square across the infill, or a number of degrees.")
    return value, f"{value:.1f} deg (fixed value)"


def resolve_max_lift(spec, lines):
    """'auto' -> AUTO_MAX_LIFT_LAYERS layer heights; 0 or 'off' -> no ceiling."""
    if isinstance(spec, str) and spec.strip().lower() in ("off", "none"):
        return 0.0, "no ceiling"
    if is_auto(spec):
        lh = detect_layer_height(lines)
        if not lh:
            return 0.0, "no ceiling (auto, but no layer height in the G-code)"
        value = AUTO_MAX_LIFT_LAYERS * lh
        return value, (f"{value:.3f} mm (auto: {AUTO_MAX_LIFT_LAYERS:g} x the "
                       f"{lh:.3f} mm layer)")
    try:
        value = max(0.0, float(spec))
    except (TypeError, ValueError):
        raise NonPlanarError(
            f"Could not understand the Z ceiling {spec!r}. Use 'auto', 0 for "
            f"no ceiling, or a number of millimetres.")
    return value, (f"{value:.3f} mm (fixed value)" if value else "no ceiling")


def slicer_setting(lines, key):
    """One `; key = value` line out of the slicer's config block, or None."""
    pattern = re.compile(r"^;\s*" + re.escape(key) + r"\s*=\s*(.+?)\s*$")
    for line in reversed(lines):          # the config block is at the end
        m = pattern.match(line.rstrip("\n"))
        if m:
            return m.group(1).strip()
    return None


def resolve_cell_mm(spec, lines):
    """'auto' -> the nozzle diameter; a number -> itself.

    Returns (millimetres, human description).
    """
    if spec is None or (isinstance(spec, str) and spec.strip().lower() in ("", "auto")):
        nozzle = detect_nozzle_diameter(lines)
        if nozzle is None:
            return FALLBACK_CELL_MM, (
                f"{FALLBACK_CELL_MM:.3f} mm (auto, but this G-code does not "
                f"name a nozzle diameter, so the default was used)")
        return nozzle, f"{nozzle:.3f} mm (auto: one column per nozzle width)"
    try:
        v = float(spec)
    except (TypeError, ValueError):
        raise NonPlanarError(
            f"Could not understand the grid cell size {spec!r}. Use 'auto' to "
            f"follow the nozzle diameter, or a number of millimetres.")
    return v, f"{v:.3f} mm (fixed value)"


# ---------------------------------------------------------------------------
#  Nozzle clearance: can the nozzle hit something it already printed?
#
#  This plugin prints a layer at several Z heights at once, which is the
#  whole point of it. The hazard that creates: a move at one Z passing over
#  material that was laid down HIGHER earlier. Within a single wave the
#  displacement is a function of position, so two moves crossing at the same
#  XY always agree on Z and cannot collide -- but the waved infill also has
#  to coexist with everything that is NOT waved (perimeters, solid skin, the
#  next layer), and those are flat.
#
#  So rather than reason about it, measure it: walk the finished file in
#  print order, remember the highest material deposited in each small XY
#  cell, and report any move whose nozzle passes below that by more than the
#  clearance. This sees real collisions regardless of which feature caused
#  them, including the next layer running into a crest.
#
#  It is a REPORT by default, not a refusal. The author of a part is better
#  placed than this plugin to judge whether 0.03 mm of interference matters
#  on their machine -- but they cannot judge it if nobody tells them.
# ---------------------------------------------------------------------------
COLLISION_ACTIONS = ("warn", "refuse", "off")
# Deliberately generous, and calibrated against the shipped settings rather
# than against zero. This plugin EXISTS to make layers key into each other,
# so the nozzle grazing a crest it laid down earlier is the feature working,
# not a crash: at the shipped 200% amplitude the measured interference is
# 0.23 mm, a bit over one layer height. The threshold sits just above that,
# so a stock setup is quiet and anything that genuinely over-lifts -- 400%
# amplitude measures 0.46 mm -- is reported.
AUTO_CLEARANCE_LAYERS = 1.25     # x layer height when clearance is "auto"


def resolve_clearance(spec, lines):
    """'auto' -> a quarter of the layer height; a number -> millimetres."""
    if is_auto(spec):
        lh = detect_layer_height(lines) or 0.2
        value = AUTO_CLEARANCE_LAYERS * lh
        return value, (f"{value:.3f} mm (auto: {AUTO_CLEARANCE_LAYERS:g} x the "
                       f"{lh:.3f} mm layer)")
    try:
        return max(0.0, float(spec)), f"{float(spec):.3f} mm (fixed value)"
    except (TypeError, ValueError):
        raise NonPlanarError(
            f"Could not understand the nozzle clearance {spec!r}. Use 'auto' "
            f"or a number of millimetres.")


def check_nozzle_clearance(lines, cell_mm=None, clearance=0.05,
                           max_report=5):
    """Find moves whose nozzle would pass through material already printed.

    Returns (worst_depth_mm, [description, ...], moves_checked). An empty
    list means nothing was found. Never raises: a safety CHECK that breaks
    the export it is checking would be worse than the hazard.
    """
    try:
        cell = max(0.05, float(cell_mm or detect_nozzle_diameter(lines)
                               or FALLBACK_CELL_MM))
        top = {}                      # (ix, iy) -> highest material top Z
        x = y = z = None
        relative_e = True
        worst = 0.0
        hits = []
        checked = 0
        for raw in lines:
            line = raw.strip()
            if line.startswith("M83"):
                relative_e = True
                continue
            if line.startswith("M82"):
                relative_e = False
                continue
            if not line.startswith(("G0", "G1")):
                continue
            words = {}
            for token in line.split()[1:]:
                if token[:1] in "XYZEF":
                    try:
                        words[token[0]] = float(token[1:])
                    except ValueError:
                        pass
            nx = words.get("X", x)
            ny = words.get("Y", y)
            nz = words.get("Z", z)
            e = words.get("E")
            extruding = e is not None and (e > 0.0 if relative_e else True)
            if None not in (x, y, nx, ny) and nz is not None:
                checked += 1
                length = math.hypot(nx - x, ny - y)
                steps = max(1, int(length / cell) + 1)
                for i in range(steps + 1):
                    t = i / steps
                    px, py = x + (nx - x) * t, y + (ny - y) * t
                    key = (int(px / cell), int(py / cell))
                    already = top.get(key)
                    if already is not None and nz < already - clearance:
                        depth = already - nz
                        if depth > worst:
                            worst = depth
                        if len(hits) < max_report:
                            hits.append(
                                f"at X{px:.1f} Y{py:.1f} the nozzle passes at "
                                f"Z{nz:.3f} through material already printed "
                                f"up to Z{already:.3f} ({depth:.3f} mm deep)")
                    if extruding:
                        if already is None or nz > already:
                            top[key] = nz
            x, y, z = nx, ny, nz
        return worst, hits, checked
    except Exception:
        return 0.0, [], 0



def build_solid_grid(lines, cell_mm=FALLBACK_CELL_MM):
    """Rasterise every solid-skin extrusion into an XY column map."""
    grid = SolidGrid(cell_mm)
    x = y = None
    z = 0.0
    solid = False
    solid_heights = set()
    for line in lines:
        name = section_name(line)
        if name is not None:
            solid = is_solid_section(name)
            continue
        words = parse_words(line)
        if not words or words.get("G") not in (0.0, 1.0):
            continue
        nx = words.get("X", x)
        ny = words.get("Y", y)
        if "Z" in words:
            z = words["Z"]
        e = words.get("E")
        if solid and e is not None and e > 0 and None not in (x, y, nx, ny):
            grid.add_move(x, y, nx, ny, z)
            solid_heights.add(round(z, 4))
        x, y = nx, ny
    grid.finalize()
    return grid, sorted(solid_heights)


def _empty_stats(**over):
    base = {"amplitude_mm": 0.0, "amplitude_desc": "", "extrusion_mode": "",
            "solid_layers": 0, "solid_columns": 0, "sections": 0, "moves": 0,
            "segments": 0, "max_wiggle": 0.0, "skipped_unbracketed": 0,
            "already_processed": False, "cell_mm": 0.0, "cell_desc": "",
            "nozzle_mm": None, "layer_height_mm": None, "blend_mm": 2.0,
            "full_strength": False, "pattern": DEFAULT_PATTERN,
            "wave_angle": DEFAULT_WAVE_ANGLE, "shape": DEFAULT_SHAPE,
            "layer_phase": DEFAULT_LAYER_PHASE,
            "max_lift_mm": 0.0, "clamped": 0,
            "wave_layers": 0, "frequency_desc": "", "segment_desc": "",
            "blend_desc": "", "wave_angle_desc": "", "max_lift_desc": ""}
    base.update(over)
    return base


def process(lines, amplitude_spec=DEFAULT_AMPLITUDE, frequency=DEFAULT_FREQUENCY,
            segment_mm=DEFAULT_SEGMENT_MM, require_relative_e=True,
            cell_mm=DEFAULT_CELL_MM, blend_mm=DEFAULT_BLEND_MM,
            full_strength=False, pattern=DEFAULT_PATTERN,
            wave_angle=DEFAULT_WAVE_ANGLE, shape=DEFAULT_SHAPE,
            layer_phase=DEFAULT_LAYER_PHASE, max_lift_mm=DEFAULT_MAX_LIFT_MM):
    """Rewrite sparse-infill moves as wavy ones. Returns (out_lines, stats)."""
    # Orca may run the export step twice for one slice (file + upload). Waving
    # an already-waved file would double every displacement, so bail out.
    if already_processed(lines):
        return list(lines), _empty_stats(already_processed=True)

    amplitude, amp_desc = resolve_amplitude(amplitude_spec, lines)

    mode = detect_extrusion_mode(lines)
    if mode == "absolute" and require_relative_e:
        raise NonPlanarError(
            "This G-code uses ABSOLUTE extrusion (M82). Splitting moves would "
            "corrupt it.\n\nFix: OrcaSlicer > Printer Settings > Advanced > "
            "'Use relative E distances', then slice again.")

    cell_resolved, cell_desc = resolve_cell_mm(cell_mm, lines)
    grid, solids = build_solid_grid(lines, cell_resolved)
    # Anything that can be read off the print is read off the print. Each
    # resolver also hands back a sentence saying what it decided and why, so
    # the log and "Check setup" can show the real number rather than "auto".
    frequency, frequency_desc = resolve_frequency(frequency, lines)
    segment_mm, segment_desc = resolve_auto_length(
        segment_mm, "segment_mm", lines, "the segment length")
    segment_mm = max(0.05, segment_mm)
    blend_mm, blend_desc = resolve_auto_length(
        blend_mm, "blend_mm", lines, "the blend radius")
    blend_mm = max(0.0, blend_mm)
    full_strength = bool(full_strength)
    pattern = resolve_pattern(pattern)
    shape = resolve_shape(shape)
    wave_angle, wave_angle_desc = resolve_wave_angle(wave_angle, lines)
    angle_rad = math.radians(wave_angle)
    layer_phase_rad = math.radians(float(layer_phase))
    max_lift_mm, max_lift_desc = resolve_max_lift(max_lift_mm, lines)

    out = []
    x = y = z = None
    in_infill = False
    displaced = False           # nozzle currently sitting at a wave-shifted Z
    sections = moves = segments = 0
    max_wiggle = 0.0
    skipped_unbracketed = 0
    clamped = 0                 # segments the max_lift ceiling actually caught

    # Which infill layer we are on, for `layer_phase`. Counted only at the
    # heights where infill is really waved, and in the order they are met, so
    # a Z-hop or a travel at some other height cannot advance the phase. The
    # first waved layer is 0, which keeps `layer_phase` from moving anything
    # when the feature is off.
    layer_ordinal = {}

    def restore_z():
        """Never leave the nozzle on the wave once the infill stroke ends."""
        nonlocal displaced
        if displaced and z is not None:
            out.append(f"G1 Z{z:.3f}\n")
            displaced = False

    for line in lines:
        name = section_name(line)
        if name is not None:
            was = in_infill
            in_infill = is_infill_section(name)
            if in_infill and not was:
                sections += 1
            if not in_infill:
                restore_z()
            out.append(line)
            continue

        words = parse_words(line)
        if not words or words.get("G") not in (0.0, 1.0):
            out.append(line)
            continue

        nx = words.get("X", x)
        ny = words.get("Y", y)
        e = words.get("E")

        if "Z" in words:                 # the slicer's own Z always wins
            z = words["Z"]
            displaced = False

        movable = (in_infill and e is not None and e > 0 and "Z" not in words
                   and None not in (x, y, z) and (nx != x or ny != y))
        if movable:
            # sample the taper at the midpoint of the stroke
            scale = grid.scale((x + nx) * 0.5, (y + ny) * 0.5, z,
                               full_strength, blend_mm)
            if scale > 0.0:
                length = math.hypot(nx - x, ny - y)
                n = max(1, int(length // segment_mm))
                feed = words.get("F")
                zr = round(z, 4)
                if zr not in layer_ordinal:
                    layer_ordinal[zr] = len(layer_ordinal)
                phase_rad = layer_ordinal[zr] * layer_phase_rad
                # Hand out the extrusion so the printed digits sum to exactly
                # `e`. Rounding each segment independently drifts, and across
                # a whole print that drift is systematic under/over-extrusion.
                spent = 0.0
                for i in range(1, n + 1):      # skip i=0: we are already there
                    t = i / n
                    sx = x + t * (nx - x)
                    sy = y + t * (ny - y)
                    dz = amplitude * scale * displacement(
                        sx, sy, pattern, shape, frequency, angle_rad, phase_rad)
                    if max_lift_mm and abs(dz) > max_lift_mm:
                        dz = math.copysign(max_lift_mm, dz)
                        clamped += 1
                    if abs(dz) > max_wiggle:
                        max_wiggle = abs(dz)
                    share = round(e * t - spent, 5)
                    spent += share
                    tail = f" F{feed:.0f}" if (i == 1 and feed is not None) else ""
                    out.append(f"G1 X{sx:.3f} Y{sy:.3f} Z{z + dz:.3f} "
                               f"E{share:.5f}{tail}\n")
                segments += n
                moves += 1
                displaced = True
                x, y = nx, ny
                continue
            skipped_unbracketed += 1

        restore_z()
        out.append(line)
        x, y = nx, ny

    restore_z()

    if moves:
        out.insert(0, MARKER)

    return out, {
        "amplitude_mm": amplitude,
        "amplitude_desc": amp_desc,
        "extrusion_mode": mode,
        "solid_layers": len(solids),
        "solid_columns": len(grid.columns),
        "sections": sections,
        "moves": moves,
        "segments": segments,
        "max_wiggle": max_wiggle,
        "skipped_unbracketed": skipped_unbracketed,
        "already_processed": False,
        "cell_mm": grid.cell,
        "cell_desc": cell_desc,
        "nozzle_mm": detect_nozzle_diameter(lines),
        "layer_height_mm": detect_layer_height(lines),
        "blend_mm": blend_mm,
        "full_strength": full_strength,
        "pattern": pattern,
        "wave_angle": float(wave_angle),
        "shape": shape,
        "layer_phase": float(layer_phase),
        "max_lift_mm": max_lift_mm,
        "clamped": clamped,
        "wave_layers": len(layer_ordinal),
        # What each "auto" actually resolved to, in words.
        "frequency_desc": frequency_desc,
        "segment_desc": segment_desc,
        "blend_desc": blend_desc,
        "wave_angle_desc": wave_angle_desc,
        "max_lift_desc": max_lift_desc,
    }
# --- END nonplanar_core -----------------------------------------------------


def default_output_path(input_file):
    folder = os.path.dirname(os.path.abspath(input_file))
    name, _ = os.path.splitext(os.path.basename(input_file))
    return os.path.join(folder, name + "_unlayered.gcode")


def read_lines(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return f.readlines()


def describe(stats):
    """Plain-English account of what the engine did, and why if it did nothing."""
    if stats["already_processed"]:
        return ["This file has ALREADY been waved (it carries the "
                f"'{MARKER_PREFIX}' stamp).",
                "Nothing was changed -- re-processing would double every "
                "displacement.",
                "Slice again and run the tool on the fresh file."]

    lh = stats.get("layer_height_mm")
    noz = stats.get("nozzle_mm")
    out = [
        f"extrusion mode ....... {stats['extrusion_mode']}",
        f"layer height ......... "
        + (f"{lh:.3f} mm" if lh else "not stated in the G-code"),
        f"nozzle ............... "
        + (f"{noz:.3f} mm" if noz else "not stated in the G-code"),
        f"grid columns ......... {stats.get('cell_desc', '')}",
        f"amplitude ............ {stats['amplitude_desc']}",
        f"solid skin found ..... {stats['solid_layers']} layer height(s), "
        f"{stats['solid_columns']} XY column(s)",
        f"sparse-infill runs ... {stats['sections']}",
        f"infill moves waved ... {stats['moves']}",
        f"segments emitted ..... {stats['segments']}",
        f"largest Z shift ...... {stats['max_wiggle']:.3f} mm"
        + ("  (full strength)" if stats["full_strength"] else
           "  (classic taper -- peaks at half the amplitude)"),
        f"wave ................. {stats.get('shape', 'sine')}, "
        f"{stats.get('pattern', 'linear')}"
        + (f" at {stats['wave_angle']:g} deg" if stats.get("wave_angle") else "")
        + (f", +{stats['layer_phase']:g} deg per layer"
           if stats.get("layer_phase") else ""),
    ]
    if stats.get("max_lift_mm"):
        out.append(f"Z ceiling ............ {stats['max_lift_mm']:.3f} mm"
                   + (f"  (capped {stats['clamped']} segment(s))"
                      if stats.get("clamped") else "  (never reached)"))
    if stats["skipped_unbracketed"]:
        out.append(f"left flat ............ {stats['skipped_unbracketed']} move(s) "
                   f"with no solid skin both above and below")

    if stats["moves"]:
        if not stats["full_strength"] and stats["max_wiggle"] < 0.1:
            out.append("")
            out.append("It worked, but the wave is small -- the classic taper "
                       "peaks at HALF the amplitude, and fades to nothing near "
                       "the skins. If you cannot see it:")
            out.append("  - add --full-strength (up to the full amplitude), and/or")
            out.append("  - use a bigger amplitude, e.g. -a -150%% of layer height")
        return out

    # Nothing changed -- say why, concretely, instead of leaving them guessing.
    out.append("")
    out.append("NOTHING WAS CHANGED. The likely reason:")
    if stats["sections"] == 0:
        out.append("  No sparse-infill sections in this file at all.")
        out.append("  - Is infill density 0%? A wave needs infill to ride on.")
        out.append("  - Thin, all-wall parts genuinely have no infill.")
        out.append("  - Is this really sliced G-code? The engine looks for")
        out.append("    ';TYPE:Sparse infill' / ';TYPE:Internal infill' lines.")
    elif stats["solid_layers"] == 0:
        out.append("  Infill was found, but no solid skin was.")
        out.append("  The wave tapers to flat against the skin above and below,")
        out.append("  so with no skin there is nothing to taper against.")
        out.append("  - Check top/bottom shell layers are not 0.")
    else:
        out.append("  Infill and skin were both found, but no infill move sits")
        out.append("  between a solid skin above AND below it in its own column.")
        out.append("  - Very short parts, or infill only under a ledge, do this.")
        out.append("  - Try a taller test part (a 20 mm cube is a good check).")
    return out


def process_file(path, out_path, amplitude, frequency, inplace=False,
                 dry_run=False, require_relative_e=True, full_strength=False,
                 cell_mm=DEFAULT_CELL_MM, pattern=DEFAULT_PATTERN,
                 wave_angle=DEFAULT_WAVE_ANGLE, shape=DEFAULT_SHAPE,
                 layer_phase=DEFAULT_LAYER_PHASE,
                 max_lift_mm=DEFAULT_MAX_LIFT_MM):
    """Run the engine over one file. Returns (stats, wrote_path_or_None)."""
    lines = read_lines(path)
    new_lines, stats = process(lines, amplitude_spec=amplitude,
                               frequency=frequency,
                               require_relative_e=require_relative_e,
                               full_strength=full_strength,
                               cell_mm=cell_mm, pattern=pattern,
                               wave_angle=wave_angle, shape=shape,
                               layer_phase=layer_phase,
                               max_lift_mm=max_lift_mm)
    if dry_run or stats["already_processed"] or not stats["moves"]:
        return stats, None
    target = path if inplace else out_path
    with open(target, "w", encoding="utf-8") as f:
        f.writelines(new_lines)
    return stats, target


# ---------------------------------------------------------------------------
# Mode 1: terminal / slicer post-processing
# ---------------------------------------------------------------------------
def run_cli(argv=None):
    p = argparse.ArgumentParser(
        prog="unlayered_infill_post.py",
        description="Make sparse infill ride a sine wave in Z so layers "
                    "interlock. Works on any sliced G-code.")
    p.add_argument("input_file", nargs="?", help="the G-code file to process")
    p.add_argument("-a", "--amplitude", default=DEFAULT_AMPLITUDE,
                   help='a share of layer height (200%% of a 0.3 mm layer is '
                        '0.6 mm), a multiple (2x), or plain mm (0.6). '
                        'Default: %(default)s')
    p.add_argument("-f", "--frequency", type=float, default=DEFAULT_FREQUENCY,
                   help="ripples per mm along X (default: %(default)s)")
    p.add_argument("-i", "--inplace", action="store_true",
                   help="rewrite the file itself -- what OrcaSlicer's "
                        "Post-processing scripts setting needs")
    p.add_argument("-c", "--cell", default=DEFAULT_CELL_MM,
                   help="width of the solid-skin grid columns in mm. "
                        "'auto' (default) uses the nozzle diameter from the "
                        "G-code, so a local ceiling or floor only affects the "
                        "columns beneath it.")
    p.add_argument("-n", "--dry-run", action="store_true",
                   help="report what would happen, write nothing")
    p.add_argument("-s", "--full-strength", action="store_true",
                   help="let the wave reach the full amplitude mid-span. The "
                        "default classic taper peaks at half of it, which is "
                        "safer but much less visible.")
    p.add_argument("-p", "--pattern", default=DEFAULT_PATTERN, choices=PATTERNS,
                   help="'linear' ripples along one direction only; 'cross' "
                        "is an egg-crate rippling along both, so an infill "
                        "line running in any direction still rises and falls "
                        "(default: %(default)s)")
    p.add_argument("--angle", type=float, default=DEFAULT_WAVE_ANGLE,
                   help="degrees to turn the ripples, counter-clockwise, 0 "
                        "being along X. Aim them across your infill lines "
                        "(default: %(default)s)")
    p.add_argument("--shape", default=DEFAULT_SHAPE, choices=SHAPES,
                   help="wave profile: smooth 'sine', sharp-peaked "
                        "'triangle', or 'square' -- flat crests with short "
                        "ramps, a saturated sine rather than a Z cliff "
                        "(default: %(default)s)")
    p.add_argument("--layer-phase", type=float, default=DEFAULT_LAYER_PHASE,
                   help="degrees of extra phase per waved layer, so crests "
                        "walk sideways instead of stacking in a column. 180 "
                        "puts a crest over the trough below (default: "
                        "%(default)s)")
    p.add_argument("--max-lift", type=float, default=DEFAULT_MAX_LIFT_MM,
                   help="hard ceiling on the Z displacement in mm, whatever "
                        "amplitude and taper work out to. 0 = no ceiling "
                        "(default: %(default)s)")
    p.add_argument("--allow-absolute-e", action="store_true",
                   help="do not refuse absolute-E (M82) G-code. Not "
                        "recommended: splitting moves under absolute E "
                        "corrupts the file.")
    p.add_argument("--version", action="version",
                   version=f"unlayered-infill v{TOOL_VERSION}")
    args = p.parse_args(argv)

    if not args.input_file:
        return False        # no file given -> caller opens the window instead

    if not os.path.isfile(args.input_file):
        print(f"ERROR: no such file: {args.input_file}")
        sys.exit(1)

    out_path = default_output_path(args.input_file)
    print(f"Unlayered Infill v{TOOL_VERSION}")
    if args.inplace and not args.dry_run:
        print("In-place mode: rewriting the file itself.")
    try:
        stats, written = process_file(
            args.input_file, out_path, args.amplitude, args.frequency,
            inplace=args.inplace, dry_run=args.dry_run,
            require_relative_e=not args.allow_absolute_e,
            full_strength=args.full_strength, cell_mm=args.cell,
            pattern=args.pattern, wave_angle=args.angle, shape=args.shape,
            layer_phase=args.layer_phase, max_lift_mm=args.max_lift)
    except NonPlanarError as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    for line in describe(stats):
        print(line)
    if written:
        print("")
        print(f"WROTE: {written}")
        if not args.inplace:
            print("Print THAT file, not the original.")
    elif args.dry_run:
        print("")
        print("Dry run -- nothing written.")
    return True


# ---------------------------------------------------------------------------
# Mode 2: double-click -> a small window
# ---------------------------------------------------------------------------
def run_gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox

    root = tk.Tk()
    root.title(f"Unlayered Infill v{TOOL_VERSION}")
    root.geometry("620x520")
    state = {"file": ""}

    file_lbl = tk.Label(root, text="No G-code file chosen yet", wraplength=580,
                        anchor="w", justify="left")
    file_lbl.pack(padx=12, pady=(12, 4), fill="x")

    def browse():
        path = filedialog.askopenfilename(
            title="Choose the G-code you exported from your slicer",
            filetypes=[("G-code", "*.gcode *.gco *.g *.txt"), ("All files", "*.*")])
        if path:
            state["file"] = path
            file_lbl.config(text=path)

    tk.Button(root, text="Choose G-code file...", command=browse).pack(pady=(0, 8))

    row = tk.Frame(root)
    row.pack()
    tk.Label(row, text="Amplitude (mm or -150%): ").pack(side="left")
    amp_var = tk.StringVar(value=str(DEFAULT_AMPLITUDE))
    tk.Entry(row, textvariable=amp_var, width=8).pack(side="left")
    tk.Label(row, text="   Frequency: ").pack(side="left")
    freq_var = tk.StringVar(value=str(DEFAULT_FREQUENCY))
    tk.Entry(row, textvariable=freq_var, width=8).pack(side="left")

    full_var = tk.BooleanVar(value=False)
    tk.Checkbutton(root, variable=full_var,
                   text="Full strength (wave reaches the whole amplitude -- "
                        "much more visible)").pack(pady=(6, 0))

    status = tk.Text(root, height=16, wrap="word", state="disabled")
    status.pack(padx=12, pady=10, fill="both", expand=True)

    def say(msg=""):
        status.config(state="normal")
        status.insert("end", str(msg) + "\n")
        status.see("end")
        status.config(state="disabled")
        root.update_idletasks()

    def go():
        status.config(state="normal")
        status.delete("1.0", "end")
        status.config(state="disabled")
        if not state["file"]:
            messagebox.showinfo("Pick a file first",
                                "Use the button above to choose your sliced G-code.")
            return
        try:
            freq = float(freq_var.get())
        except ValueError:
            messagebox.showerror("Numbers needed", "Frequency must be a number, like 1.5.")
            return
        try:
            stats, written = process_file(state["file"],
                                          default_output_path(state["file"]),
                                          amp_var.get(), freq,
                                          full_strength=full_var.get())
        except NonPlanarError as e:
            messagebox.showerror("Cannot process this file", str(e))
            return
        except Exception as e:      # never die silently on a beginner's machine
            messagebox.showerror("Something went wrong", f"{type(e).__name__}: {e}")
            return
        for line in describe(stats):
            say(line)
        if written:
            say()
            say("DONE. Wavy G-code saved as:")
            say(f"  {written}")
            say("Print THAT file, not the original.")
            say()
            say("To SEE it: drag that file into OrcaSlicer. The preview of the "
                "original slice never shows post-processing.")

    tk.Button(root, text="MAKE IT WAVY", command=go,
              font=("Segoe UI", 11, "bold")).pack(pady=(0, 12))
    root.mainloop()


if __name__ == "__main__":
    if not run_cli():
        try:
            run_gui()
        except ImportError:
            print("This Python has no tkinter, so there is no window mode.")
            print("Run it like:  python unlayered_infill_post.py yourfile.gcode")
            sys.exit(1)
