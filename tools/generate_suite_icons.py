#!/usr/bin/env python3
"""Generate the unified Soft Vanguard suite emblem, app icons (.ico + .png),
and a showcase sheet so every app in the repository shares one cohesive visual identity."""

from pathlib import Path
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
BRAND_DIR = ROOT / "docs" / "brand"


def trispoke_sdf_alpha(size: int, scale: float = 0.88, thickness_boost: float = 0.0,
                       angle_deg: float = 0.0) -> np.ndarray:
    """Analytic sub-pixel anti-aliased alpha field [0..1] of the Suite Tri-Spoke Core Emblem:
      - Outer precision ring
      - Three 120-degree curved arc segments (gaps at 12, 4, and 8 o'clock)
      - Three double-parallel radial spokes at -90 deg, +30 deg, +150 deg
      - Solid center disc with crisp upward-pointing equilateral triangle cutout
    """
    lin = np.linspace(-1.0, 1.0, size, dtype=np.float32)
    yy, xx = np.meshgrid(lin, lin, indexing="ij")
    px_size = 2.0 / float(size)
    aa = max(px_size * 1.35, 1e-4)

    # Rotate the SAMPLE grid (not the output): the shape spins while every
    # frame keeps the same studio lighting, so the spin matches the static
    # icon's shading at all times instead of swinging a highlight around.
    if angle_deg:
        th = math.radians(-angle_deg)
        ct, st = math.cos(th), math.sin(th)
        xr = xx * ct - yy * st
        yr = xx * st + yy * ct
        xx, yy = xr, yr

    r = np.hypot(xx, yy) / scale
    ang = np.degrees(np.arctan2(yy, xx))  # [-180, 180], -90 is straight up

    b = thickness_boost

    # 1. Outer thin circular ring
    r_out_mid = 0.895
    r_out_hw = 0.022 + b * 0.45
    d_outer_ring = np.abs(r - r_out_mid) - r_out_hw

    # 2. Three thick 120-degree arc segments centered at -30, +90, -150 deg
    r_arc_mid = 0.742
    r_arc_hw = 0.056 + b * 0.5
    d_radial_arc = np.abs(r - r_arc_mid) - r_arc_hw

    # Angular distance to each of the 3 gap centers (-90, +30, +150)
    d_gap = np.full_like(r, 180.0)
    for gap_deg in (-90.0, 30.0, 150.0):
        diff = np.abs(((ang - gap_deg + 180.0) % 360.0) - 180.0)
        d_gap = np.minimum(d_gap, diff)
    # Linear arc gap distance around radius r_arc_mid
    gap_half_deg = 12.5
    d_ang_cut = (gap_half_deg - d_gap) * (math.pi / 180.0) * r_arc_mid
    d_arcs = np.maximum(d_radial_arc, d_ang_cut)

    # 3. Three double-parallel radial spoke bars at -90 (up), +30 (4 o'clock), +150 (8 o'clock)
    d_spokes = np.full_like(r, 10.0)
    p_mid = 0.052 + b * 0.12
    p_hw = 0.024 + b * 0.42
    s_in = 0.258
    s_out = 0.722
    s_mid = 0.5 * (s_in + s_out)
    s_hw = 0.5 * (s_out - s_in)

    for deg in (-90.0, 30.0, 150.0):
        rad = math.radians(deg)
        ux, uy = math.cos(rad), math.sin(rad)
        proj = (xx * ux + yy * uy) / scale
        perp = np.abs(-xx * uy + yy * ux) / scale
        d_along = np.abs(proj - s_mid) - s_hw
        d_bars = np.abs(perp - p_mid) - p_hw
        d_this = np.maximum(d_along, d_bars)
        d_spokes = np.minimum(d_spokes, d_this)

    # 4. Solid center hub minus upward-pointing equilateral triangle cutout
    d_hub_outer = r - (0.224 + b * 0.25)
    # Equilateral triangle vertices at (-90, +30, +150) -> outward normals at (+90, -150, -30)
    tri_inradius = 0.074
    d_tri = np.full_like(r, -10.0)
    for ndeg in (90.0, -150.0, -30.0):
        nrad = math.radians(ndeg)
        nx, ny = math.cos(nrad), math.sin(nrad)
        d_half = ((xx * nx + yy * ny) / scale) - tri_inradius
        d_tri = np.maximum(d_tri, d_half)
    d_hub = np.maximum(d_hub_outer, -d_tri)

    d_union = np.minimum(np.minimum(d_outer_ring, d_arcs), np.minimum(d_spokes, d_hub))
    alpha = np.clip(0.5 - (d_union * scale) / aa, 0.0, 1.0)
    return alpha


