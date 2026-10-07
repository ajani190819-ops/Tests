# Spatial HUD (build project — canonical)

**Spatial GUI, but for your HUD.** The bottom HUD strip — hotbar, hearts,
hunger, armor, air, XP, mount bars, and held-item name — becomes a compact
floating panel instead of being glued to the bottom edge. It is **client-side
only**; servers do not need it. **H** toggles it in-game.

This folder is the active build project. **You never need to build locally** —
GitHub Actions builds every change and replaces the jar at this permanent link:

**Download (always the latest build):**
https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

## v0.4 — compact and configurable

This release is tuned around the first in-game feedback:

- **Smaller and lower by default.** The v0.3 untouched defaults automatically
  migrate to the new compact, lower placement. Custom v0.3 placement values
  are respected.
- **Smooth frame-time parallax.** Sway now uses the same time-based
  exponential filtering approach as Spatial GUI, rather than updating only at
  Minecraft's 20-tick rhythm. Every wrapped HUD part uses the same frame pose.
- **Correct rotation pivot.** The small parallax turn now happens around the
  panel centre, eliminating the old off-centre jump.
- **Mod Menu configuration.** Spatial HUD now has a normal Cloth Config screen
  in **Mod Menu → Spatial HUD → Configure**, matching Spatial GUI's familiar
  configuration experience. It also adds an **unbound** `Open Spatial HUD
  Config` entry under Controls if you prefer a keybind.
- **FOV-aware sizing.** Enabled by default, using the same comfortable
  FOV-compensation curve as Spatial GUI.

The config screen groups the controls into **General**, **First-Person
Placement**, **Motion**, and **Visible HUD Parts**. The underlying file stays
at `config/spatialhud.json`.

## How it works

MC 26.x uses render-state extraction with 2D affine GUI poses. Spatial HUD uses
Fabric's official HUD API only:

- `HudElementRegistry.replaceElement` wraps every vanilla bottom-strip element
  — when disabled it is a direct vanilla passthrough (no mixins).
- A shared perspective pose maps the strip to its configured width, distance,
  and lower-screen height. The GUI pipeline currently has 2D affine poses, so
  this is a screen-parallel floating panel rather than a perspective trapezoid.
- A subtle backing panel is drawn under the strip. Every render error is caught;
  the mod disables itself and restores vanilla HUD rather than crashing the
  game.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config
(Spatial GUI already requires Cloth Config). Mod Menu is optional but strongly
recommended; it supplies the one-click **Configure** button.

## Source layout

- `src/client/java/` — client-only implementation and Mod Menu entrypoint
- `src/main/resources/` — mod metadata and English config labels
- `config/spatialhud.json` — generated user settings (not in this repository)
