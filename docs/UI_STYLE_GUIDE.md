# Suite UI Style Guide — Vanguard Slate (Destiny 2-Inspired Desktop Language)

Every desktop app and companion tool in this repository belongs to one visual family so you can tell at a glance that they are part of the same suite:

- **Project Hub** (`project-hub/ProjectHub.exe`)
- **Bluetooth Guard** (`bluetooth-guard/BluetoothGuard.exe`)
- **Keyboard Lighting** (`keyboard-lighting/Tray.ps1`)
- **Arena Link Windows** (`arena-link-windows/`)

Reference concept mockup: [`docs/ui-concepts/suite-ui-concept.png`](ui-concepts/suite-ui-concept.png)

---

## 1. Core Design Philosophy

Inspired by **Destiny 2's Director & Settings menus** — architectural, calm, sharp, and readable, with **zero visual clutter**:

1. **Sharp Architectural Geometry over Bubbly Pills:** Surfaces, buttons, and status badges use crisp **0px–2px rectangular corners** with `1px` hairline borders rather than heavy drop shadows or rounded bubble buttons.
2. **Luminous Edge Bars Signal Meaning:**
   - Every item card has a **full-height `6px` solid status bar** flush along its left edge showing live state at a glance.
   - Important header buttons carry a **crisp `3px` luminous top-edge bar** (`Solar Gold` for app self-updates, `Arc Cyan` for primary suite actions).
3. **Subtle Celestial Backdrop, Never Distracting:** A deep obsidian-navy ground (`#0B101B`) with ultra-faint (`~4–6%` contrast) compass circles, axis crosshairs, outer hairline frame, and tiny edge diamond ticks gives depth without competing with text.
4. **Plain, Calm Copy:** Every card shows one clear title, one short subtitle/summary line, one status tag on the top right, and clean uppercase action tiles on the bottom right. Never stuff raw `C:\Users\...` file paths into normal status text.

---

## 2. Shared Color Palette Tokens

| Token | Role | Hex | Win32 `RGB(r, g, b)` | CSS Variable |
| :--- | :--- | :--- | :--- | :--- |
| **Canvas Void** | Main window background | `#0B101B` | `RGB(11, 16, 27)` | `--bg-canvas: #0b101b;` |
| **Director Line** | Subtle background compass rings / axes / outer frame | `#141E30` | `RGB(20, 30, 48)` | `--line-director: #141e30;` |
| **Plate Fill** | Card & button resting fill | `#101726` | `RGB(16, 23, 38)` | `--panel: #101726;` |
| **Plate Hover** | Card / button hover fill | `#182338` | `RGB(24, 35, 56)` | `--panel-hover: #182338;` |
| **Hairline Border** | Card & divider resting border | `#26354B` | `RGB(38, 53, 75)` | `--border: #26354b;` |
| **Button Border** | Action button resting border | `#31445F` | `RGB(49, 68, 95)` | `--border-btn: #31445f;` |
| **Hover Rim** | Crisp Destiny hover border highlight | `#DCE8F8` | `RGB(220, 232, 248)` | `--border-hot: #dce8f8;` |
| **Ghost White** | Primary uppercase headers & active labels | `#EEF4FC` | `RGB(238, 244, 252)` | `--text-primary: #eef4fc;` |
| **Slate Mist** | Card subtitles & calm detail text | `#8CA2C0` | `RGB(140, 162, 192)` | `--text-secondary: #8ca2c0;` |
| **Muted Telemetry** | `//` header & footer telemetry text | `#687E9C` | `RGB(104, 126, 156)` | `--text-muted: #687e9c;` |
| **Arc Cyan** | Primary global action / manual session active | `#38BDF8` | `RGB(56, 189, 248)` | `--accent-arc: #38bdf8;` |
| **Strand Emerald** | Active / Running / Connected / Up to Date | `#2CE38B` | `RGB(44, 227, 139)` | `--status-ok: #2ce38b;` |
| **Emerald Tint** | `ACTIVE` / `UP TO DATE` badge background | `#0D2B24` | `RGB(13, 43, 36)` | `--status-ok-bg: #0d2b24;` |
| **Solar Gold** | Self-update button bar / `UPDATE READY` | `#F5B942` | `RGB(245, 185, 66)` | `--status-warn: #f5b942;` |
| **Solar Tint** | `UPDATE READY` badge background | `#2C2210` | `RGB(44, 34, 16)` | `--status-warn-bg: #2c2210;` |
| **Crimson Alert** | Frozen / Disconnected error alert | `#F87171` | `RGB(248, 113, 113)` | `--status-err: #f87171;` |
| **Crimson Tint** | Error / `FROZEN` badge background | `#2E141B` | `RGB(46, 20, 27)` | `--status-err-bg: #2e141b;` |

