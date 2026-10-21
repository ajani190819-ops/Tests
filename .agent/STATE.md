# Repository Current State (`STATE.md`)

**Active Working Branch:** `arena/e895b94f-tests`
**Last Updated:** 2026-10-21

## Active Component Status

| Component | Path | Version | Build / Test Status |
| :--- | :--- | :--- | :--- |
| **Project Hub (`ProjectHub.exe`)** | `project-hub/` | `2.3.0` | **GREEN** — Sub-pixel anti-aliased `14px` rounded cards (`SmoothRoundRect`), Tri-Spoke Core Emblem (`docs/brand/emblem.ico`), embedded 32-bit suite icons (`hub.ico`..`icon.ico`), smart existing-shortcut/process path resolution, and one-click `Update Hub` self-updater |
| **Bluetooth Guard (`BluetoothGuard.exe`)** | `bluetooth-guard/` | `2.3.0` | **GREEN** — Sub-pixel anti-aliased `14px` rounded cards, smooth sliding pill toggles (`DrawPillToggle`), Tri-Spoke Core Emblem, `btguard.ico`, and reliable CoreAudio `KSPROPERTY_ONESHOT_RECONNECT` manual connect |
| **Keyboard Lighting (`Tray.ps1` + `ui_controls.cs.txt`)** | `keyboard-lighting/` | `1.3.0` | **GREEN** — Soft Slate palette, `14px` rounded cards, `11px` rounded controls, anti-aliased `Theme.DrawSuiteEmblem` title bar + rounded window caption buttons, and new suite `keyboard-lighting/app.ico` |
| **Suite UI & App Icon System** | `docs/UI_STYLE_GUIDE.md`, `tools/generate_suite_icons.py`, `docs/brand/` | `2.0.0` | **GREEN** — Unified brushed-slate Windows 11 superellipse squircle icon family across all 6 apps + analytic SDF Tri-Spoke Core Emblem |
| **OrcaSlicer Installer** | `Orca-Plugins.bat` | `2.2.0` | **GREEN** — Pure CRLF, auto-detects active GitHub branch |
| **Wave Overhangs Plugin** | `plugins/wave-overhangs/` | `0.0.51` | **GREEN** — 100% unit tests passing |
| **Unlayered Infill Plugin** | `plugins/unlayered-infill/` | `0.4.10` | **GREEN** — 100% unit tests passing |
| **Arena Link Windows** | `arena-link-windows/` | `0.1.0` | **GREEN** — Soft Slate rounded CSS + inline SVG Tri-Spoke Core Emblem + `assets/icon.ico` |
| **Spatial HUD (MC 26.3)** | `spatial-hud-template-26.3/` | `1.0.2+mc26.3` | **GREEN** — Builds cleanly with Java 25 (`./gradlew build`) |

## Verification Gate

Run `python3 tools/check_all.py` to execute the entire repository verification suite (hygiene, CRLF batch checks, Orca plugins, Windows desktop apps & icon headers, `arena-link-windows` Node tests, and `spatial-hud-template-26.3` Gradle build).
