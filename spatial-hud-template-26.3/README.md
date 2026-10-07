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

### Safe look-down plane tilt

The default **Tilt HUD Plane Toward Look-Down** setting adds a deliberately safe
2.5D floor-plane cue. Near the horizon, the spatial hotbar/panel is vertically
foreshortened as though it is edge-on; as you look down it fills out, reaching
its normal height at **65° down** by default. This makes looking toward the
panel feel more like looking perpendicular to a floating horizontal plane,
without restoring the rejected world/UI capture renderer.

This is intentionally an affine GUI effect, not a world-rendered object:
minimap, FPS/debug text, screens, and unrelated overlays remain outside it.
Change **Face-On Look-Down Pitch** to require a steeper or shallower view, or
turn the tilt off if you prefer the previous front-facing panel.

**Taper Backing Plate Width** is also enabled by default. It draws the panel
backing as a near/far trapezoid: its upper (far) edge begins at **48%** of the
near edge near the horizon and widens smoothly to a rectangle at the face-on
pitch. The vanilla hotbar/icons are intentionally not texture-warped; doing so
requires the world/UI capture path that previously broke modded screens.
Companion status bars protected by the AppleSkin/Detail Armor compatibility
setting stay native while revealed so their own overlays remain coherent.

The configuration screen offers separate controls for the two reveal angles,
hidden distance, panel placement, FOV-aware sizing, motion, companion-mod
layout protection, and individual HUD parts.

### AppleSkin + Detail Armor Bar Reconstructed

For the tested Minecraft 26.3 Fabric releases—**AppleSkin 3.0.10** and
**Detail Armor Bar Reconstructed 5.3.2**—Spatial HUD enables a narrow
compatibility layout by default. Those mods add their visuals *inside* the
vanilla health, food, air, and armor extraction calls, rather than as unrelated
Fabric HUD elements. When either matching mod is loaded, Spatial HUD:

- continues to hide the affected status group until the look-down reveal starts;
- draws the complete adjacent health/food/air/armor group in its native
  in-game layout while revealed, including AppleSkin's overlays and Detail
  Armor Bar Reconstructed's armor renderer;
- leaves the spatial hotbar, XP, held-item name, panel, minimap, FPS/debug text,
  and every other GUI layer alone.

This keeps AppleSkin saturation/held-food indicators aligned with hunger and
prevents the armor renderer from being composited through a separate transformed
status-bar pass. Turn off **Bottom-HUD Compatibility → Keep AppleSkin and Detail
Armor Bars Native** only if you deliberately prefer the old behavior where all
status bars are spatially scaled together.

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
**Mount Opacity**, **Durability Warner HUD**, and **Async Hotbars**. The two
explicitly supported 26.3 status-bar integrations are described above.
Async Hotbars changes data handling rather than drawing and remains outside the
render path. Other bottom-HUD mods can still choose a different Fabric layer or
modify the same vanilla root, so their visual result must be checked with the
exact jar versions in use. Use Spatial HUD's **Visible HUD Parts** switches to
let a companion mod own a part if two mods draw it twice.

## Configuration

Open **Mod Menu → Spatial HUD → Configure**. The underlying configuration file
is `config/spatialhud.json`. An unbound **Open Spatial HUD Config** entry is
also available under Minecraft Controls if you want to assign a key.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config.
Mod Menu is optional but recommended for the Configure button.
