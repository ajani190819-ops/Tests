# 0002 — Keep only Methods 3 and 4

- Status: accepted 2026-10-08 (user request).
- Read when: touching Spatial HUD render modes, config, or docs.

## Decision

Only two presentations ship:

- **Method 3 — Real 3D Panel** (`WORLD_SPACE_TEXTURE`): the captured lower HUD
  on a flat world-space quad, with a purple border.
- **Method 4 — Purple Horizontal Panel** (`POLYGON_TEST`, the saved enum name):
  a flat world panel with its normal along Y, carrying the purple border and the
  captured lower HUD. It is anchored to the player's feet, is visible at a shallow
  look-down pitch, and faces you square-on when you look straight down. Distance,
  height, and angle are configurable.

Methods 1 (classic affine) and 2 (captured projective mesh composite) are
removed from code and config. Their source snapshot and removal record live in
`archive/spatial-hud-methods-1-2/`.

## Consequences

- The render-mode picker is 3–4; config migrations clamp older values into it.
- On capture failure, show the vanilla HUD and a red marker. No affine fallback.
- Method 4 reuses the Method 3 capture texture and quad. It is not a GUI-space
  warp. Config version 22.
