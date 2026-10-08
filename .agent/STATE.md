# Current state

- Last updated: 2026-10-07
- Session branch: `arena/c83497e6-tests`
- Read when: starting any task in this repository.

## Active work

### Spatial HUD render methods

- Status: all three methods compile and are published; every method still needs
  real F5W runtime approval.
- Goal: provide one clear render-method choice without mixing renderer paths:
  1. Classic Affine (original stable HUD transform), 2. Captured Projective
  Mesh (GUI-space texture warp), and 3. World-Space Texture (captured texture
  on a real level quad).
- `bb99cda` passed CI but its newer projective/PiP path regressed the actual
  F5W renderer. Do not use it as a runtime baseline.
- `5c245d5` restores `ExperimentalHudCapture.java` and its two mixin
  registrations exactly from user-observed working camera-yaw revision
  `46874e5`; it removes the PiP-map accessors introduced by `bb99cda`.
- `d2308fb` adds `renderMethod`, migrates saved `experimentalCaptureWarp`
  settings to Captured Projective Mesh, and reorganizes Cloth Config into clear
  categories and localized descriptions. Method 3 uses the same selected-HUD
  private capture but draws the completed previous-frame texture in the level.
  It exposes Camera Yaw/Player Body anchor and world-occlusion settings.
- Methods 1 and 2 retain camera-yaw anchoring with no body-yaw branch. Their
  fixed plane is face-on at 30° down and projectively trapezoidal at grazing
  angles. Method 3 is intentionally configurable per the user's selection.
- H opens settings; a separate unbound Toggle Spatial HUD action remains in
  Minecraft Controls.
- The attached log identifies the actual launch failure: custom pipeline
  `spatialhud:world_texture_through_world` fails resource reload because its
  cloned entity snippet requests undefined `Sampler1`. Fabric then aborts
  loading, explaining the missing mod/UI and disturbed resource/animation
  state. `036ca35` removes that custom shader pipeline completely: Method 3
  now uses only vanilla `ENTITY_TRANSLUCENT` (depth on) and `GUI_TEXTURED`
  (depth off) pipelines. No Spatial HUD custom pipeline is registered.
- `d6aae3a` also excludes Cloth Config runtime singletons and keeps Method 3's
  GPU buffer lazy. GitHub Actions run `37711539940` compiled and published
  `036ca35` with JDK 25; the rolling JAR was updated at 2026-10-08 01:10 UTC.
  A local Gradle build is unavailable because this sandbox has no Java runtime.
- The configuration/UI follows the user's simplification request: Cloth Config
  is reduced to three sections—Setup & Render Method, Panel Positioning &
  Orientation, and HUD Contents—with detailed contextual tooltips. The panel
  carries an actual top-edge method marker: green for Classic Affine, blue for
  Captured Projective Mesh, and red for World-Space Texture. Texture methods
  capture that marker with the rest of the panel. GitHub Actions run
  `37713731571` compiled and published `b1c9600` with JDK 25; the rolling JAR
  was updated at 2026-10-08 01:36 UTC.

### Direct Modrinth updater

- Status: implemented; not the active task.
- Target defaults to `%APPDATA%\ModrinthApp\profiles\F5W\mods` and is
  remembered. Folder opening is optional and never forced to Windows Explorer.
- The eventual branch picker must offer `main` plus five recent successfully
  built branches, each with its own compiled JAR.

## Non-negotiable constraints

- Do not restore global GUI cancellation, whole-screen offscreen capture, or
  a general GUI mixin. Only the selected lower HUD may be captured.
- Do not claim F5W, Iris, AppleSkin, or Detail Armor compatibility without a
  real test result.
- AppleSkin and Detail Armor Bar Reconstructed pixels belong in the same
  selected capture when their vanilla status root is captured.
- Methods 1 and 2 must keep the waist-height, camera-yaw hologram: no
  player-body rotation, rectangular at 30° down, and horizontally narrower at
  higher/grazing views. For Method 3, the user selected a configurable Camera
  Yaw/Player Body anchor and configurable terrain occlusion.
- Keep all work on `arena/c83497e6-tests`; commit and push each completed
  change.

## Next action

1. Install the rolling JAR updated at 2026-10-08 01:10 UTC in F5W, replacing
   every older `spatial-hud` JAR. The new `latest.log` must contain
   `Spatial HUD render-method build initialized` and must not contain
   `spatialhud:world_texture_through_world` or `Sampler1`.
2. Verify H opens the config without a `No GUI provider registered for field
   ... SpatialHudConfig.instance` error. Confirm the three sections and all
   three named methods with their long contextual descriptions: green Method 1,
   blue Method 2, and red Method 3.
3. Test Method 1 as the baseline. Test Method 2 at 30° down and a grazing
   view: backing and hotbar must form one trapezoid without flicker or body-yaw
   rotation.
4. Test Method 3 separately with Camera Yaw, then Player Body, and with
   Occlude Behind World both enabled and disabled. It will show the previous
   completed frame by design; record any persistent loss/flicker or depth
   failure.
5. For both texture methods, record hotbar slots/items, status bars, AppleSkin,
   Detail Armor, XP, and held-item text separately; check the log for a
   hotbar-root capture message and any capture failure latch.

## Evidence

- Supplied F5W log: `latest.log`.
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
