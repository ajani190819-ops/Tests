# Spatial HUD facts

- Status: confirmed in source 2026-10-08. Two shipped methods (3 and 4). Not
  compiled or run in this sandbox; CI on push is the only compile check.
- Last verified: 2026-10-08 (source re-read after Methods 1 and 2 were removed).
- Read when: changing Spatial HUD code, its updater, or F5W compatibility.

## Source and release

- Mod source: `spatial-hud-template-26.3/`.
- Workflow: `.github/workflows/build-spatial-hud.yml` (JDK 25, Temurin, `./gradlew
  build`). Triggers on pushes to `main` and `arena/*-tests` that touch the mod
  folder or the workflow file. An agent connection cannot dispatch a run (403).
- Each successful run publishes the jar (`spatial-hud-1.0.0.jar`) to two releases:
  - `spatial-hud-latest`: the rolling feed. The plain download link in
    `spatial-hud-template-26.3/README.md` points here.
  - `spatial-hud-build-<branch>`: one per branch. Release notes carry
    `Branch: <name>`.
- Source version and asset name stay `1.0.0`. Identify a test build by commit and
  asset timestamp.
- Full owner process: `docs/spatial-hud/CI-SETUP.md`.

## Updater

- `Update-SpatialHUD.bat` is the only updater. On each run it fetches
  `Update-SpatialHUD.ps1` from `main` and from the session branch, and keeps the
  copy with the higher `SpatialHUD-Helper-Version` marker.
  `Get-Latest-SpatialHUD.bat` is a redirect. Its helper is deleted.
- The helper reads `/releases?per_page=100` and lists `spatial-hud-build-*`:
  `[1]` is main, then the five most recently built branches. `[A]` lists every
  branch with a published build, `[T]` takes a typed branch name, `[R]` takes the
  newest build from any branch, `[D]` saves a copy to Downloads without changing
  mods, and `[M]`/Enter keeps the current choice.
- Remembered state lives in `%LOCALAPPDATA%\SpatialHudUpdater`. The mods folder
  defaults to `%APPDATA%\ModrinthApp\profiles\F5W\mods`.
- Install safety: download to a temp folder, verify the file is a Spatial HUD jar
  through its `fabric.mod.json` id, move old copies aside, and delete the backups
  only after the new jar is in place.
- Not verified on Windows. No PowerShell in the agent sandbox.

## Rendering design

- The only visible selector is the Setup **Render Mode Slider**
  (`renderModePicker`, 3–4). **F8** selects Method 3 and **F9** selects Method 4.
  Config version is 21. `renderMethod` is hidden and defaults to
  `WORLD_SPACE_TEXTURE`. The v21 step clamps the picker to 3–4.
  - `WORLD_SPACE_TEXTURE` (Method 3, "Real 3D Panel"): the captured lower HUD on
    a flat world-space quad, with a 3 px purple border.
  - `POLYGON_TEST` (Method 4, "Purple 2.5D Panel"): the captured lower HUD
    warped onto four percentage-positioned GUI corners, pitch-responsive.
- Both methods use one private capture. `ExperimentalHudCapture` owns a private
  `GuiRenderState`, `GuiRenderer`, and `TextureTarget`, fed only by the wrapped
  `SpatialHudElement` roots. `SpatialHudGuiRendererMixin` redirects only that
  renderer, by object identity. `SpatialHudGameRendererMixin` runs the composite
  after the normal GUI pass.
- Method 3 geometry: `VirtualHudPlane` (camera-yaw map surface, waist height,
  no pitch follow). `WorldSpaceHudRenderer` draws the quad. Its tilt is
  `topY = cos(pitch)`, `topZ = forward·sin(pitch)`. Method 3 has viewport culling.
  The world-space anchor (Camera Yaw or Player Body) and terrain occlusion apply
  to Method 3 only.
- Method 4 geometry lives in `PolygonTestRenderer`. `quad(cfg, guiWidth,
  guiHeight)` is the one definition shared by the capture and the warp. With
  `polygonFollowCameraPitch`, the corners are projected as a physical sheet fixed
  to the player's feet and head yaw. Constants: `REFERENCE_EYE_HEIGHT` 1.62,
  `SHEET_DISTANCE` 2.0, `MAX_TILT_DEGREES` 70, `MIN_DEPTH_FRACTION` 0.25.
  `PolygonTestRenderer.Quad` stores the square-to-quad homography, so straight HUD
  lines stay straight. Crossed or collapsed handles fall back to bilinear.
- Method 4 samples only the bottom `ExperimentalHudCapture.POLYGON_SOURCE_HEIGHT`
  (72) GUI pixels of the strip, not the plane's 184px source rectangle. The taller
  rectangle would put the HUD in the bottom quarter of the quad.
- Method 4 draws its purple interior into the captured texture before the vanilla
  roots extract. It then draws the 3 px border and four corner handles on top.
  `Show Backing Panel` controls the interior tint only.
- **Method 4 geometry change pending.** The user has specified a horizontal panel
  (normal along Y) carrying the purple border and HUD, world-anchored to the
  player with configurable distance, height, and angle. Visible at a shallow look-down
  pitch, face-on when looking straight down. Not implemented; confirm before changing.
- `SpatialHudConfig.instance` and `registered` are excluded from Cloth Config. The
  read-only **Method Guide — Read This First** tab is transient and never saved.

## Failure behavior

- A capture error calls `ExperimentalHudCapture.fallback(...)`. It latches a
  session fallback, keeps the selected method, and logs the stage. Changing method
  retries the capture in the same session. `hasCaptureFailed()` reports the latch.
- `SpatialHudElement` draws the untouched vanilla root whenever the private
  capture is not running, so the player keeps a HUD.
- `SpatialHudPanelElement` draws a red 8×8 square at `(guiWidth/2 − 100,
  guiHeight − 31)` when `hasCaptureFailed()`. It sits just above the hotbar's
  top-left corner. Outline `0xFF3A0000`, fill `0xFFE53935`.

## Compatibility

- First-class targets: AppleSkin and Detail Armor Bar Reconstructed. Their pixels
  enter the same capture as the vanilla root they inject into.
- F5W also carries other same-region HUD mods (see `docs/spatial-hud/COMPATIBILITY.md`).
- No F5W, Iris, or companion-mod compatibility claim is verified by a build. Only
  a real run in the F5W profile can confirm one.

## Related files

- Method decision: `.agent/decisions/0002-methods-3-and-4-only.md`.
- Capture constraint: `.agent/decisions/0001-selected-hud-only.md`.
- Removed Methods 1 and 2: `archive/spatial-hud-methods-1-2/README.md`.
- Compatibility matrix: `docs/spatial-hud/COMPATIBILITY.md`.
- User setup guide: `spatial-hud-template-26.3/README.md`.
- F5W test runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Build and updater process: `docs/spatial-hud/CI-SETUP.md`.
