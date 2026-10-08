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

## Render methods and placement

Spatial HUD treats the selected bottom strip as one panel at a player-relative
location. Bind the three direct method-selection actions in **Minecraft
Controls** to choose its renderer instantly:

1. **Balanced Captured Warp (green marker)** — captures the selected gameplay
   lower HUD into a private texture and applies a balanced forced projective
   mesh. It is no longer a flat root-by-root fallback: hearts, slots, icons,
   bars, and glyphs all receive a real pixel warp.
2. **Strong Projective Warp (blue marker; default)** — captures the same lower
   HUD, then applies an intentionally stronger forced projection through a
   dense **32 × 24** GUI-space mesh. Its exaggeration makes the perspective
   unmistakable even at camera angles where natural taper would be subtle.
3. **World-Space Texture (red marker)** — captures the same
   selected texture but draws it on a real quad in the rendered level. It
   supports a separate **Camera Yaw** or **Player Body** horizontal anchor and
   an **Occlude Behind World** switch. It intentionally displays the completed
   previous-frame texture, avoiding global GUI redirection while the current
   GUI is captured.

Every method draws a **full-width, eight-pixel colour band** inside the panel's
source rectangle: green for Method 1, blue for Method 2, and red for Method 3.
The band is part of the captured texture in Methods 2 and 3, so it follows the
same mesh or world plane rather than becoming a separate screen overlay.

The **Panel Positioning & Orientation** controls are shared by all three
methods and model a real waist-height **flat Minecraft-map surface** rather
than a card that follows camera pitch:

- **Forward Distance**, **Horizontal Offset**, and **Vertical Offset** place
  the panel around the player. The default vertical offset is waist-high.
- There is **no look-down-angle gate**. The selected roots and texture capture
  remain active while any part of the finite, positive-depth panel intersects
  the screen, and are culled only once the complete surface leaves the field of
  view or passes behind the camera.
- **Panel Width** controls physical size.
- **Primary Flat-Map Tilt — Left ↔ Right Axis** defaults to **85°**, producing
  a near-horizontal surface that reads like a Minecraft map viewed from above.
  Lower it when a deliberately steep paper-like angle is preferred.
- **Fine Tilt Offset** uses that same left-to-right axis; **Turn — Up Axis**
  angles the panel left/right; **Roll — Panel-Normal Axis** raises the right
  edge for a deliberate slant. Their in-game tooltips define each positive
  direction exactly.

Methods 1 and 2 both apply genuine per-pixel projective perspective through
captured meshes: Method 1 is balanced; Method 2 is deliberately stronger and
the default. Both explicitly pinch the far/top row horizontally and widen the
near/bottom row, so their top two corners move toward each other as a proper
map trapezoid rather than merely stretching in independent X/Y directions.
Method 3 is a true world quad and samples the same lower-HUD source rectangle
rather than a full-window texture; Methods 1 and 2 are camera-yaw anchored,
while Method 3 exposes its anchor as a separate setting so it can instead
remain fixed to player-body yaw.

### Companion-mod baseline

AppleSkin and Detail Armor Bar Reconstructed are captured before the optional
native-layout fallback. Their injected pixels therefore stay with the vanilla
hearts, hunger, armor, and air roots in every captured method. The green
balanced mesh also transforms those status roots by default. Enable **Keep
Companion Bars Native in Method 1** only when a companion mod visibly produces
detached or duplicated decorations. The supplied F5W modpack also includes Bedrock Hotbar,
Immersive Hotbar, DualBar, Armor Indicator, Status Effect Bars, Mount Opacity,
Durability Warner HUD, Async Hotbars, and Spatial GUI; these are tracked as
bottom-HUD compatibility candidates rather than being globally intercepted.

See [`COMPATIBILITY.md`](COMPATIBILITY.md) for the test order and the exact
compatibility boundary. No normal Screen, minimap, chat, debug/FPS text, or
separately registered overlay enters the capture target.

### Compatibility boundary

**Balanced Captured Warp**, **Strong Projective Warp**, and **World-Space
Texture** use two narrowly scoped renderer hooks solely for their private
bottom-HUD renderer; normal GUI renderers never meet their identity check.
World-Space Texture additionally submits only that finished private texture to
the level render pass. With **Gameplay Only** enabled (the default), all three
methods bypass private capture whenever another screen is open. That includes:

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
configuration file is `config/spatialhud.json`. The screen has one read-only
explanation tab plus three focused settings sections:

- **Method Guide — Read This First** is a plain-language, read-only comparison
  of the green stable path, blue true projective mesh, and red real world panel.
  It contains no settings and is never saved into the configuration file.
- **Setup** combines the enable switch, panel visibility, gameplay boundary,
  and companion-bar safeguard. Method selection is intentionally not a config
  field; use the three direct keybindings listed below.
- **Panel Positioning & Orientation** keeps every geometric control together:
  distance, size, offsets, three-axis rotation, the flat-map tilt, and the
  Method 3 anchor and terrain-occlusion choices. The Method 3-only tooltips
  explicitly say when a setting is ignored by the other two methods.
- **HUD Contents** chooses which lower-HUD roots Spatial HUD owns.

Minecraft Controls contains:

- **Open Spatial HUD Settings** — defaults to **H** and can be rebound.
- **Toggle Spatial HUD** — unbound by default, so you can assign a rapid
  test key without replacing a modpack binding.
- **Spatial HUD — Select Method 1 / 2 / 3** — default to **F6 / F7 / F8**,
  avoiding the hotbar number keys. Rebind any conflict in Controls. Each press
  shows the chosen green balanced warp, blue strong warp, or red world-map mode
  in the action bar.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config.
Mod Menu is optional but recommended for the Configure button.
