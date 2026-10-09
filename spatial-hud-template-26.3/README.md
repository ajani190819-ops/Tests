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

The menu remembers its folder, chosen build, and optional folder opener under
`%LOCALAPPDATA%\SpatialHudUpdater`, and lets you:

- **choose the build** — listed as **main first, then the five most recently
  built branches**, with **[A]** for every branch that has a published build,
  **[T]** to type one branch name, and **[R]** for the newest build on any
  branch;
- change the profile's `mods` folder at any time;
- leave the post-install folder opener disabled (the default), or ask it to
  auto-detect **OneCommander** / save a custom file-manager `.exe`;
- inspect the chosen build's release, JAR name, build time, and size; and
- restore the default F5W folder, newest-build choice, and no-opener setting.

Pick the build whose behavior you want to test, then install it:

```text
 [1] Install or update Spatial HUD now
 [2] Choose the build - main or one of the newest branches
 [3] Change the Modrinth mods folder
 [4] Configure optional folder opener (OneCommander / none)
 [5] Check the chosen build details
 [6] Advanced: use a different release feed
 [7] Restore the default folder, build, and no-opener setting
 [Q] Quit
```

**One-time note (2026-10-08):** if your saved `Update-SpatialHUD.bat` is older
than this build picker, replace that file once with the current copy from
<https://raw.githubusercontent.com/ajani190819-ops/Tests/main/spatial-hud-template-26.3/Update-SpatialHUD.bat>
(or from the `arena/...` branch you are testing). Older copies still install the
newest build correctly, but they show the older menu — the quick check is the
**`Build:`** row in the menu header, which only the current updater shows. After this one
replacement, the file keeps working on its own: it reads the newer of its two
helper copies by a version marker, so it survives a merge without any further
download.

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

The build selector is intentionally based on **published builds**, not a fake
source-branch download: a Minecraft JAR is a compiled artifact, while a Git
branch only contains source. The default `spatial-hud-latest` feed always holds
the newest successful build on any branch; each branch that GitHub builds also
gets its own release named `spatial-hud-build-<branch>` (for example
`spatial-hud-build-arena-b4016c28-tests`), which is what the picker lists. A
branch therefore only appears once its own build has succeeded — a branch with
no published build is never offered as an installable JAR. `main` appears after
main itself has been built.

The batch file temporarily refreshes its maintained PowerShell helper from
GitHub before each run, then deletes that temporary helper. Its
ExecutionPolicy bypass applies only to that updater process and does not change
the computer's permanent policy.

This is the only updater you need. Option **[D]** in its menu saves the chosen
build to your Downloads folder instead of the mods folder, for when you want to
add it yourself through Modrinth's "From file". (The old
[`Get-Latest-SpatialHUD.bat`](Get-Latest-SpatialHUD.bat) now just points here.)

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
4. **Purple 2.5D Panel (Method 4)** — the same captured HUD warped onto one
   purple four-corner surface that is drawn in the normal GUI layer. It only
   looks like a 3D sheet: its corners move with your look pitch, but it is not
   an object in the world. Each corner is set in the **Purple Polygon Test**
   tab. Its border and four corner handles are part of the captured texture.

**Failure indicator.** If the capture fails, the untouched vanilla HUD is shown
in place of the panel, and a small red square appears just outside the hotbar's
top-left corner. The selected method is kept, so restarting the game or picking
the method again retries it.

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

**Real 3D Panel** and the **Purple 2.5D Panel** both use the same two narrowly
scoped renderer hooks solely for their private bottom-HUD renderer; normal GUI
renderers never meet their identity check. The Real 3D Panel additionally
submits only that finished private texture to the level render pass. With
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
  of the real 3D panel (Method 3) and the purple 2.5D panel (Method 4). It contains no settings and is never saved into
  the configuration file.
- **Setup** combines the enable switch, panel visibility, gameplay boundary,
  and the **Render Mode Slider** (3 or 4).
- **Panel Positioning & Orientation** keeps Method 3's geometric controls together:
  distance, size, offsets, three-axis rotation, the flat-map tilt, and the
  Method 3 anchor and terrain-occlusion choices. The Method 3-only tooltips
  explicitly say when a setting is ignored by the other two methods.
- **Purple Polygon Test** provides eight percentage sliders—X and Y for each
  top-left, top-right, bottom-right, and bottom-left handle. They affect only
  Method 4, and they set the four corners that the captured lower HUD is warped
  onto. **Follow Camera Pitch** makes the surface behave like a physical sheet
  held in front of you, as described above; turn it off for a perfectly fixed
  target. Head pitch is limited to 70°; beyond that the sheet stops following. **Show Backing Panel** controls
  Method 4's translucent purple interior; the purple border and corner handles
  always remain. Anything another mod draws higher than the bottom of the strip
  stays outside the quad.
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
