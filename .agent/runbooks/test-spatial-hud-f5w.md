# Test Spatial HUD in F5W

- Status: current test procedure.
- Last verified: 2026-10-08 CI build `37713731571` (`b1c9600`); all F5W method results remain pending.
- Read when: requesting or evaluating an F5W runtime test.

## Before starting

1. Close Minecraft.
2. Install the current rolling Spatial HUD JAR in the F5W profile. Remove or
   replace older copies so Fabric does not load the wrong artifact. Confirm the
   new `latest.log` contains `Spatial HUD render-method build initialized` and
   contains neither `spatialhud:world_texture_through_world` nor `Sampler1`. The
   current Method 3 uses vanilla `ENTITY_TRANSLUCENT` or `GUI_TEXTURED`, not a
   custom shader pipeline.
3. Keep the normal F5W renderer and HUD-mod stack enabled. Do not disable Iris
   or companion HUD mods unless the test is explicitly an isolation test.

## Test setup

1. Press H and verify that it opens Spatial HUD settings without changing HUD
   enabled state. Confirm there are exactly three focused sections—**Setup &
   Render Method**, **Panel Positioning & Orientation**, and **HUD Contents**—
   and that all placement controls are together in the middle section. Confirm
   all three named methods have readable contextual descriptions.
2. Start with `Face-On Look-Down Angle` at 30 and `Pitch Offset` at 0.
3. Test with native status-bar preservation both on and off if companion bars
   are involved. This setting changes only Method 1 behavior.

## Required observations

Record each result separately by Render Method:

| Method | Check | Pass condition |
|---|---|---|
| 1 — Classic Affine | Baseline and marker | Selected HUD is stable; it may not form a true icon-level trapezoid. The compact upper-left panel marker is green. |
| 2 — Captured Projective Mesh | Capture stability and marker | No flicker, disappearing plate, or frame-to-frame loss. The compact upper-left panel marker is blue and follows the captured mesh. |
| 2 — Captured Projective Mesh | Geometry | Backing, hotbar, icons, bars, text, and blue marker use one real trapezoid: rectangular at 30° down, with a visibly narrower far edge at a grazing view. |
| 2 or 3 | Scope | Menus and unrelated GUI elements remain normal. |
| 2 or 3 | Companion pixels | AppleSkin and Detail Armor pixels are captured with their selected status root. |
| 3 — World-Space Texture | Camera Yaw anchor and marker | Plane stays in front while looking left/right without player-body lag. The compact upper-left panel marker is red and is part of the world texture. |
| 3 — World-Space Texture | Player Body anchor | Plane stays at the body heading and may move to the side when looking away. |
| 3 — World-Space Texture | Occlude Behind World on | A block between camera and panel hides the panel like a physical object. |
| 3 — World-Space Texture | Occlude Behind World off | Panel remains visible through the same block. |
| 3 — World-Space Texture | Latency/stability | One completed-frame delay is expected; persistent loss, flicker, or stale texture is not. |

## Report format

Provide the installed JAR timestamp or commit if known, the config values,
which HUD mods were visible, a screenshot or short clip if possible, and
`latest.log` from the same run. State whether the test was performed with Iris
enabled.
