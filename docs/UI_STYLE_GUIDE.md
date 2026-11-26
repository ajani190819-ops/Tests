# Suite UI & Icon Style Guide — Soft Slate Language

Every desktop application in this repository shares one calm, approachable, unified design language and a cohesive app icon family (like Microsoft Fluent or Google Workspace):

- **Project Hub** (`project-hub/ProjectHub.exe`, icon: `project-hub/hub.ico`)
- **Bluetooth Guard** (`bluetooth-guard/BluetoothGuard.exe`, icon: `bluetooth-guard/btguard.ico`)
- **Prism Lighting** (`keyboard-lighting/Tray.ps1` + `keyboard-lighting/ui_controls.cs.txt`, icon: `keyboard-lighting/app.ico`) — public Hub label; own-app branding unchanged. The owner removed the fresh-log edit prerequisite; this does not expand the current flicker-repair scope.
- **Spatial HUD** (`docs/brand/spatial-hud.ico`)
- **Undahang** (`docs/brand/undahang-wave-v1.ico`)
- **Unlayered Infill** (`docs/brand/orca-plugins.ico`, unchanged legacy asset path)
- **Arena Link Windows** (`arena-link-windows/`, icon: `arena-link-windows/assets/icon.ico`)

 Suite icon family preview: [`docs/brand/suite-icons-preview.png`](brand/suite-icons-preview.png) · Master suite emblem: [`docs/brand/suite-emblem.png`](brand/suite-emblem.png)

---

## 1. Core Principles

1. **Soft, Rounded Geometry — Never Harsh or Sharp:**
   - Windows use native **Windows 11 full rounded corners** (`DWMWCP_ROUND = 2`).
   - Content cards use **`14px` smooth anti-aliased rounded corners** (`border-radius: 14px`).
   - Action buttons, dropdown pickers, and color swatches use **`9px–10px` rounded corners**.
   - Status badges and toggle switches use **full smooth capsule pills** (`999px` radius).
   - Never use sharp `0px` box corners, harsh white borders, or busy background crosshairs.
