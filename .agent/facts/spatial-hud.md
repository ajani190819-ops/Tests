# Spatial HUD facts

- Status: confirmed in source 2026-10-08. Two shipped methods (3 and 4). Method 4
  horizontal panel implemented 2026-10-08. Not compiled or run in this sandbox; CI on
  push is the only compile check.
- Last verified: 2026-10-08 (source re-read after the Method 4 horizontal panel).
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

## Installer

- `Update-SpatialHUD.bat` is the only installer. It is one file, modelled on
  `Orca-Plugins.bat`: menu, build picker, folder picker, download check, and
  install all live in it. There is no second helper file.
- Version: `rem UPDATER_VERSION <n> end`, mirrored in `set "UPDATER_VERSION=<n>"`.
  Self-update hands over only to a copy with a higher number.
- Build list: `GET /repos/{repo}/releases?per_page=100`, filtered to tags starting
  `spatial-hud-build-`. Branch name = `Branch:` line in the release body, or the
  tag suffix. Menu: `[1]` is main, then the five most recently built branches.
  Number = position in that same list. `[L]` is the newest build of any branch,
  `[T]` takes a typed branch name, `[4]` forgets the choices.
- Release tag: `spatial-hud-build-` plus the branch with `/` replaced by `-`,
  which matches the workflow's `SAFE="${REF//\//-}"`. Newest any-branch build:
  `spatial-hud-latest`. Asset name: `spatial-hud-1.0.0.jar`.
- Remembered state lives in `%LOCALAPPDATA%\SpatialHudUpdater`
  (`build.txt`, `mods-folder.txt`). It is written only after an install succeeds.
  The mods folder defaults to `%APPDATA%\ModrinthApp\profiles\F5W\mods`.
- Install safety: download to `%TEMP%`, check size and the Fabric mod id
  `spatialhud`, move old `spatial-hud-*.jar` to the backup folder, copy the new jar
  in, and put the old one back if the copy fails.
- Removed from the earlier installer, on purpose: the OneCommander folder opener,
  the save-to-Downloads option, the release details view, and the alternative
  release feed. Say so if you want any of them back.

## Rendering design

- Method 4 (the purple panel) is the only active method. `selectedRenderMethod()`
  always returns `POLYGON_TEST`. Method 3 is shelved: its settings (`renderModePicker`,
  `distance`, `virtual*`, `worldSpace*`, `showPanel`, the Method 3 guide card, and the
  flat-map tilt) are `@ConfigEntry.Gui.Excluded`. They stay in the JSON so old files
  still load, and their code still compiles. The F8 and F9 keys were removed, as the
  user asked for config-only switching. Config version is still 22.
- Config tabs (Cloth Config category ids, in order): `guide`, `setup` (Enabled,
  Only During Gameplay), `presets` (Panel Preset, Waist, Face, Custom 1, Custom 2),
  `panel` (Panel Width, Heading, Angle, Shoulder Camera, fill, border, edges,
  occlusion, third-person exception), `wiggle` (all `panelWiggle*`), `contents` (HUD
  contents, Picture Offset, Slot Cycling Width). Field declaration order sets tab order.
  - `WORLD_SPACE_TEXTURE` (Method 3, "Real 3D Panel"): the captured lower HUD on a
    flat, world-anchored quad, with a 3 px purple border.
  - `POLYGON_TEST` (Method 4, "Purple Horizontal Panel"; the enum name is kept
    because it is saved in configs): a flat world panel with its normal along Y,
    carrying the purple border and the captured HUD. Its look-down pitch makes it
    face you square-on at `horizontalPanelAngle` 90 (looking straight down). A
    shallower look-down pitch tilts it.
- Both methods use one private capture. `ExperimentalHudCapture` owns a private
  `GuiRenderState`, `GuiRenderer`, and `TextureTarget`, fed only by the wrapped
  `SpatialHudElement` roots. `SpatialHudGuiRendererMixin` redirects only that
  renderer, by object identity. `SpatialHudGameRendererMixin` runs the composite
  after the normal GUI pass.
- Method 3 geometry: `VirtualHudPlane` (camera-yaw map surface, waist height, no
  pitch follow) plus the shared `WorldSpaceHudRenderer` quad. Its tilt is
  `topY = cos(pitch)`, `topZ = forward·sin(pitch)`. Method 3 has viewport culling.
  The world-space anchor (Camera Yaw or Player Body) and terrain occlusion apply to
  Method 3 only.
