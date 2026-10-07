# Spatial HUD (build project — canonical)

**Spatial GUI, but for your HUD.** The bottom HUD strip (hotbar, hearts,
hunger, armor, air, XP, mount bars, held-item name) rendered as a panel
floating in front of you instead of glued to the screen edge. Client-side
only. **H** toggles it in-game.

This folder is the active build project. **You never need to build locally** —
GitHub Actions builds it on every change and publishes the jar here:

**Download (always the latest build):**
https://github.com/ajani190819-ops/Tests/releases/tag/spatial-hud-latest

## How it works (v0.3, MC 26.3)

MC 26.x rebuilt the GUI pipeline around render-state extraction with 2D
affine transforms, so the mod now uses the official Fabric HUD element API:

- Each vanilla bottom-strip element is wrapped with
  `HudElementRegistry.replaceElement` — spatial pose when enabled, perfect
  vanilla passthrough when disabled (zero mixins in this mod!)
- The pose is real perspective math: focal length from the current FOV,
  `distance`/`planeWidth`/`height` in blocks, so the panel behaves like a
  screen-parallel plane floating in the world
- Look-lag "sway" — the panel drifts slightly when you turn, then settles
- Translucent backing panel behind the strip
- Any render error → the mod logs once and disables itself; vanilla HUD
  returns. It will never crash your game over a HUD.

Config: `config/spatialhud.json` (distance, planeWidth, height, sway,
per-element toggles, showPanel).

## Source layout (official 26.x template structure)

- `src/client/java/` — all mod code (client-only mod)
- `src/main/resources/` — fabric.mod.json, lang
- Split environment source sets per the official template; no mappings
  line (26.x Minecraft is unobfuscated)

## Known limits / next steps

- **Tilt**: the 26.x GUI pose API is 2D affine, so a *tilted* 3D plane
  (perspective trapezoid) needs a v0.4 trick (e.g., affine slice columns or
  a world-space custom renderer). v0.3's plane is screen-parallel — real
  distance, real scale, sway, no tilt.
- Feedback wanted: position, size, sway strength, panel look — all tunable.