2. **Suite Tri-Spoke Core Emblem Everywhere (No Compass Icon):**
   - Every app header and icon watermark uses the **Tri-Spoke Core Emblem** (`docs/brand/suite-emblem.png` / `docs/brand/emblem.ico`: outer thin ring, three 120-degree curved arcs separated at 12, 4, and 8 o'clock, three double-parallel radial spokes, and a center disc with an upward triangle cutout). Never use the old 8-point compass emblem.
3. **Native Header Emblem Engine (One Source, No Forks):**
   - Native Win32 headers use [`shared/win32/suite_emblem.c`](../shared/win32/suite_emblem.c), included once by the app's unity-build root and called through a tiny app-local placement adapter.
   - The engine owns the analytic geometry, supersampled cache, thirty-second time helper, palette choice, alpha compositing, and cache release. Dark slate headers use the canonical Platinum-Ice/Soft-Sky gradient; light themes use the app's accessible accent-to-text gradient.
   - Header paint code must initialize a 32-bit DIB with `SuiteEmblemDibPixel(COL_BG)`, never a raw `COLORREF`. This preserves transparent compositing and prevents the opaque brown/gold square regression.
   - A future native app adds an adapter and the source include; it must not copy SDF, cache, rotation, or pixel-blending math. Add the engine to that target's `TARGET_SHARED_SOURCES` build-cache entry too.
4. **Cohesive App Icon Suite (`tools/generate_suite_icons.py`):**
   - Every app icon is built on the **exact same brushed dark slate Windows 11 superellipse squircle** with a subtle recessed Tri-Spoke Core motif, soft top-left studio lighting, and a distinct per-app Fluent accent color:
     - **Project Hub:** Platinum-Ice & Soft Sky (`#93C5FD` / `#F8FAFC`) — full Tri-Spoke Core Medallion
     - **Bluetooth Guard:** Soft Cobalt-Sky (`#60A5FA` -> `#2563EB`) — rounded Bluetooth shield badge
     - **Prism Lighting:** Soft Violet-Sky Aura (`#A78BFA` -> `#38BDF8` + warm amber underglow) — rounded keycap & lightbar glyph
     - **Spatial HUD:** Warm Amber-Gold (`#FDE68A` -> `#D97706`) — soft isometric 3D cube
     - **Undahang:** Mint–Ice Cyan — abstract rising wave-overhang stroke with a shorter underside echo; owner-approved artwork, not a literal print sample
     - **Unlayered Infill:** Soft Mint-Emerald (`#6EE7B7` -> `#10B981`) — existing three non-planar wave ribbons, unchanged
     - **Arena Link:** Soft Teal-Mint (`#5EEAD4` -> `#14B8A6`) — rounded interlocking link glyph
5. **Calm, Natural Typography:**
   - Use natural **Title Case** (`Project Hub`, `Bluetooth Guard`, `Prism Lighting`) and plain human sentences rather than shouty wide-spaced all-caps strings or raw `C:\Users\...` file paths.

---

## 2. Shared Color Palette Tokens

| Token | Role | Hex | Win32 `RGB(r, g, b)` | CSS Variable |
| :--- | :--- | :--- | :--- | :--- |
| **Canvas Slate** | Soft low-glare window background | `#181B24` | `RGB(24, 27, 36)` | `--bg-canvas: #181b24;` |
| **Card Surface** | Rounded card background | `#222734` | `RGB(34, 39, 52)` | `--panel: #222734;` |
| **Control Surface** | Secondary button / input fill | `#2B3142` | `RGB(43, 49, 66)` | `--panel-raised: #2b3142;` |
| **Control Hover** | Soft hover state fill | `#343C50` | `RGB(52, 60, 80)` | `--panel-hover: #343c50;` |
| **Soft Hairline** | Subtle card & button border | `#3A4256` | `RGB(58, 66, 86)` | `--border: #3a4256;` |
| **Hover Border** | Gentle hover border highlight | `#525E7A` | `RGB(82, 94, 122)` | `--border-hot: #525e7a;` |
| **Primary Text** | Titles & button labels | `#ECEFF6` | `RGB(236, 239, 246)` | `--text-primary: #eceff6;` |
| **Secondary Text** | Subtitles & calm status copy | `#9AA3B8` | `RGB(154, 163, 184)` | `--text-muted: #9aa3b8;` |
| **Primary Sky** | Primary action buttons & active toggles | `#38BDF8` | `RGB(56, 189, 248)` | `--accent: #38bdf8;` |
| **Accent Indigo** | Secondary gradient partner (sliders/tabs) | `#818CF8` | `RGB(129, 140, 248)` | `--accent-indigo: #818cf8;` |
| **Active Mint** | Running / Connected / Up to Date | `#34D399` | `RGB(52, 211, 153)` | `--status-ok: #34d399;` |
| **Warm Amber** | `Update Hub` & `Update Ready` | `#FBBF24` | `RGB(251, 191, 36)` | `--status-warn: #fbbf24;` |
| **Soft Coral** | Disconnect / Frozen alert | `#F87171` | `RGB(248, 113, 113)` | `--status-err: #f87171;` |

---

## 3. Regenerating All Suite Icons

Whenever branding or icon geometry changes, run:

```bash
python3 tools/generate_suite_icons.py
```

This regenerates all multi-resolution `.ico` files (`256, 64, 48, 32, 24, 16` in 32-bit RGBA), PNG previews, `docs/brand/emblem.ico`, and `docs/brand/suite-icons-preview.png`. Recompile `ProjectHub.exe` and `BluetoothGuard.exe` afterward so their embedded `.rc` icon resources update inside the standalone `.exe` binaries too.

Undahang's approved lossless source is `docs/brand/icon-undahang-wave-v1.png`.
`build_icon_undahang` samples this RGBA master for each ICO frame; the generator
never overwrites the approved source with an enlarged derivative. Other icons
retain their existing analytic builders. Keep Undahang's two-stroke silhouette
distinct from Unlayered Infill's three ribbons, preserve the transparent outer
margin, and keep the shared native header emblem engine unchanged. The family
preview grows with the icon list so new entries are not clipped off the sheet.