- Method 4 geometry lives in `WorldSpaceHudRenderer.purplePanelState`. It reuses
  the same capture texture and quad. Anchor = feet, `yBodyRot` heading. Centre =
  feet + forward × `horizontalPanelDistance` at y = feet + `horizontalPanelHeight`.
  Look-down angle A (20–90): in-plane up `u = sin(A)·forward + cos(A)·up`, normal
  `n = −cos(A)·forward + sin(A)·up`. Width = `planeWidth`, height matches the
  sampled band. Always depth-tested. No culling.
- Method 4 samples only the band from `ExperimentalHudCapture.purpleSourceRect`:
  bottom `PURPLE_SOURCE_HEIGHT` (72) GUI pixels, clamped to the real GUI, and
  horizontally the centre ± 112 GUI px (hotbar plus offhand). With **Show Slot
  Cycling** on (`showSlotCycling`, default on) it is ± 172 px, so Hotbar Slot
  Cycling's side display (`hotbarslotcycling:cycling_slots`) sits inside it. A
  taller rectangle would put the HUD in a small strip under empty purple.
  Because the panel's width is fixed (`planeWidth`), a wider band makes the
  hotbar appear smaller.
- Hotbar Slot Cycling attaches its element after HOTBAR (Puzzles Lib calls
  `HudElementRegistry.attachElementAfter`). `SpatialHud` wraps it with
  `replaceElement` in `ClientLifecycleEvents.CLIENT_STARTED`, the only point where
  it is guaranteed to exist. The wrap is not yet confirmed in game.
- Method 4 capture draws nothing purple into the texture. `capturePanelDecorations`
  returns early for Method 4, so the backing and the 3 px purple edge stripes are
  gone. The purple fill and white border are world geometry. Method 3 still draws
  its backing (`showPanel`) and border into the texture.
- Method 4 depth modes (`horizontalPanelOcclusion`, `horizontalPanelThirdPersonException`).
  Occluded (default): the fill is `RenderTypes.debugFilledBox()` (depth test, no write),
  and the band is `RenderTypes.entityTranslucent(id, false)` (depth test and write).
  Body exclusion (`horizontalPanelThirdPersonException`, now both views): blocks and mobs are ignored too, so the panel is open. In first person, the open panel (and occlusion off) is drawn by `WorldSpaceHudRenderer.drawOverHand` from the `GameRenderer.render3dHud` RETURN mixin, after the hand pass, with its own storage and a render pass on the main target. The world pass skips it (`PlaneState.overHand`). Third person keeps the open path in the world pass. Hand, armour and particles therefore sit under the panel in first person. Not verified in-game.
  Open (occlusion off, or body exclusion): the band uses
  `RenderTypes.textSeeThrough(id)` (no depth test or write, pipeline
  `pipeline/text_see_through`, vertex layout position, UV, colour, light). Solid
  rects (fill and border) use `textSeeThrough` on a 1x1 white texture
  (`SolidColorTexture`), tinted by the vertex colour. Verified in the 26.x source
  (mc-dataminning/build-changes, `RenderTypes.java` and `RenderPipelines.java`).
  `RenderTypes.gui()` and `RenderTypes.guiTextured(...)` do not exist in 26.x.
  Writers set every element of their layout (light is set even on layouts without
  it, as vanilla glyph code does), so a vertex cannot be left incomplete.
- Fail-safe: `WorldSpaceSolidQuad.openPathReady()` creates the white texture once.
  If that fails, or an open draw throws, `disableOpenPath` logs one warning and
  the panel draws occluded for the rest of the session. An occluded draw that throws
  stops the band (`bandDisabled`) and logs an error. Nothing rethrows into the render frame.
- Picture offset (`horizontalPanelPictureOffset`, percent, default -4, range -10..10; -3 was the confirmed position): the band's bottom edge moves by that gap, and the panel is taller by `panelHeightScale(gap)` so the band keeps its aspect. Band UVs are unchanged. The user confirmed -3 is the right position. -20 cut off more of the hotbar. The range was widened to -10..10 at the user's request.
- Placement presets (`horizontalPanelPreset`: WAIST, FACE, CUSTOM_ONE, CUSTOM_TWO):
  `panelPlacement(cfg)` returns `attachToCamera`, `distance`, `height`.
  - WAIST: feet anchor, `horizontalPanelDistance` and `horizontalPanelHeight`. Heading comes from `horizontalPanelAnchor`. Unchanged from before.
  - FACE: view-locked. Camera anchor (`mainCamera().position()`). The view basis is built from the wiggle-lagged `shownYaw` and `shownPitch` (roll 0), not from the raw camera vectors. Centre = anchor + forward x distance + up x height, so the panel stays square to the view and keeps its place on screen as you look around. The yaw and pitch that build the basis are the wiggle-lagged `shownYaw` and `shownPitch`, so the panel swings behind turns and tilts when Wiggle Heading is on. The anchor position takes the position lag. The angle setting does not apply.
