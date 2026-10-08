# Spatial HUD facts

- Status: four renderer modes compile in CI history; the Method 4 texture
  change is pushed but has no build or runtime result yet.
- Last verified: source re-read on 2026-10-08 at `4f7162f`; the last CI build
  of any revision was run `37808537092` (previous session, now gone from this
  checkout's squashed history).
- Read when: changing Spatial HUD code, its updater, or F5W compatibility.

## Source and release

- Mod source: `spatial-hud-template-26.3/`.
- Rolling artifact: GitHub release `spatial-hud-latest`, asset
  `spatial-hud-1.0.0.jar`.
- GitHub workflow: `.github/workflows/build-spatial-hud.yml` (JDK 25,
  Temurin). Its `on.push.branches` list must be kept current; an agent
  connection cannot edit `.github/workflows/**`, so a human pastes changes —
  see `PASTE-ME-CI-SETUP.md`.
- Source version and asset name may stay `1.0.0`; identify a test build by
  commit hash and asset timestamp.
- This checkout's git history is squashed, so commit hashes mentioned in older
  notes may not exist locally.

## Rendering design

- The one visible selector is the Setup **Render Mode Slider**
  (`renderModePicker`, 1–4); `SpatialHudConfig.selectedRenderMethod()` maps it
  onto the `RenderMethod` enum. `renderMethod` itself stays hidden for
  compatibility and direct key selection. Migration is version 20.
  1. `CLASSIC_AFFINE` (green) — captured HUD through the balanced GUI-space
     mesh. The name is historical; it is **not** a flat per-root fallback.
  2. `CAPTURED_MESH` (blue) — the same capture through the stronger mesh.
  3. `WORLD_SPACE_TEXTURE` (red) — the same capture on a real level quad,
     drawn one completed frame later.
  4. `POLYGON_TEST` (purple) — the same capture warped onto four
     percentage-positioned GUI corners, pitch-responsive.
- Every mode requires the private texture:
  `SpatialHudConfig.capturesTexture()` is true for all four, and
  `SpatialHud.isTextureCaptureActive()` adds the gameplay/screen boundary.
- `ExperimentalHudCapture` captures only the selected lower HUD into a private
  `GuiRenderState` rendered by a private `GuiRenderer` into a private
  `TextureTarget`. Method 2 composites it through a 32×24 mesh;
  `WorldSpaceHudRenderer` draws it for Method 3; `compositePolygonTestMesh`
  warps it into the Method 4 quad. The capture path exits before
  allocation/upload when `isTextureCaptureActive()` fails.
  `SpatialHudGuiRendererMixin` redirects only the private renderer by object
  identity; `SpatialHudGameRendererMixin` runs the composite after the normal
  GUI pass and closes Method 3's buffer on shutdown.
- Method 4 geometry lives in `PolygonTestRenderer`. `quad(cfg, guiWidth,
  guiHeight)` is the single definition used by both the capture mesh and the
  plain fallback guide, so the warped HUD and the outline always share the four
  corners and the live pitch response (`respondToPitch`, driven by
  `SpatialHud.pitch` via `polygonFollowCameraPitch` /
  `polygonPitchResponsePercent`).
- Method 4 draws its own purple identity into the captured texture: a
  translucent interior painted before the vanilla roots extract (so the HUD
  pixels stay readable), then a 3px border and four inward corner handles on
  top. Both are sampled from the same source band, so they land exactly on the
  configured corners. `Show Backing Panel` controls the interior tint only.
- Method 4 samples the bottom `ExperimentalHudCapture.POLYGON_SOURCE_HEIGHT`
  (72) GUI pixels of the strip, clamped to the real GUI. The plane's own source
  rectangle is 184px tall and extends 4px below the screen; the extra height is
  empty sky and the below-screen line is clipped, so using it would leave the
  HUD in the bottom quarter of the quad with a transparent sliver at its base.
- `VirtualHudPlane` is the physical waist-height plane used by Methods 1–3. It
  follows camera yaw, never rotates with camera pitch, and refuses panels with
  a behind-camera corner or no viewport intersection. **There is no
  look-down-angle gate**: `minimumLookDownPitch` remains as an unused field
  that migration forces to 0.
- `SpatialHudConfig.instance` and `registered` are excluded from Cloth Config.
  The read-only **Method Guide — Read This First** tab is transient and never
  serialized.

## Failure behavior

- Any capture/render error calls `ExperimentalHudCapture.fallback(...)`, which
  latches a session fallback, keeps the user's selected method, and logs the
  stage. Changing method retries the capture in the same session.
- `SpatialHudElement` decides per root: feed the private capture when it is
  active; in Method 4, otherwise draw the untouched vanilla root so a failed
  capture cannot leave the player without a HUD; in the other modes, fall
  through to the affine presentation.
- Method 4 draws its plain outline and handles whenever the capture is not
  running, so the control surface stays visible.

## Compatibility

- First-class targets: AppleSkin and Detail Armor Bar Reconstructed. Their
  pixels enter the same capture as the vanilla root they inject into.
- F5W also carries other same-region HUD mods: Bedrock Hotbar, Immersive
  Hotbar, DualBar, Armor Indicator, Status Effect Bars, Mount Opacity,
  Durability Warner HUD.
- No F5W, Iris, or companion-mod compatibility claim is verified by a build.
  Only a real run in the F5W profile can confirm one.

## Related files

- Compatibility test plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- User-facing setup: `spatial-hud-template-26.3/README.md`.
- F5W test runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Design constraint: `.agent/decisions/0001-selected-hud-only.md`.
- Build instructions for the owner: `PASTE-ME-CI-SETUP.md`.