---

## 3. Typography & Hierarchy

All Windows desktop apps use **Segoe UI** (built into every Windows 10/11 PC with zero extra font installs) with explicit letter spacing for titles and tags:

1. **Window Title (`18–20px` Semibold, Uppercase, Wide-Tracked):**
   - Rendered with explicit wide character spacing (`P R O J E C T   H U B`, `B L U E T O O T H   G U A R D`).
2. **Telemetry Subtitle (`11px` Regular/Semibold, Uppercase, Tracked):**
   - Uses double-slash divider syntax: `SUITE CONTROL  //  v2.2`, `AUDIO SHIELD  //  ACTIVE`.
3. **Card Title (`14px` Semibold, Uppercase, Medium-Tracked):**
   - `B L U E T O O T H   G U A R D`, `K E Y B O A R D   L I G H T I N G`, `S P A T I A L   H U D`, etc.
4. **Card Subtitle / Status Detail (`12px` Regular, Normal Tracking):**
   - Plain sentence case for effortless reading (`Phone audio protection`, `Wave Overhangs 0.0.51 & Unlayered Infill 0.4.10`).
5. **Action Buttons & Status Badges (`10.5–11px` Semibold, Uppercase):**
   - Short 1–2 word labels: `OPEN`, `UPDATE`, `STOP`, `FOLDER`, `CONNECT`, `DISCONNECT`, `UPDATE HUB`, `UPDATE ALL`.

---

## 4. Standard Layout Anatomy

1. **Header Bar (`74px` scaled by DPI):**
   - Left: **8-point Wayfinder Compass Emblem** inside a hairline circle + wide-tracked app title + `//` telemetry subtitle.
   - Right: Rectangular `34px`-tall header action tiles with `3px` colored top bars on featured actions (`UPDATE HUB` in Solar Gold, primary global action in Arc Cyan).
   - Bottom edge: `1px` full-width hairline divider (`#1E2B3E`).
2. **Card List (`78–82px` card height, `12px` vertical spacing):**
   - Sharp rectangular card (`#101726`, `1px` `#26354B` border).
   - `6px` full-height status bar along left edge (`Emerald`, `Solar Gold`, `Arc Cyan`, `Crimson`, or `Muted Slate`).
   - Minimal monochrome vector line icon (`24–28px`) in the left gutter.
   - Uppercase title + clean subtitle on the left; bordered status tag (`● ACTIVE`, `● UPDATE READY`, `● READY`) on the top right; crisp rectangular action buttons aligned along the bottom right.
3. **Footer Status Bar (`42px` scaled by DPI):**
   - Full-width `1px` top divider line (doubling as a crisp `2px` luminous cyan progress bar along the top edge whenever a background download is running).
   - Colored `7px` status dot on the left + uppercase status message (`ALL SUITE MODULES VERIFIED  //  GITHUB CONNECTED`).

---

## 5. Self-Update Standard for Standalone `.exe` Apps

- Standalone launcher hubs (`ProjectHub.exe`) must expose a visible top-level **`UPDATE HUB`** button right in the header bar (highlighted with the **Solar Gold** top accent bar) rather than burying self-update inside hidden menus.
- On startup and on **`REFRESH`**, `ProjectHub.exe` compares its compiled `HUB_VERSION` against `HUB_VERSION` in `project-hub/src/project_hub.c` on GitHub so you immediately see when a newer `ProjectHub.exe` is ready, and clicking **`UPDATE HUB`** downloads, atomically swaps (`MoveFileExW`), and relaunches the updated `.exe` in one click.
