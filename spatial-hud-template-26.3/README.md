# Spatial HUD (canonical build project)

**Spatial GUI-inspired HUD behavior without touching the rest of the GUI.**
Spatial HUD controls only the vanilla bottom strip: hotbar, hearts, hunger,
armor, air, XP, mount bars, and held-item name. It is client-side only;
servers do not need it. Press **H** to toggle it.

GitHub Actions builds every change and replaces the jar at this fixed link:

**Download (always the newest successful build):**
https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

## v0.5 — look-down HUD and compatibility-first behavior

The normal gameplay view should be for the world, not UI. By default Spatial
HUD hides the bottom strip just below the screen. Deliberately look down to
bring it smoothly into view; keep looking down to see the complete panel.

- **Reveal start pitch:** 18° down
- **Fully visible pitch:** 48° down
- **Normal/level/upward view:** the complete Spatial HUD is below the screen
- **Smooth transition:** it slides in and out using the exact pitch, not a
  sudden on/off switch

The configuration screen offers separate controls for the two reveal angles,
hidden distance, panel placement, FOV-aware sizing, motion, and individual HUD
parts.

### Compatibility boundary

This mod deliberately has **no renderer mixins, no framebuffer redirects, no
screen hooks, and no world-render passes.** It uses Fabric's public HUD element
API only. With **Gameplay Only** enabled (the default), it delegates directly
to vanilla whenever another screen is open. That includes:

- inventory, creative inventory, chest, crafting, furnace, anvil, and other
  container screens
- chat, pause, title, Mod Menu, Cloth Config, and all mod configuration pages
- modded interfaces such as Jade, Inventory Profiles Next, OptiGUI, and REI/JEI
  style overlays

Spatial HUD never registers over **minimap**, **world map**, **FPS counter**,
**debug overlay**, **crosshair**, **chat**, **boss bar**, **advancement**, or
**status-effect overlay** layers. Xaero's Minimap/World Map and your FPS/debug
counter are therefore outside its render scope.

### Bottom-HUD companion mods

These modpack entries intentionally alter the same bottom HUD region:
**Bedrock Hotbar**, **Immersive Hotbar**, **Detail Armor Bar Reconstructed**,
**DualBar**, **Status Effect Bars**, **AppleSkin**, **Armor Indicator**,
**Mount Opacity**, **Durability Warner HUD**, and **Async Hotbars**. Spatial HUD
wraps the final Fabric HUD element, so compatible visual changes normally move
with it; Async Hotbars changes data handling rather than drawing and remains
outside the render path. The visual layout winner for a particular bottom-strip
piece is determined by Fabric HUD registration order, which differs by version.
Use Spatial HUD's **Visible HUD Parts** switches to let a companion mod own a
part if two mods draw it twice.

## Configuration

Open **Mod Menu → Spatial HUD → Configure**. The underlying configuration file
is `config/spatialhud.json`. An unbound **Open Spatial HUD Config** entry is
also available under Minecraft Controls if you want to assign a key.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config.
Mod Menu is optional but recommended for the Configure button.