def make_squircle_base(size: int, tint_rgb: tuple, watermark: bool = True) -> Image.Image:
    """Legacy squircle plate (kept for the showcase sheet only). The exported
    app icons use make_transparent_studio_base instead: the opaque plate read
    as a muddy warm-brown blob at taskbar size and never matched the app
    window backgrounds, so the icons now float on full transparency just like
    the emblem in each window header."""
    lin = np.linspace(-1.0, 1.0, size, dtype=np.float32)
    yy, xx = np.meshgrid(lin, lin, indexing="ij")
    px = 2.0 / float(size)

    # Superellipse squircle (n = 4.4) with clean margin for drop shadow
    R = 0.875
    n = 4.4
    p = (np.abs(xx / R) ** n + np.abs(yy / R) ** n) ** (1.0 / n)
    sd = (p - 1.0) * R

    mask = np.clip(0.5 - sd / (px * 1.4), 0.0, 1.0)

    # Soft drop shadow layer
    sh_img = Image.fromarray((mask * 155.0).astype(np.uint8), mode="L")
    sh_img = sh_img.filter(ImageFilter.GaussianBlur(radius=size * 0.032))
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sh_layer = Image.new("RGBA", (size, size), (6, 9, 16, 255))
    sh_layer.putalpha(sh_img)
    canvas.alpha_composite(sh_layer, dest=(0, int(size * 0.02)))

    # Brushed dark slate body with soft top-left studio light + subtle accent glow
    t_diag = np.clip((xx * 0.55 + yy * 0.85 + 1.0) * 0.5, 0.0, 1.0)
    base_top = np.array([48.0, 54.0, 66.0], dtype=np.float32)
    base_bot = np.array([25.0, 29.0, 38.0], dtype=np.float32)
    rgb = base_top[None, None, :] * (1.0 - t_diag[:, :, None]) + base_bot[None, None, :] * t_diag[:, :, None]

    # Subtle radial brushed metallic sheen (like the user's metallic medallion screenshot)
    theta = np.arctan2(yy, xx)
    sheen = 0.5 + 0.5 * np.cos(2.0 * (theta + 0.65))
    r_norm = np.clip(np.hypot(xx, yy) / R, 0.0, 1.0)
    rgb += (sheen[:, :, None] - 0.5) * 7.5 * r_norm[:, :, None]

    # Soft color-matched ambient glow in upper-left quadrant
    glow = np.exp(-(((xx + 0.35) ** 2 + (yy + 0.42) ** 2) / 0.62))
    tint = np.array(tint_rgb, dtype=np.float32)
    rgb = rgb * (1.0 - 0.16 * glow[:, :, None]) + tint[None, None, :] * (0.16 * glow[:, :, None])

    # Subtle recessed Tri-Spoke Core watermark embedded in the dark slate plate
    if watermark:
        wm = trispoke_sdf_alpha(size, scale=0.73)
        rgb = rgb * (1.0 - 0.14 * wm[:, :, None]) + np.array([150.0, 165.0, 190.0])[None, None, :] * (0.14 * wm[:, :, None])

    # Crisp 1.5px top-lit inner bevel rim along the squircle edge
    rim = np.clip(1.0 - np.abs(sd + 0.012) / 0.014, 0.0, 1.0)
    rim_light = np.clip(0.65 - 0.45 * (xx * 0.4 + yy * 0.9), 0.12, 1.0)
    rim_strength = rim * rim_light * 0.42
    rgb = rgb * (1.0 - rim_strength[:, :, None]) + np.array([226.0, 236.0, 250.0])[None, None, :] * rim_strength[:, :, None]

    arr = np.zeros((size, size, 4), dtype=np.uint8)
    arr[:, :, :3] = np.clip(rgb, 0.0, 255.0).astype(np.uint8)
    arr[:, :, 3] = (mask * 255.0).astype(np.uint8)
    body = Image.fromarray(arr, mode="RGBA")
    canvas.alpha_composite(body)
    return canvas


