# Test Spatial HUD in F5W

- Status: current procedure, rewritten 2026-10-08 to match the shipped four
  modes. No F5W result exists for the Method 4 texture change.
- Read when: requesting or evaluating an F5W runtime test.

## Before starting

1. Close Minecraft.
2. Install the newest rolling Spatial HUD jar in the F5W profile. Remove or
   replace older copies so Fabric cannot load the wrong artifact.
3. Confirm the new `latest.log` contains
   `Spatial HUD forced-warp build initialized` and contains neither
   `spatialhud:world_texture_through_world` nor `Sampler1`. No custom shader
   pipeline is registered by the mod.
4. Keep the normal F5W renderer and HUD-mod stack enabled. Do not disable Iris
   or companion HUD mods unless the test is explicitly an isolation test.

## Test setup

1. Press **H**. It opens Spatial HUD settings and must not change the HUD's
   enabled state.
2. Check **Method Guide — Read This First**: read-only cards for the green,
   blue, red, and purple modes, no editable field, nothing saved from it.
3. Check the sections: **Setup**, **Panel Positioning & Orientation**,
   **Purple Polygon Test**, **HUD Contents**.
4. Set the **Render Mode Slider** to 4 for the Method 4 checks. Its defaults
   are top-left 30/35, top-right 70/35, bottom-right 80/70, bottom-left 20/70
   (percent of GUI width/height), **Follow Camera Pitch** on,
   **Pitch Response Strength** 100.

## Required observations

Record each result separately by mode.

| Mode | Check | Pass condition |
|---|---|---|
| all | Scope | Menus, chat, inventory, minimap, debug text, and unrelated mod GUIs are untouched. Only the selected lower HUD is captured. |
| 1 | Baseline | The selected HUD is stable on the balanced mesh. The wide band is green. |
| 2 | Geometry | Backing, hotbar, icons, bars, text, and the blue band share one trapezoid; its far/top edge is visibly narrower than its near/bottom edge. |
| 3 | World quad | The plane stays in front while looking left/right (Camera Yaw) or stays at body heading (Player Body); **Occlude Behind World** on/off hides/shows it behind a block. One completed-frame delay is expected; persistent loss or flicker is not. |
| 1–3 | Capture failure | If any capture stage fails, the log names the stage and the mode keeps working in its safe presentation. |
| 4 | Content | The captured hotbar, hearts/hunger/armor/air, XP bar and level, and held-item text are all warped into the purple quad. Nothing else from the screen appears inside it. |
| 4 | Single surface | Nothing is drawn at the normal vanilla HUD position — no second dark rectangle anywhere on screen while the mode is enabled. |
| 4 | Border and handles | The purple border and the four corner handles sit exactly on the four configured corners and deform with them. |
| 4 | Pitch response | The surface behaves like a flat card in a 3D renderer: looking down tips the far/top edge away so it narrows while the near/bottom edge widens and the card foreshortens; looking up mirrors it. Warped HUD, border, and handles tilt as one surface, the card centre stays where the handles placed it, and the tilt stops at 70° without ever becoming a line. **Follow Camera Pitch = off** freezes the quad. |
| 4 | Straight lines | On the tilted surface, straight HUD lines (hotbar edges, bar outlines) stay straight instead of bowing. |
| 4 | Corners | Move each of the eight percentages and confirm exactly that corner moves. |
| 4 | Companion pixels | AppleSkin and Detail Armor pixels appear inside the quad with their vanilla root. |
| 4 | Backing switch | **Show Backing Panel** off leaves the purple border and handles with a see-through interior; the HUD pixels stay readable with it on. |

## Axis to note, not a pass/fail

- The Method 4 quad is composited over the GUI inside its four corners, so it
  can draw over whatever else is there (for example the crosshair region).
  Record this if it bothers you; it is a consequence of the warp, not a bug.
- A modded bar drawn higher than 72 GUI pixels above the bottom of the screen
  stays outside the quad. Record which bars are missing so the band height can
  be revisited.

## Report format

Give the installed jar's timestamp or size, the config values, which HUD mods
were visible, a screenshot or short clip if possible, and `latest.log` from the
same run. State whether Iris was enabled, and whether this was the rolling
build from `spatial-hud-latest`.
