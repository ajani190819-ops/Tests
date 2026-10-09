# 0002 — Keep only Methods 3 and 4

- Status: accepted 2026-10-08 (user request).
- Read when: touching Spatial HUD render modes, config, or docs.

## Decision

Only two presentations ship:

- **Method 3 — Real 3D Panel** (`WORLD_SPACE_TEXTURE`): the captured lower HUD
  on a flat world-space quad, with a purple border.
- **Method 4 — Purple 2.5D Panel** (`POLYGON_TEST`): the captured lower HUD
  warped onto four GUI-space corners.

Methods 1 (classic affine) and 2 (captured projective mesh composite) are
removed from code and config. Their source snapshot and removal record live in
`archive/spatial-hud-methods-1-2/`.

## Consequences

- The render-mode picker is 3–4; config migrations clamp older values into it.
- On capture failure, show the vanilla HUD and a red marker. No affine fallback.
- Method 4 geometry change (horizontal panel, normal along Y) is agreed but not
  implemented; check with the user before changing it.
