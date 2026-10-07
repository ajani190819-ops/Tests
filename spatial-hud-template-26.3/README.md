# Spatial HUD (canonical build project)

**Spatial GUI, but for the Minecraft HUD.** Hotbar, hearts, hunger, armor, air,
XP, mount bars, and the held-item name are captured exactly as Minecraft draws
them, then presented on a real 3D panel in front of the player. It is
**client-side only**; servers do not need it. Press **H** to toggle it.

You never need a local Java or Gradle installation. GitHub Actions builds every
change and replaces the jar at this permanent download link:

**Download (always the newest successful build):**
https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

## v0.5 — real world-space HUD

This is a rendering architecture rewrite, not another 2D position adjustment.
It follows the same high-level approach as Spatial GUI:

- **Isolated vanilla HUD capture:** Fabric HUD elements extract into their own
  transparent render state. Minecraft continues to render every native icon,
  item decoration, number, resource-pack texture, and compatibility detail.
- **Actual 3D texture panel:** that render state is drawn into its own texture,
  then submitted as a camera-relative quad using Minecraft 26.3's RenderPearl
  rendering pipeline and the real world projection matrix.
- **True perspective and parallax:** turning your view changes the perspective
  of a physical panel rather than merely rotating a flat 2D HUD. Smooth
  frame-time-based sway gives it the same settled feel as Spatial GUI.
- **Clean presentation:** a transparent glass-like backing is composited with
  the strip before it is rendered in world space.
- **Safe vanilla fallback:** H immediately returns every wrapped element to
  vanilla. Any rendering exception disables Spatial HUD and restores the
  ordinary HUD instead of taking down the game.

## Configuration

Open **Mod Menu → Spatial HUD → Configure**. The Cloth Config screen has the
same familiar structure as Spatial GUI:

- **General** — enable/disable, FOV-aware sizing, backing panel
- **First-Person Placement** — panel distance, scale, vertical and side offset
- **Motion** — parallax amount, response time, and rotation
- **Visible HUD Parts** — individual hotbar/bar/XP/mount/held-name toggles

There is also an unbound **Open Spatial HUD Config** control; assign a key in
Minecraft's Controls screen if you prefer a keybind. Settings are stored in
`config/spatialhud.json`. Older configurations automatically receive the v0.5
3D placement defaults while preserving their visibility and motion choices.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config.
Mod Menu is optional but recommended for the one-click Configure button.
Spatial GUI already supplies Cloth Config and Mod Menu in the target modpack.

## Implementation notes

The 26.3 implementation uses two small, targeted client mixins only to route
an isolated `GuiRenderer` to the capture texture and draw it before the main
GUI pass. HUD selection itself remains on Fabric's public
`HudElementRegistry` API. The world-space capture/presentation pattern was
studied from [Spatial GUI](https://github.com/tastytrash/Spatial-GUI), which is
licensed MIT.
