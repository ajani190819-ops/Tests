# Spatial HUD (canonical build project)

**Spatial GUI-inspired HUD behavior without touching the rest of the GUI.**
Spatial HUD controls only the vanilla bottom strip: hotbar, hearts, hunger,
armor, air, XP, mount bars, and held-item name. It is client-side only;
servers do not need it. Press **H** to open its settings.

GitHub Actions builds every change and replaces the jar at this fixed link:

**Download (always the newest successful build):**
https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

### Windows: one-click install into Modrinth

Save [`Update-SpatialHUD.bat`](Update-SpatialHUD.bat) once, then double-click it
whenever you want the newest build. It is the Spatial HUD **installer**: one
file, the same shape as `Orca-Plugins.bat`. It installs into your F5W Modrinth
profile:

```text
%APPDATA%\ModrinthApp\profiles\F5W\mods
```

Press **Enter** to install. The menu:

```text
 [1] Install or update Spatial HUD now
 [2] Choose the build - main or one of the five newest branches
 [3] Change the Modrinth mods folder
 [4] Forget my choices - newest build and the default folder
 [Q] Quit
```

Each install runs in three steps:

1. **Download and check.** The jar is downloaded from GitHub. Its size must match
   what GitHub reported, and it must be the Spatial HUD mod (its
   `fabric.mod.json` id is `spatialhud`). Nothing on your computer changes until
   this passes.
2. **Swap the jar.** Your old `spatial-hud-*.jar` moves to
   `%LOCALAPPDATA%\SpatialHudUpdater\backup`, and the new jar is copied in. If
   the copy fails, the old jar is put back. Spatial GUI and other mods are not
   touched.
3. **Save your choices.** The build and folder are remembered only after steps 1
   and 2 succeed, so a branch name that does not work is never remembered.

Close Minecraft before installing, so Windows cannot hold the old jar open.

**Choosing a build (option 2).** The list is built from GitHub's published
builds: `main` first, then the five most recently built branches. Each row has a
number; type the number shown. **[L]** installs the newest build from any
branch, and **[T]** lets you type a branch name. A branch appears only after its
own build has succeeded.

**Updating the installer itself.** On each run it looks for a newer copy of
itself on the session branch. If it finds one, that copy runs the update, and
your saved file keeps working on its own. `--no-self-update` turns this off.

**Where to get it:**
<https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/b4016c28-tests/spatial-hud-template-26.3/Update-SpatialHUD.bat>
Save it anywhere convenient (for example your Desktop) and double-click it.
Until this branch is merged, use that link; the copy on `main` is stale.

**Old copies.** If your saved copy shows **[4] Configure optional folder opener**
or **[7] Restore**, it is an older installer. Download it again from the link
above. Windows may show an "Unknown Publisher" prompt the first time: that is
expected for a downloaded `.bat`.

Builds come from GitHub releases. The rolling `spatial-hud-latest` release
always holds the newest successful build of any branch; each branch gets its own
`spatial-hud-build-<branch>` release (for example
`spatial-hud-build-arena-b4016c28-tests`), which is what option 2 lists. The
repository must stay public, because the installer downloads without a login.

## Render methods and placement

Spatial HUD has two presentations. Pick one with the **Render Mode Slider** in
the Setup config tab, or with the direct keys **F8** (Method 3) and **F9**
(Method 4):