- Curve (`horizontalPanelCurveDegrees`, 0 to 120, default 0 = flat, all presets): `WorldSpaceSolidQuad.Shape` bends the panel around the viewer like a curved monitor. The arc keeps the flat width as its length, so higher values shorten the chord. Vertical edges stay straight. The centre stays put. Each rectangle (fill, border strips, captured band) is split into `CURVE_SEGMENTS` (16) flat strips when curved. `PlaneState` carries the `Shape`, and `submitPanel` and `submitCapturedBand` draw from it. Not verified in-game.
  - CUSTOM_ONE/TWO: their own distance and height, plus an `AttachToCamera` flag. Attach to Camera on uses the same view-locked path as Face.
  The anchor point goes through the same wiggle as the feet, so the lag applies. Presets are config-only. Saving and loading presets is roadmap work.
- Shoulder camera (`horizontalPanelFollowShoulderCamera`, default on): the feet-anchored target is moved sideways by the camera's lateral offset from the eye, measured along the panel's own right. The shift goes through the wiggle. With position wiggle off, it snaps. Only shifts when the camera is off the eye, so first person is unaffected. Not verified in-game. Depends on the shoulder mod moving the real camera position, not just the view matrix.
- Slot cycling width (`slotCyclingHalfWidth`, GUI px from centre, default 172, range 112..200):
  `purpleSourceRect` uses it as the band half-width while Slot Cycling is shown. The
  panel widens with the band, and the UVs follow it. Clamped in code too. The hotbar's
  own half-width (112) is the floor.
- Hypothesis (water, now reported fixed): the occluded band's depth write hid translucent
  water behind the panel. Open mode is the fix path.
- Crash `Missing elements in vertex` (report 03:01:50, `crash-2026-10-09_03.01.50-client.txt`):
  caused by `TexturedRenderer.vertex` picking a different vertex layout from the
  `open` flag while the buffer was always `entityTranslucent`. Turning occlusion
  off (open = true) hit it. Fixed: the writer now always uses the entity layout.
  Rule: a vertex writer must match the format of the buffer it writes to.
- `SpatialHudConfig.instance` and `registered` are excluded from Cloth Config. The
  read-only **Method Guide — Read This First** tab is transient and never saved.
- Legacy `polygon*Percent` and `polygonFollowCameraPitch` fields are kept only as
  `@Gui.Excluded` so old configs still load.

## Failure behavior

- **Upload and draw are split.** The new log (`ea11ba5`, 2026-10-08 22:27) showed
  the next failure: `StagedVertexBuffer.upload()` inside `renderPlane` also throws
  `Close the existing render pass`. Only the draw is allowed inside the level pass.
  So the vertices are now staged in `stagePlaneGeometry()` during extraction
  (`END_EXTRACTION`, no pass open), and `renderPlane` only draws the staged
  `ExecuteInfo`. `WorldSpaceHudRenderer.endFrame()` still runs after the GUI pass.
  Not yet confirmed in game.
- **Never end a frame's buffer inside a level render pass.** The user's log
  (2026-10-09, `spatial-hud-template-26.3/latest.log` on the branch) showed both
  methods failing with `Close the existing render pass before performing
  additional commands`, thrown by `StagedVertexBuffer.endFrame()` called from
  `renderPlane`, which runs inside `AFTER_TRANSLUCENT_TERRAIN`. The fix moves the
  call to `WorldSpaceHudRenderer.endFrame()`, invoked from the GameRenderer hook
  after `GuiRenderer.render()`, the same point where the capture composite ends
  its own frame. Not yet confirmed in game.
- A capture error calls `ExperimentalHudCapture.fallback(...)`. It latches a
  session fallback, keeps the selected method, and logs the stage. Changing method
  retries the capture in the same session. `hasCaptureFailed()` reports the latch.
- `SpatialHudElement` draws the untouched vanilla root whenever the private
  capture is not running, so the player keeps a HUD.
- `SpatialHudPanelElement` draws an 8×8 square at `(guiWidth/2 − 100,
  guiHeight − 31)` when `hasCaptureFailed()`. It sits just above the hotbar's
  top-left corner. Red (outline `0xFF3A0000`, fill `0xFFE53935`) when the failed
  method was Method 3; purple (outline `0xFF2A0A3D`, fill `0xFFC75CFF`) when it
  was Method 4. The colour comes from `ExperimentalHudCapture.failedMethod()`.
  On failure, the vanilla HUD is shown and the marker only; no purple outline.

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