def make_transparent_studio_base(size: int, tint_rgb: tuple) -> Image.Image:
    """Fully transparent canvas. Depth comes from the per-glyph drop shadow +
    glow inside composite_elevated_layer, so the icon reads cleanly on any
    taskbar, desktop, or start-menu background instead of carrying its own
    mismatched plate color."""
    return Image.new("RGBA", (size, size), (0, 0, 0, 0))


def composite_elevated_layer(base: Image.Image, alpha: np.ndarray,
                             col_top: tuple, col_bot: tuple,
                             glow_rgb: tuple = None,
                             shadow: bool = True) -> Image.Image:
    """Composite a smooth gradient glyph layer with soft studio drop shadow + subtle ambient glow."""
    size = base.width
    lin = np.linspace(0.0, 1.0, size, dtype=np.float32)
    yy, xx = np.meshgrid(lin, lin, indexing="ij")
    t = np.clip(xx * 0.35 + yy * 0.75, 0.0, 1.0)

    a_u8 = (np.clip(alpha, 0.0, 1.0) * 255.0).astype(np.uint8)
    mask_img = Image.fromarray(a_u8, mode="L")

    if glow_rgb is not None:
        gl = mask_img.filter(ImageFilter.GaussianBlur(radius=size * 0.045))
        gl_arr = np.asarray(gl, dtype=np.float32) * 0.36
        gl_layer = Image.new("RGBA", (size, size), (*glow_rgb, 255))
        gl_layer.putalpha(Image.fromarray(np.clip(gl_arr, 0, 255).astype(np.uint8), mode="L"))
        base.alpha_composite(gl_layer)

    if shadow:
        sh = mask_img.filter(ImageFilter.GaussianBlur(radius=size * 0.022))
        sh_arr = np.asarray(sh, dtype=np.float32) * 0.55
        sh_layer = Image.new("RGBA", (size, size), (8, 11, 18, 255))
        sh_layer.putalpha(Image.fromarray(np.clip(sh_arr, 0, 255).astype(np.uint8), mode="L"))
        base.alpha_composite(sh_layer, dest=(0, int(size * 0.018)))

    c1 = np.array(col_top, dtype=np.float32)
    c2 = np.array(col_bot, dtype=np.float32)
    rgb = c1[None, None, :] * (1.0 - t[:, :, None]) + c2[None, None, :] * t[:, :, None]

    fg = np.zeros((size, size, 4), dtype=np.uint8)
    fg[:, :, :3] = np.clip(rgb, 0.0, 255.0).astype(np.uint8)
    fg[:, :, 3] = a_u8
    base.alpha_composite(Image.fromarray(fg, mode="RGBA"))
    return base


