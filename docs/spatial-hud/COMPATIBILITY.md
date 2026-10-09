# Spatial HUD — F5W compatibility matrix

This is the initial compatibility baseline supplied by the F5W Modrinth
profile. It is intentionally based on project IDs/names rather than guessed
JAR filenames or versions. A mod being listed below does **not** mean it has
been certified yet.

## Rendering contract

Spatial HUD has two presentations (Method 3 and Method 4). Both use the same
private capture, and neither redirects Minecraft's normal GUI renderer:

1. **Method 3 — real 3D panel:** only the selected vanilla bottom-HUD roots,
   their injected pixels, and the purple panel border are extracted into a
   private `GuiRenderState`, rendered to a private target, and drawn on a flat
   client-side world panel.
2. **Method 4 — purple 2.5D panel:** the same private capture, warped onto four
   GUI-space corners.

No `Screen`, chat, minimap, debug text, boss bar, crosshair, or separately
registered overlay is captured. If the capture fails, the vanilla HUD is shown
and a small red square appears beside the hotbar; the selected method is kept.

## Test order

| Tier | Components | Required result before advancing |
| --- | --- | --- |
| 0 | Vanilla Fabric 26.3, Fabric API, Cloth Config, Mod Menu | Every selected root is a visibly deformed portion of the same trapezoid; no screen-open capture. |
| 1 | AppleSkin + Detail Armor Bar Reconstructed | Injected saturation/food and custom armor pixels remain attached to their vanilla bars inside the captured mesh. |
| 2 | Bedrock Hotbar, Immersive Hotbar, DualBar, Armor Indicator, Status Effect Bars, Mount Opacity, Durability Warner HUD, Async Hotbars | No duplicate selected root, clipping, detached decoration, or lost tooltip; document an owner/visibility switch when two mods intentionally own the same root. |
| 3 | Spatial GUI, Inventory Profiles Next, Jade, OptiGUI, REI/JEI-style screens, Xaero's Minimap/World Map | Opening any normal/modded screen and all unrelated overlays stay outside the target. |
| 4 | Sodium, Iris, Nvidium, ImmediatelyFast, BadOptimizations, ModernFix, Entity Culling, More Culling | Capture target and mesh remain stable under the profile's renderer/performance stack. |

Method 3 (the real 3D panel) is the world-rendered mode. Its tier results are
recorded per exact JAR version.

## F5W profile candidates

### Direct bottom-HUD ownership or decoration

- AppleSkin
- Detail Armor Bar Reconstructed
- Bedrock Hotbar
- Immersive Hotbar
- DualBar
- Armor Indicator
- Status Effect Bars
- Mount Opacity
- Durability Warner HUD
- Async Hotbars / Hotbar Lag Fixer
- Durability Tooltip
- Dynamic Crosshair
- XPlus Autofish / Pick Up Notifier where an item notification overlaps the
  selected bottom envelope

These are the highest-risk group because they may inject into a vanilla root,
replace a root, or independently register a nearby overlay. Experimental mode
captures injected root pixels together; it must not steal independently
registered HUD layers.

### GUI/screen isolation candidates

- Spatial GUI
- Inventory Profiles Next and libIPN
- Jade
- OptiGUI
- Shulker Box Tooltip
- Better Advancements / Advancement Plaques / Plane Advancements
- Mod Menu, Cloth Config API, Configured Defaults, YACL
- Xaero's Minimap and Xaero's World Map

These validate the hard rule that no normal `Screen` or unrelated overlay is
routed to the Spatial HUD target.

### Render-stack candidates

- Sodium, Sodium Extra, Iris, Nvidium, ImmediatelyFast
- BadOptimizations, ModernFix, FerriteCore, Lithium, Krypton
- Entity Culling, More Culling, Cull Display Entities, Better Block Entities
- Continuity, Euphoria Patches, EMF/ETF/ESF, Animatica Refabricated

The capture implementation must be checked with this group enabled because it
exercises non-vanilla render scheduling, texture state, and performance paths.

## Evidence to collect per tier

For each test case, record the exact mod versions and capture:

1. level view with Method 4 selected;
2. the same view with Method 3 selected;
3. a close-up of hotbar, health/food/armor, XP, and held-item label;
4. one screen-open view proving regular GUI is untouched; and
5. the Minecraft log (`latest.log`) if the red failure indicator appears.

The supplied project list is sufficient to start the audit. Exact versions can
be added later from Modrinth when a candidate reaches certification.
