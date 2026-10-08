# Spatial HUD (canonical build project)

**Spatial GUI-inspired HUD behavior without touching the rest of the GUI.**
Spatial HUD controls only the vanilla bottom strip: hotbar, hearts, hunger,
armor, air, XP, mount bars, and held-item name. It is client-side only;
servers do not need it. Press **H** to open its settings.

GitHub Actions builds every change and replaces the jar at this fixed link:

**Download (always the newest successful build):**
https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

### Windows: one-click Modrinth install / update

For the simplest route, save [`Update-SpatialHUD.bat`](Update-SpatialHUD.bat)
to your computer once, then double-click it whenever you want an update. On
its first run, it already targets the supplied F5W Modrinth profile:

```text
%APPDATA%\ModrinthApp\profiles\F5W\mods
```

For `C:\Users\kamau`, that resolves to the requested
`C:\Users\kamau\AppData\Roaming\ModrinthApp\profiles\F5W\mods`. Press
**Enter** (or choose **1**) to install the newest successful Spatial HUD build
directly there. It does **not** open any folder afterward by default, and there
is no manual "From file" step in Modrinth.

The menu remembers its folder, release feed, and optional folder opener under
`%LOCALAPPDATA%\SpatialHudUpdater`, and lets you:

- change the profile's `mods` folder at any time;
- choose a different published GitHub build/release feed after validating it;
- leave the post-install folder opener disabled (the default), or ask it to
  auto-detect **OneCommander** / save a custom file-manager `.exe`;
- inspect the current JAR name, build time, and size; and
- restore the default F5W folder, `spatial-hud-latest` feed, and no-opener
  setting.

When a OneCommander path is configured, the updater opens the profile's `mods`
folder in OneCommander after a successful install. It never invokes Windows
Explorer. If OneCommander is installed in an unusual portable or Microsoft
Store location, choose **[4]** and paste the full path to `OneCommander.exe`.

It downloads and verifies the replacement before touching the profile, then
identifies old copies through their own `fabric.mod.json` mod id (`spatialhud`),
not just their filenames. It replaces every old Spatial HUD copy it finds and
leaves Spatial GUI and every unrelated mod alone. If the download, validation,
or move fails, it restores any copies already moved out of the way.

Close Minecraft before updating so Windows cannot hold the old JAR open.

The build selector is intentionally based on **published build/release feeds**,
not a fake source-branch download: Minecraft JARs are compiled artifacts, while
a Git branch only contains source. The default `spatial-hud-latest` feed is
updated by the successful GitHub Actions build from this branch. When a
branch-specific build is published as its own release feed, choose it through
**[3]** (or enter that published tag); a raw branch is never mislabeled as an
installable JAR.

The batch file temporarily refreshes its maintained PowerShell helper from
GitHub before each run, then deletes that temporary helper. Its
ExecutionPolicy bypass applies only to that updater process and does not change
the computer's permanent policy. The old
[`Get-Latest-SpatialHUD.bat`](Get-Latest-SpatialHUD.bat) remains available when
you specifically want a Downloads copy instead of a direct Modrinth install.

## v1.3 — camera-yaw projective plane

Spatial HUD treats the bottom strip as one configurable physical plane at a
player-relative location rather than a look-down-only screen card. The old
reveal settings remain only for JSON compatibility and are no longer shown in
Mod Menu.

### Virtual placement

The plane is always **camera-yaw anchored**: looking left or right keeps it in
front of the camera, without turning it when the player's body faces a
separate direction. Its fixed physical orientation is face-on at the configured
look-down angle and creates ordinary perspective at every other view angle.

The **Virtual HUD Plane** category provides these controls:

- **X:** player-relative horizontal offset, positive right
- **Y:** player-relative vertical offset from eye height, positive up
- **Z:** player-forward distance in blocks
- **Face-On Look-Down Pitch:** the view angle at which the plane is a
  rectangle; the default is 30° below the horizon
- **pitch offset and yaw:** additional orientation tuning
- **scale:** the existing Panel Width control

When the camera looks higher than the configured face-on angle, the plane's
upper/far edge recedes and becomes horizontally narrower. The captured mesh
therefore becomes a real trapezoid. Looking lower changes the perspective in
the opposite direction. The backing and every captured HUD pixel use this same
plane.

### Captured projective mesh — disabled by default

**Experimental Bottom-HUD Capture → Enable Experimental Captured-Texture Warp**
is the true deformation path. It captures only the selected gameplay bottom
HUD—backing plate, hotbar, status roots, XP, mount bar, and held-item label—into
one private texture. A 24 × 12 mesh maps that finished texture through the
same virtual-plane projection for every vertex. That means a heart, hunger
icon, hotbar slot, XP glyph, or tooltip letter itself becomes trapezoidal as
its depth changes; it is not merely moved or root-scaled.

The captured backing is intentionally part of that same texture. Its corners,
curvature, and every icon pixel therefore share one projection, removing the
old visual mismatch where a correctly tapered plate held rectangular icon groups.
The experiment remains opt-in while the modpack compatibility matrix is built;
the safe affine tangent renderer is still the default fallback.

### Companion-mod baseline

AppleSkin and Detail Armor Bar Reconstructed are captured before the legacy
native-layout safeguard. Their injected pixels are therefore warped together
with the vanilla hearts, hunger, armor, and air roots in experimental mode.
The safe renderer retains its native companion fallback. The supplied F5W
modpack also includes Bedrock Hotbar, Immersive Hotbar, DualBar, Armor
Indicator, Status Effect Bars, Mount Opacity, Durability Warner HUD, Async
Hotbars, and Spatial GUI; these are tracked as bottom-HUD compatibility
candidates rather than being globally intercepted.

See [`COMPATIBILITY.md`](COMPATIBILITY.md) for the test order and the exact
compatibility boundary. No normal Screen, minimap, chat, debug/FPS text, or
separately registered overlay enters the capture target.

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

Open **Mod Menu → Spatial HUD → Configure** or press **H**. The underlying
configuration file is `config/spatialhud.json`. Minecraft Controls contains:

- **Open Spatial HUD Settings** — defaults to **H** and can be rebound.
- **Toggle Spatial HUD** — unbound by default, so you can assign a rapid
  test key without replacing a modpack binding.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config.
Mod Menu is optional but recommended for the Configure button.