def draw_antialias_mask(size: int, fn) -> np.ndarray:
    """Draw vector shapes via Pillow onto a 2x supersampled grayscale mask."""
    ss = size * 2
    img = Image.new("L", (ss, ss), 0)
    draw = ImageDraw.Draw(img)
    fn(draw, ss)
    img = img.resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def build_header_emblem(size: int = 512) -> Image.Image:
    """Pure transparent crisp anti-aliased Tri-Spoke Core Emblem for window headers."""
    alpha = trispoke_sdf_alpha(size, scale=0.92, thickness_boost=0.012)
    lin = np.linspace(0.0, 1.0, size, dtype=np.float32)
    yy, xx = np.meshgrid(lin, lin, indexing="ij")
    t = np.clip(xx * 0.3 + yy * 0.7, 0.0, 1.0)
    c1 = np.array([246.0, 249.0, 254.0], dtype=np.float32)
    c2 = np.array([196.0, 212.0, 234.0], dtype=np.float32)
    rgb = c1[None, None, :] * (1.0 - t[:, :, None]) + c2[None, None, :] * t[:, :, None]
    out = np.zeros((size, size, 4), dtype=np.uint8)
    out[:, :, :3] = rgb.astype(np.uint8)
    out[:, :, 3] = (alpha * 255.0).astype(np.uint8)
    return Image.fromarray(out, mode="RGBA")


def build_icon_project_hub(size: int = 512, small: bool = False,
                           angle_deg: float = 0.0, spin: bool = False) -> Image.Image:
    base = make_transparent_studio_base(size, (125, 211, 252))
    boost = 0.016 + (0.030 if small else 0.0)
    # Dark rim silhouette first: without it the pale gradient washes out on
    # light desktops / start menus and the 16px frame disappears entirely.
    # The spin loop keeps a thinner rim and drops the drop shadow + glow:
    # soft alpha blobs become a speckled halo once GIF's 1-bit transparency
    # has to dither them, and the shadow looks dirty on light surfaces. The
    # gradient + rim still carry the shading people liked on the icon.
    rim = trispoke_sdf_alpha(size, scale=0.91,
                             thickness_boost=boost + (0.022 if spin else 0.038),
                             angle_deg=angle_deg)
    rim_img = Image.fromarray((np.clip(rim, 0.0, 1.0) * 255.0).astype(np.uint8), mode="L")
    rim_layer = Image.new("RGBA", (size, size), (16, 22, 32, 255))
    rim_layer.putalpha(rim_img)
    base.alpha_composite(rim_layer)
    emblem = trispoke_sdf_alpha(size, scale=0.91, thickness_boost=boost,
                                angle_deg=angle_deg)
    return composite_elevated_layer(
        base, emblem,
        col_top=(244, 248, 255),
        col_bot=(138, 198, 250),
        glow_rgb=None if spin else (96, 165, 250),
        shadow=not spin,
    )


def build_icon_bluetooth_guard(size: int = 512, small: bool = False) -> Image.Image:
    base = make_transparent_studio_base(size, (59, 130, 246))

    # Soft translucent cobalt rounded inner badge
    badge_a = draw_antialias_mask(size, lambda d, s: d.rounded_rectangle(
        [s * 0.07, s * 0.05, s * 0.93, s * 0.95], radius=s * 0.24, fill=255
    ))
    composite_elevated_layer(base, badge_a * 0.97, (56, 152, 255), (29, 92, 214), (56, 165, 250))

    # Crisp smooth-capped white Bluetooth glyph inside the cobalt badge
    def bt_fn(d: ImageDraw.ImageDraw, s: int):
        w = int(s * (0.072 if small else 0.054))
        cx, cy = s * 0.50, s * 0.50
        pts = [
            (cx - s * 0.138, cy - s * 0.126),
            (cx + s * 0.126, cy + s * 0.126),
            (cx - s * 0.006, cy + s * 0.246),
            (cx - s * 0.006, cy - s * 0.246),
            (cx + s * 0.126, cy - s * 0.126),
            (cx - s * 0.138, cy + s * 0.126),
        ]
        d.line(pts, fill=255, width=w, joint="curve")
        for px0, py0 in (pts[0], pts[-1]):
            r = w * 0.5
            d.ellipse([px0 - r, py0 - r, px0 + r, py0 + r], fill=255)

    bt_a = draw_antialias_mask(size, bt_fn)
    return composite_elevated_layer(base, bt_a, (250, 253, 255), (219, 238, 255), None)