3. **Real 3D Panel (Method 3)** — the selected lower HUD is captured into a
   private texture and drawn on a real, flat, client-side panel in the game
   world. The panel is anchored to your position and to your camera's yaw. It
   floats at the **Forward Distance** and **Vertical Offset** you set, and it
   comes into view as you look down. Its **Primary Flat-Map Tilt** (rotation
   around the panel's left-to-right axis) sets the look-down angle at which you
   face it square-on. The default is 85°, so the panel is close to flat.
   Its purple border frames the captured HUD, and it is lifted onto the world
   surface with that HUD.
4. **Purple Horizontal Panel (Method 4)** — the same captured HUD drawn on a
   purple panel that lies flat in the game world, like a sheet on a table in
   front of you. It is anchored to your feet and to your body heading, so it
   stays put when you turn your head, and you see it when you look down. Set its
   **Panel Distance**, **Panel Height Above Feet**, and **Panel Angle** in the
   **Purple Horizontal Panel** tab. An angle of 90 lies it flat, so it is
   face-on when you look straight down. Smaller angles tilt its near edge toward
   you. Its purple border is part of the captured texture.

**Failure indicator.** If the capture fails, the untouched vanilla HUD is shown
in place of the panel, and a small square appears just outside the hotbar's
top-left corner: **red** if Method 3 failed, **purple** if Method 4 failed. The
selected method is kept, so restarting the game or picking the method again
retries it.

The **Panel Positioning & Orientation** controls belong to Method 3. They model
a waist-height flat surface, not a card that follows camera pitch:

- **Forward Distance**, **Horizontal Offset**, and **Vertical Offset** place
  the panel around the player.
- There is **no look-down-angle gate**. The panel stays live while any part of
  it is in the field of view. It is culled once the whole surface leaves the
  view or passes behind the camera.
- **Panel Width** controls physical size.
- **Primary Flat-Map Tilt — Left ↔ Right Axis** defaults to **85°**. Lower it
  to make the panel steeper, so you face it square-on at a shallower look-down
  angle.
- **Fine Tilt Offset** uses that same axis; **Turn — Up Axis** angles the panel
  left or right; **Roll — Panel-Normal Axis** raises the right edge. Their
  in-game tooltips define each positive direction.

Method 3 samples only the selected lower-HUD source rectangle, not a
full-window texture. Its anchor (camera yaw or player body) and its terrain
occlusion are set in the same tab, and their tooltips say they apply only to
Method 3.

### Companion-mod baseline

AppleSkin and Detail Armor Bar Reconstructed are captured with the vanilla
roots they decorate, so their injected pixels stay with the hearts, hunger,
armor, and air they belong to. The supplied F5W modpack also includes Bedrock
Hotbar, Immersive Hotbar, DualBar, Armor Indicator, Status Effect Bars, Mount
Opacity, Durability Warner HUD, Async Hotbars, and Spatial GUI. These are
tracked as bottom-HUD compatibility candidates rather than being globally
intercepted.

See [`COMPATIBILITY.md`](../docs/spatial-hud/COMPATIBILITY.md) for the test order and the exact
compatibility boundary. No normal Screen, minimap, chat, debug/FPS text, or
separately registered overlay enters the capture target.

### Compatibility boundary

**Real 3D Panel** and the **Purple Horizontal Panel** both use the same two
narrowly scoped renderer hooks solely for their private bottom-HUD renderer;
normal GUI renderers never meet their identity check. Both panels submit only
that finished private texture to the level render pass. With
**Gameplay Only** enabled (the default), both methods bypass private capture
whenever another screen is open. That includes:

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
explanation tab plus four focused settings sections:

- **Method Guide — Read This First** is a plain-language, read-only comparison
  of the real 3D panel (Method 3) and the purple horizontal panel (Method 4). It
  contains no settings and is never saved into the configuration file.
- **Setup** combines the enable switch, panel visibility, gameplay boundary,
  and the **Render Mode Slider** (3 or 4).
- **Panel Positioning & Orientation** keeps Method 3's geometric controls together:
  distance, size, offsets, three-axis rotation, the flat-map tilt, and the
  Method 3 anchor and terrain-occlusion choices. The Method 3-only tooltips
  explicitly say when a setting is ignored by the other two methods.
- **Purple Horizontal Panel** holds the three Method 4 placement controls:
  **Panel Distance**, **Panel Height Above Feet**, and **Panel Angle**. They
  affect only Method 4. **Show Backing Panel** is Method 3's translucent purple
  interior; Method 4 has no backing, and its fill is **Show Purple Fill**.
  Method 4 samples the full captured strip, the same source as Method 3, so
  anything another mod draws in that strip is included.
- **HUD Contents** chooses which lower-HUD roots Spatial HUD owns.

Minecraft Controls contains:

- **Open Spatial HUD Settings** — defaults to **H** and can be rebound.
- **Toggle Spatial HUD** — unbound by default, so you can assign a rapid
  test key without replacing a modpack binding.
- **Spatial HUD — Select Method 3 / 4** — default to **F8 / F9**, avoiding the
  hotbar number keys. Rebind any conflict in Controls.

## Requirements

Minecraft **26.3**, Fabric Loader **0.19.5+**, Fabric API, and Cloth Config.
Mod Menu is optional but recommended for the Configure button.
