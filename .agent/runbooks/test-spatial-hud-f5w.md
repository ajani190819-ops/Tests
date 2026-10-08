# Test Spatial HUD in F5W

- Status: current test procedure.
- Last verified: 2026-10-08 CI build `37708052786`; all F5W method results remain pending.
- Read when: requesting or evaluating an F5W runtime test.

## Before starting

1. Close Minecraft.
2. Install the current rolling Spatial HUD JAR in the F5W profile. Remove or
   replace older copies so Fabric does not load the wrong artifact.
3. Keep the normal F5W renderer and HUD-mod stack enabled. Do not disable Iris
   or companion HUD mods unless the test is explicitly an isolation test.

## Test setup

1. Press H and verify that it opens Spatial HUD settings without changing HUD
   enabled state. Confirm **Render Method** has all three named choices and
   readable descriptions.
2. Start with `Face-On Look-Down Angle` at 30 and `Pitch Offset` at 0.
3. Test with native status-bar preservation both on and off if companion bars
   are involved. This setting changes only Method 1 behavior.

## Required observations

Record each result separately by Render Method:

| Method | Check | Pass condition |
|---|---|---|
| 1 — Classic Affine | Baseline | Selected HUD is stable; it may not form a true icon-level trapezoid. |
| 2 — Captured Projective Mesh | Capture stability | No flicker, disappearing plate, or frame-to-frame loss. |
| 2 — Captured Projective Mesh | Geometry | Backing, hotbar, icons, bars, and text use one real trapezoid: rectangular at 30° down, with a visibly narrower far edge at a grazing view. |
| 2 or 3 | Scope | Menus and unrelated GUI elements remain normal. |
| 2 or 3 | Companion pixels | AppleSkin and Detail Armor pixels are captured with their selected status root. |
| 3 — World-Space Texture | Camera Yaw anchor | Plane stays in front while looking left/right without player-body lag. |
| 3 — World-Space Texture | Player Body anchor | Plane stays at the body heading and may move to the side when looking away. |
| 3 — World-Space Texture | Occlude Behind World on | A block between camera and panel hides the panel like a physical object. |
| 3 — World-Space Texture | Occlude Behind World off | Panel remains visible through the same block. |
| 3 — World-Space Texture | Latency/stability | One completed-frame delay is expected; persistent loss, flicker, or stale texture is not. |

## Report format

Provide the installed JAR timestamp or commit if known, the config values,
which HUD mods were visible, a screenshot or short clip if possible, and
`latest.log` from the same run. State whether the test was performed with Iris
enabled.
