# Spatial HUD — archived Methods 1 and 2

- Status: archived 2026-10-08. Not compiled, not shipped, not selectable.
- Last commit that still had them active: `5bab133`. Every removed line can
  also be read with `git show 5bab133:<path>`.
- Why: the owner asked to keep only Method 3 (a real, flat, client-side panel
  in the world) and Method 4 (the purple 2.5D approximation in the GUI layer, since replaced by the flat purple horizontal panel that reuses Method 3's capture).

## What Method 1 and Method 2 were

- **Method 1 (green, `CLASSIC_AFFINE`)** — the captured HUD through a balanced
  GUI-space mesh. Its name is historical: it was never a flat per-root
  fallback.
- **Method 2 (blue, `CAPTURED_MESH`)** — the same capture through a stronger
  32×24 projective mesh, with a far-edge pinch and a curvature setting.
- Both also used the old "safe affine" presentation, which re-drew each vanilla
  root under one affine transform. It also served as the failure fallback for
  Method 3. It is archived too.

## What was removed from the live code

| Item | Where it was | Notes |
|---|---|---|
| Enum values `CLASSIC_AFFINE`, `CAPTURED_MESH` | `SpatialHudConfig.RenderMethod` | Old saved names load as null; config version 21 fixes them. |
| Keys F6 and F7 | `SpatialHud` | F8 = Method 3, F9 = Method 4 now. |
| `compositeProjectiveMesh`, `addWarpMesh`, `addWarpVertex` | `ExperimentalHudCapture` | `drawToMainTarget` and `WARP_BUFFER` stay (Method 4 uses them). |
| `projectWarped` | `VirtualHudPlane` | Only the Method 2 mesh used it. |
| `applySpatialPose` (affine tangent) | `SpatialHudElement`, `SpatialHudPanelElement` | Method 3's failure path is now the vanilla HUD plus a red indicator. |
| `shouldPreserveNativeStatusLayout`, `isStatusLayoutRevealed`, companion-mod checks | `SpatialHud`, `SpatialHudElement` | The AppleSkin / Detail Armor Bar layout switch existed only for Method 1. Their pixels still enter the capture. The `preserveCompanionStatusLayout` JSON field stays, hidden. |
| `modeIndicatorColor` (the green/blue/red top band) | `SpatialHudConfig` | Method 3 now uses the purple border instead. |
| `meshTopEdgeWidthMultiplier`, `meshBottomEdgeWidthMultiplier` | `SpatialHudConfig` | Method 2 only. |
| Guide tabs for Methods 1 and 2 | `SpatialHudConfig`, `en_us.json` | |
| `preserveCompanionStatusLayout` JSON field and its migrations | `SpatialHudConfig` | Nothing read it after the layout switch went. Old files still load (unknown fields are ignored). |
| `VirtualHudPlane.projectAt` and its `lerp` helper | `VirtualHudPlane` | No callers. |
| Method 4 plain outline drawn when capture fails | `SpatialHudPanelElement` | Failure now shows the vanilla HUD plus the red marker only. `PolygonTestRenderer.drawGuide` remains, now uncalled. |

## Still in the code, and why (vetted)

- `experimentalCaptureWarp`, `experimentalCaptureCurvaturePercent`,
  `projectiveIconScaling`, `lookDownPlaneTilt`, `minimumLookDownPitch`: old JSON
  fields. Migrations read them, so they stay (hidden), but nothing renders from
  them any more.
- `VirtualHudPlane.curvedDepth`: curvature is 0 by default. It is still called
  by the shared physical-plane math that Method 3 uses for viewport culling.
  Removing it would change Method 3's culling, so it stays for now.
- `PolygonTestRenderer.drawGuide`: uncalled after the failure outline was
  removed. Kept only because it could not be compiled here to confirm a deletion
  is clean. Delete it in a follow-up once CI is green.

## Source snapshot

`source-at-5bab133/` holds the full pre-change copies of the files that were
edited, so the old code can be compared or revived without digging through git:
`SpatialHud.java`, `SpatialHudConfig.java`, `ExperimentalHudCapture.java`,
`SpatialHudElement.java`, `SpatialHudPanelElement.java`, `VirtualHudPlane.java`,
`en_us.json` (the `WorldSpaceHudRenderer.java` copy was dropped because it is
unchanged and Method 3 still uses it). These copies are not compiled (they live outside the Gradle
source tree).