def build_icon_keyboard_lighting(size: int = 512, small: bool = False) -> Image.Image:
    base = make_transparent_studio_base(size, (168, 85, 247))

    # Luminous aura underglow bar beneath keycap plate
    glow_bar = draw_antialias_mask(size, lambda d, s: d.rounded_rectangle(
        [s * 0.195, s * 0.685, s * 0.805, s * 0.775], radius=s * 0.045, fill=255
    ))
    composite_elevated_layer(base, glow_bar, (196, 181, 253), (56, 189, 248), (168, 85, 247))

    # Smooth rounded keyboard plate in violet-indigo-sky gradient
    plate_a = draw_antialias_mask(size, lambda d, s: d.rounded_rectangle(
        [s * 0.155, s * 0.235, s * 0.845, s * 0.655], radius=s * 0.11, fill=255
    ))
    composite_elevated_layer(base, plate_a, (167, 139, 250), (56, 189, 248), (129, 140, 248))

    # Crisp white keycaps + spacebar inside keyboard plate. At icon size the
    # 4x2 grid merges into mush, so tiny frames drop to a bold 3x2 grid with
    # wider gutters - the details survive where the old downscale lost them.
    def keys_fn(d: ImageDraw.ImageDraw, s: int):
        if small:
            for row_i, y0 in enumerate((0.305, 0.418)):
                for col_i in range(3):
                    x0 = 0.255 + col_i * 0.168
                    d.rounded_rectangle(
                        [s * x0, s * y0, s * (x0 + 0.135), s * (y0 + 0.082)],
                        radius=s * 0.024, fill=255,
                    )
            d.rounded_rectangle([s * 0.30, s * 0.535, s * 0.70, s * 0.605], radius=s * 0.026, fill=255)
            return
        for row_i, y0 in enumerate((0.315, 0.412)):
            for col_i in range(4):
                x0 = 0.245 + col_i * 0.135
                d.rounded_rectangle(
                    [s * x0, s * y0, s * (x0 + 0.105), s * (y0 + 0.068)],
                    radius=s * 0.022, fill=255,
                )
        d.rounded_rectangle([s * 0.315, s * 0.520, s * 0.685, s * 0.585], radius=s * 0.024, fill=255)

    keys_a = draw_antialias_mask(size, keys_fn)
    return composite_elevated_layer(base, keys_a, (252, 253, 255), (226, 236, 255), None)


def build_icon_spatial_hud(size: int = 512, small: bool = False) -> Image.Image:
    base = make_squircle_base(size, (245, 158, 11))

    # 3-face soft isometric Fluent cube (top bright gold, left warm amber, right deep bronze-gold)
    cx, cy, r = 0.50, 0.50, 0.285
    dx, dy = r * 0.866, r * 0.50

    top_a = draw_antialias_mask(size, lambda d, s: d.polygon([
        (s * cx, s * (cy - r)),
        (s * (cx + dx), s * (cy - dy)),
        (s * cx, s * cy),
        (s * (cx - dx), s * (cy - dy)),
    ], fill=255))
    left_a = draw_antialias_mask(size, lambda d, s: d.polygon([
        (s * (cx - dx), s * (cy - dy)),
        (s * cx, s * cy),
        (s * cx, s * (cy + r)),
        (s * (cx - dx), s * (cy + dy)),
    ], fill=255))
    right_a = draw_antialias_mask(size, lambda d, s: d.polygon([
        (s * cx, s * cy),
        (s * (cx + dx), s * (cy - dy)),
        (s * (cx + dx), s * (cy + dy)),
        (s * cx, s * (cy + r)),
    ], fill=255))

    composite_elevated_layer(base, left_a, (251, 191, 36), (217, 119, 6), (245, 158, 11))
    composite_elevated_layer(base, right_a, (217, 119, 6), (161, 82, 7), None)
    composite_elevated_layer(base, top_a, (254, 243, 199), (251, 191, 36), None)
    return base


