# Spatial HUD (canonical build project)

**Spatial GUI-inspired HUD behavior without touching the rest of the GUI.**
Spatial HUD controls only the vanilla bottom strip: hotbar, hearts, hunger,
armor, air, XP, mount bars, and held-item name. It is client-side only;
servers do not need it. Press **H** to toggle it.

GitHub Actions builds every change and replaces the jar at this fixed link:

**Download (always the newest successful build):**
https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

### Windows: replace the Downloads copy automatically

Run [`Get-Latest-SpatialHUD.ps1`](Get-Latest-SpatialHUD.ps1) whenever you want
the latest successful build. It downloads the fixed release asset to
`Downloads\spatial-hud-1.0.0.jar`, verifies that it is a JAR, then removes only
older `spatial-hud*.jar` copies in Downloads. It never touches your Modrinth
instance or any other mod. After it finishes, use Modrinth App → your instance
→ **Mods** → **Add content** → **From file**, and choose that one jar.

To download and run the script from a fresh PowerShell window:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
$script = Join-Path $HOME 'Downloads\Get-Latest-SpatialHUD.ps1'
Invoke-WebRequest 'https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/c83497e6-tests/spatial-hud-template-26.3/Get-Latest-SpatialHUD.ps1' -OutFile $script
& $script -OpenFolder
```

The execution-policy setting is process-only: it closes with that PowerShell
window and does not change the computer's permanent policy.

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
its normal height at **72° down** by default. This deliberately requires a
steeper downward view before the plane looks face-on, closer to a physical
floating plane. Looking toward the panel therefore feels more like looking
perpendicular to a floating horizontal plane,
without restoring the rejected world/UI capture renderer.

This is intentionally an affine GUI effect, not a world-rendered object:
minimap, FPS/debug text, screens, and unrelated overlays remain outside it.
Change **Face-On Look-Down Pitch** to require a steeper or shallower view, or
turn the tilt off if you prefer the previous front-facing panel.

**Taper Backing Plate Width** is also enabled by default. It draws the panel
backing as a near/far trapezoid: its upper (far) edge begins at **42%** of the
near edge near the horizon and widens smoothly to a rectangle at the face-on
pitch.

**Perspective-Scale HUD Icons** is enabled by default as well. It uses the
flat-plane perspective relation `scale = 1 / (1 + depth × k)` for each vanilla
bottom-HUD root: nearer hotbar content stays wider, while health, hunger,
armor, XP, and held-item roots narrow according to their depth in the strip.
That gives icon groups real near/far width change instead of leaving them all
at one width. A single icon cannot be trapezoid-warped by Fabric's public
affine HUD pose, so exact per-pixel curvature/projective texture warping needs
an explicitly opt-in capture path rather than the default performance path.

### Experimental captured-texture warp — disabled by default

**Experimental Bottom-HUD Capture → Enable Experimental Captured-Texture Warp**
is a separate, off-by-default prototype for real finished-texture deformation.
It does not capture Minecraft's GUI wholesale. The existing Fabric wrappers
send only the selected gameplay bottom-HUD roots—hotbar/spectator controls,
status bars, XP, held-item label, and mount bar—to one private render state.
That private state is rendered into a private texture and composited as a
tessellated trapezoidal mesh, so completed slot icons, bars, and text undergo
actual texture warping instead of the default per-root affine approximation.

The experimental path deliberately has stricter safeguards than the default:

- it runs only with an active player and world **and while no screen is open**,
  even if the normal Gameplay Only preference was changed;
- the renderer-target mixin uses a strict private-renderer identity check; it
  never redirects Minecraft's normal renderer, any Screen, chat, minimap,
  debug/FPS text, or a separately registered overlay;
- status roots remain native when the existing AppleSkin/Detail Armor safety
  setting is active, because that companion combination is not yet certified
  for the experimental texture path;
- any setup, extraction, texture-target, or GPU draw error disables the
  experimental switch for the session and returns to the released affine mode.

The optional **Experimental Capture Curvature** control bends the tessellated
texture surface in screen space. It is zero/flat by default. This is an honest
2.5D GUI composite, not a world-space HUD object. A true 3D/world-rendered
"quality" mode is intentionally unavailable until this captured path has a
reproducible compatibility matrix, including AppleSkin and Detail Armor Bar
Reconstructed.

Companion status bars protected by the AppleSkin/Detail Armor compatibility
setting stay native while revealed so their own overlays remain coherent. Turn
that protection off only if you choose visual perspective over the verified
AppleSkin/Detail Armor layout safeguard.

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

The released **default performance mode** uses Fabric's public HUD element API
only: it has no renderer mixins, framebuffer redirects, screen hooks, or
world-render passes. The disabled experimental capture switch adds two narrowly
scoped renderer hooks solely for its private bottom-HUD renderer; normal GUI
renderers never meet their identity check. With **Gameplay Only** enabled (the
default), the safe renderer delegates directly to vanilla whenever another
screen is open, and the experimental renderer enforces that same screen-open
bypass unconditionally. That includes:

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
