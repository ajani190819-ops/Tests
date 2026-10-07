# Test Spatial HUD in F5W

- Status: current test procedure.
- Last verified: 2026-10-07.
- Read when: requesting or evaluating an F5W runtime test.

## Before starting

1. Close Minecraft.
2. Install the current rolling Spatial HUD JAR in the F5W profile. Remove or
   replace older copies so Fabric does not load the wrong artifact.
3. Keep the normal F5W renderer and HUD-mod stack enabled. Do not disable Iris
   or companion HUD mods unless the test is explicitly an isolation test.

## Test setup

1. In Mod Menu, enable the experimental captured-texture warp.
2. Enable `Tilt Plane While Looking Down`.
3. Start with `Face-On Look-Down Pitch` at 60 and `Virtual Plane Pitch Offset`
   at 0.
4. Test with native status-bar preservation both on and off if companion bars
   are involved.

## Required observations

Record each result separately:

| Check | Pass condition |
|---|---|
| Capture stability | No flicker, disappearing plate, or frame-to-frame loss |
| Scope | Menus and unrelated GUI elements remain normal |
| Whole-plane geometry | Backing, hotbar, icons, bars, and text use the same shape |
| Head-on geometry | Capture appears rectangular |
| Grazing geometry | Far edge visibly narrows horizontally into a trapezoid |
| Look response | Geometry changes continuously while view pitch changes |
| Companion pixels | AppleSkin and Detail Armor pixels are captured with their status root |

## Report format

Provide the installed JAR timestamp or commit if known, the config values,
which HUD mods were visible, a screenshot or short clip if possible, and
`latest.log` from the same run. State whether the test was performed with Iris
enabled.