def build_icon_orca_plugins(size: int = 512, small: bool = False) -> Image.Image:
    base = make_squircle_base(size, (52, 211, 153))

    # Stacked flowing wave ribbons representing non-planar Wave Overhangs + Unlayered Infill
    for idx, (y_center, c_top, c_bot) in enumerate([
        (0.35, (167, 243, 208), (52, 211, 153)),
        (0.50, (110, 231, 183), (16, 185, 129)),
        (0.65, (52, 211, 153), (5, 150, 105)),
    ]):
        def wave_fn(d: ImageDraw.ImageDraw, s: int, yc=y_center, phase=idx * 0.7):
            pts = []
            for k in range(65):
                t = k / 64.0
                x = s * (0.23 + 0.54 * t)
                y = s * (yc + 0.055 * math.sin(t * math.pi * 2.0 + phase))
                pts.append((x, y))
            w = int(s * 0.072)
            d.line(pts, fill=255, width=w, joint="curve")
            for px0, py0 in (pts[0], pts[-1]):
                rr = w * 0.5
                d.ellipse([px0 - rr, py0 - rr, px0 + rr, py0 + rr], fill=255)

        wa = draw_antialias_mask(size, wave_fn)
        composite_elevated_layer(base, wa, c_top, c_bot, (52, 211, 153) if idx == 1 else None)

    return base


def build_icon_arena_link(size: int = 512, small: bool = False) -> Image.Image:
    base = make_squircle_base(size, (45, 212, 191))

    def ring1(d: ImageDraw.ImageDraw, s: int):
        d.rounded_rectangle([s * 0.22, s * 0.36, s * 0.59, s * 0.64],
                            radius=s * 0.14, outline=255, width=int(s * 0.068))

    def ring2(d: ImageDraw.ImageDraw, s: int):
        d.rounded_rectangle([s * 0.41, s * 0.36, s * 0.78, s * 0.64],
                            radius=s * 0.14, outline=255, width=int(s * 0.068))

    a1 = draw_antialias_mask(size, ring1)
    a2 = draw_antialias_mask(size, ring2)
    composite_elevated_layer(base, a1, (153, 246, 228), (45, 212, 191), (45, 212, 191))
    composite_elevated_layer(base, a2, (204, 251, 241), (20, 184, 166), None)
    return base


