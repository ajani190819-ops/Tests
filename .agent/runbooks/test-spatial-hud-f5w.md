# Test Spatial HUD in F5W

- Status: current procedure, rewritten 2026-10-08 for the two shipped methods
  (3 and 4). Methods 1 and 2 are archived. The Method 4 horizontal-panel
  change is agreed but **not yet implemented**; the Method 4 checks below
  describe the current code.
- Read when: requesting or evaluating an F5W runtime test.

## Before starting

1. Close Minecraft.
2. Install the newest build for the branch under test with
   `Update-SpatialHUD.bat` (branch picker, then install). Remove or replace older
   copies so Fabric cannot load the wrong artifact.
3. Confirm the new `latest.log` contains neither `spatialhud:world_texture_through_world`
   nor `Sampler1`. No custom shader pipeline is registered by the mod.
4. Keep the normal F5W renderer and HUD-mod stack enabled. Do not disable Iris
   or companion HUD mods unless the test is explicitly an isolation test.

## Test setup

1. Press **H**. It opens Spatial HUD settings and must not change the HUD's
   enabled state.
2. Check **Method Guide — Read This First**: read-only cards for Methods 3 and
   4, no editable field, nothing saved from it.
3. Check the sections: **Setup**, **Panel Positioning & Orientation** (Method 3),
   **Purple Polygon Test** (Method 4), **HUD Contents**.
4. Use **F8** for Method 3 and **F9** for Method 4, or the **Render Mode
   Slider** (3 or 4).

## Required observations

Record each result separately by mode.

| Mode | Check | Pass condition |
|---|---|---|
| all | Scope | Menus, chat, inventory, minimap, debug text, and unrelated mod GUIs are untouched. Only the selected lower HUD is captured. |
| all | Capture failure | The vanilla HUD stays visible and a red square appears just above the hotbar's top-left corner. Nothing else is drawn in its place. |
| 3 | World quad | The plane stays in front while looking left/right (Camera Yaw) or stays at body heading (Player Body); **Occlude Behind World** on/off hides/shows it behind a block. One completed-frame delay is expected; persistent loss or flicker is not. |
| 3 | Border | The purple border is 3 px wide and surrounds the captured HUD. |
| 4 | Content | The captured hotbar, hearts/hunger/armor/air, XP bar and level, and held-item text are all warped into the purple quad. Nothing else from the screen appears inside it. |
| 4 | Single surface | Nothing is drawn at the normal vanilla HUD position — no second dark rectangle anywhere on screen while the mode is enabled. |
| 4 | Border and handles | The purple border and the four corner handles sit exactly on the four configured corners and deform with them. |
| 4 | Pitch response | Stays put as you turn. Looking up or down changes its perspective. Crouching moves it, because it is ground-anchored. **Follow Camera Pitch = off** freezes the quad on screen. |
| 4 | Corners | Move each of the eight percentages and confirm exactly that corner moves. |
| 4 | Companion pixels | AppleSkin and Detail Armor pixels appear inside the quad with their vanilla root. |
| 4 | Backing switch | **Show Backing Panel** off leaves the purple border and handles with a see-through interior; the HUD pixels stay readable with it on. |

## Axis to note, not a pass/fail

- The Method 4 quad is composited over the GUI inside its four corners, so it
  can draw over whatever else is there (for example the crosshair region).
  Record this if it bothers you.
- A modded bar drawn higher than 72 GUI pixels above the bottom of the screen
  stays outside the quad. Record which bars are missing.

## Report format

Give the installed jar's timestamp or size, the branch and build run, the config
values, which HUD mods were visible, a screenshot or short clip if possible, and
the `latest.log` from the same run. State whether Iris was enabled, and whether
this was the rolling build from `spatial-hud-latest` or a branch build.