def save_multi_ico(build, ico_path: Path, png_path: Path = None):
    """Write a true multi-size ICO where EVERY size is rendered analytically
    at its own resolution (with small-size stroke boosts) instead of being
    downscaled from one master - the old downscale path turned the 16-32px
    frames into mush and the details vanished."""
    import io
    import struct
    ico_path.parent.mkdir(parents=True, exist_ok=True)
    sizes = [(256, 256), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    frames = []
    for (w, h) in sizes:
        im = build(w, small=(w <= 32)).convert("RGBA")
        if png_path is not None and (w, h) == (256, 256):
            png_path.parent.mkdir(parents=True, exist_ok=True)
            # the companion PNG export is the LARGE crisp master, not 256
            build(2048, small=False).convert("RGBA").save(png_path, format="PNG")
        buf = io.BytesIO()
        im.save(buf, format="PNG")
        frames.append((w, h, buf.getvalue()))
    out = struct.pack("<HHH", 0, 1, len(frames))
    offset = 6 + 16 * len(frames)
    entries = b""
    data = b""
    for (w, h, px) in frames:
        wb = 0 if w >= 256 else w
        hb = 0 if h >= 256 else h
        entries += struct.pack("<BBBBHHII", wb, hb, 0, 0, 1, 32, len(px), offset)
        offset += len(px)
        data += px
    ico_path.write_bytes(out + entries + data)


def main():
    BRAND_DIR.mkdir(parents=True, exist_ok=True)

    emblem = build_header_emblem(512)
    save_multi_ico(lambda w, small=False: build_header_emblem(w),
                   BRAND_DIR / "emblem.ico", BRAND_DIR / "suite-emblem.png")

    icons = [
        ("Project Hub",       build_icon_project_hub,       ROOT / "project-hub" / "hub.ico",               BRAND_DIR / "icon-project-hub.png"),
        ("Bluetooth Guard",   build_icon_bluetooth_guard,   ROOT / "bluetooth-guard" / "btguard.ico",       BRAND_DIR / "icon-bluetooth-guard.png"),
        ("Keyboard Lighting", build_icon_keyboard_lighting, ROOT / "keyboard-lighting" / "app.ico",         BRAND_DIR / "icon-keyboard-lighting.png"),
        ("Spatial HUD",       build_icon_spatial_hud,       BRAND_DIR / "spatial-hud.ico",                  BRAND_DIR / "icon-spatial-hud.png"),
        ("Orca Plugins",      build_icon_orca_plugins,      BRAND_DIR / "orca-plugins.ico",                 BRAND_DIR / "icon-orca-plugins.png"),
        ("Arena Link",        build_icon_arena_link,        ROOT / "arena-link-windows" / "assets" / "icon.ico", BRAND_DIR / "icon-arena-link.png"),
    ]

    for _, build, ico_p, png_p in icons:
        save_multi_ico(build, ico_p, png_p)

    # Build a crisp showcase sheet so the user can inspect the entire icon family side by side
    cols = 3
    rows = 2
    sheet_w, sheet_h = 1140, 680
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (22, 25, 34, 255))
    draw = ImageDraw.Draw(sheet)

    try:
        font_hdr = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
        font_sub = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
        font_lbl = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
    except Exception:
        font_hdr = font_sub = font_lbl = ImageFont.load_default()

    # Header bar on showcase sheet with the Tri-Spoke Core Emblem
    hdr_emb = emblem.resize((36, 36), Image.Resampling.LANCZOS)
    sheet.alpha_composite(hdr_emb, dest=(44, 30))
    draw.text((94, 30), "Unified Suite App Icons", fill=(236, 239, 246, 255), font=font_hdr)
    draw.text((94, 58), "Transparent studio glyphs  ·  Tri-Spoke Core emblem  ·  Distinct per-app Fluent accents",
              fill=(154, 163, 184, 255), font=font_sub)

    cell_w = 340
    cell_h = 260
    start_x = 42
    start_y = 108

    for i, (label, build, _, _) in enumerate(icons):
        im = build(512)
        r_idx = i // cols
        c_idx = i % cols
        cx0 = start_x + c_idx * (cell_w + 18)
        cy0 = start_y + r_idx * (cell_h + 20)
        draw.rounded_rectangle(
            [cx0, cy0, cx0 + cell_w, cy0 + cell_h],
            radius=18,
            fill=(32, 36, 48, 255),
            outline=(53, 60, 78, 255),
            width=1,
        )
        big = im.resize((148, 148), Image.Resampling.LANCZOS)
        small32 = im.resize((32, 32), Image.Resampling.LANCZOS)
        small20 = im.resize((20, 20), Image.Resampling.LANCZOS)
        sheet.alpha_composite(big, dest=(cx0 + 70, cy0 + 26))
        sheet.alpha_composite(small32, dest=(cx0 + 246, cy0 + 68))
        sheet.alpha_composite(small20, dest=(cx0 + 252, cy0 + 114))

        tb = draw.textbbox((0, 0), label, font=font_lbl)
        tw = tb[2] - tb[0]
        draw.text((cx0 + (cell_w - tw) // 2, cy0 + 204), label, fill=(236, 239, 246, 255), font=font_lbl)

    preview_path = BRAND_DIR / "suite-icons-preview.png"
    sheet.convert("RGB").save(preview_path, format="PNG")

    # Profile-photo exports: the SHADED glyph (rim + gradient + glow, exactly
    # the desktop icon treatment people liked) on full transparency. The old
    # flat emblem and the old squircle-plate versions both read wrong once a
    # site flattened them; this is the look the spin loop uses too.
    build_icon_project_hub(2048).save(BRAND_DIR / "project-hub-icon-2048.png")
    build_icon_project_hub(1024).save(BRAND_DIR / "project-hub-icon-1024.png")
    build_icon_project_hub(2048).save(BRAND_DIR / "tri-spoke-emblem-2048.png")
    build_icon_project_hub(1024).save(BRAND_DIR / "tri-spoke-emblem-1024.png")

    # Seamless 360-degree spin loop of the shaded glyph - NO background.
    # GIF transparency is 1-bit, so the soft rim/glow edges are ordered-
    # dithered (a stipple that blends on light or dark surfaces). The
    # animated WebP keeps full 8-bit alpha for anyone who wants the
    # quality-exact version.
    export_spin_loops()

    print("Generated icons successfully:", preview_path)


BAYER4 = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)


def export_spin_loops(size: int = 512, frames: int = 240, ms: int = 50):
    """docs/brand/tri-spoke-spin.gif (transparent) + .webp (full alpha)."""
    imgs = []
    for i in range(frames):
        ang = 360.0 * i / frames
        imgs.append(build_icon_project_hub(size, angle_deg=ang, spin=True).convert("RGBA"))

    # ---- animated WebP: exact, no dithering ----
    imgs[0].save(
        BRAND_DIR / "tri-spoke-spin.webp",
        save_all=True,
        append_images=imgs[1:],
        duration=ms,
        loop=0,
        quality=80,
        method=4,
    )

    # ---- transparent GIF: shared palette + Bayer alpha dither ----
    # Palette from frame 0 with invisible pixels pre-filled by the glyph's
    # darkest visible tone so no palette entry is wasted on "nothing".
    rgb0 = imgs[0].convert("RGB")
    pal_src = rgb0.quantize(colors=255, method=Image.Quantize.MEDIANCUT)

    gif_frames = []
    trans_idx = 255
    bayer = np.array(BAYER4, dtype=np.float32)
    pal_bytes = pal_src.getpalette()
    for im in imgs:
        q = im.convert("RGB").quantize(palette=pal_src, dither=Image.Dither.NONE)
        idx = np.frombuffer(q.tobytes(), dtype=np.uint8).reshape(im.size[1], im.size[0]).copy()
        a = np.asarray(im.getchannel("A"), dtype=np.float32)
        thr = (bayer[np.arange(im.size[1])[:, None] % 4,
                     np.arange(im.size[0])[None, :] % 4] + 0.5) * 8.0
        # The faint outer glow stipples into a huge noisy annulus that LZW
        # cannot compress; shed it and dither only the real shape edge where
        # anti-aliasing matters. The rim + gradient + drop shadow - the
        # shading people liked on the icon - are untouched.
        idx[(a <= 12.0) | (a < 90.0) | ((a < 250.0) & (a < thr))] = trans_idx
        frame = Image.frombytes("P", im.size, idx.tobytes())
        frame.putpalette(pal_bytes)
        gif_frames.append(frame)

    gif_frames[0].save(
        BRAND_DIR / "tri-spoke-spin.gif",
        save_all=True,
        append_images=gif_frames[1:],
        duration=ms,
        loop=0,
        disposal=2,
        transparency=trans_idx,
        optimize=False,
    )
    import os
    print("spin gif bytes:", os.path.getsize(BRAND_DIR / "tri-spoke-spin.gif"))
    print("spin webp bytes:", os.path.getsize(BRAND_DIR / "tri-spoke-spin.webp"))


if __name__ == "__main__":
    main()
