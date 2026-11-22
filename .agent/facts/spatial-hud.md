# Spatial HUD facts

- Status: source-aware handoff; HUD-01 acceptance and scope reconciliation OPEN.
- Source audit: 2026-11-16. No renderer/config behavior changed by this audit.
- Read when: changing Spatial HUD, its updater or F5W compatibility.

## Current source, not the archived three-method narrative

- `gradle.properties`: source 1.0.5, Minecraft 26.3, Loader 0.19.5,
  Fabric API 0.162.0+26.3, Cloth Config 26.3.158; build targets Java 25.
- `SpatialHudConfig.RenderMethod` now contains `WORLD_SPACE_TEXTURE` and
  `POLYGON_TEST` only. The visible picker is excluded; `selectedRenderMethod()`
  unconditionally returns `POLYGON_TEST` (Purple Horizontal Panel). Method 3 is
  shelved; Classic Affine / Captured Mesh are absent as selectable paths.
- **HUD-01's saved Classic Affine / Captured Mesh / World-Space Texture F5W
  request is retained, not completed or silently replaced.** It cannot be
  executed as written on current source. Record current-purple-panel evidence
  separately; get owner scope reconciliation before restoring/redefining methods.
- H opens settings, not the enable toggle. The toggle is unbound by default.
  Current categories: Guide, Setup, Presets, Panel, Wiggle, HUD Contents.
  Presets: Waist, Face, Custom 1, Custom 2. Width is per preset.
- `SpatialHud.isGameplayHudActive` gates by enabled/player/level and Gameplay
  Only. There is no current fixed 30-degree look-down cutoff. Do not assert the
  archived cutoff, green/blue markers or affine fallback as current behavior.
- `ExperimentalHudCapture.fallback` latches an error, keeps the selected method,
  logs the failed stage and shows vanilla with an error indicator. This is not
  a successful hidden switch to an archived renderer.

## Preserved scope and compatibility constraints

- Capture only selected lower-HUD roots: hotbar, bars, XP, held-item label,
  backing and compatible same-root pixels. Never restore global GUI capture or
  broadly reroute unrelated screens, menus, chat, maps or debug overlays.
- Capture uses a private target/renderer and object-identity-scoped redirection;
  keep main GUI state independent. GPU resources must close on shutdown.
- Current capture wrappers are reinforced after registration and, boundedly,
  when hotbar/capture telemetry shows displacement. Source checks cover bounded
  reinforcement and first-person occlusion; they are not F5W visual evidence.
- First-class companion targets remain AppleSkin and Detail Armor Bar
  Reconstructed; retain the normal Iris/other F5W HUD stack for the first test.
  Also observe same-region Bedrock/Immersive Hotbar, DualBar, Armor Indicator,
  Status Effect Bars, Mount Opacity, Durability Warner and Hotbar Slot Cycling
  when actually installed. Do not claim compatibility from source alone.
- Historical `46874e5` was an owner-observed working capture baseline; old
  shader/Cloth Config fixes and build `37730150490` (`7964948`) are historical,
  not compilation/runtime evidence for current 1.0.5 source.

## Build, artifact identity and acceptance

- `./gradlew build test --no-daemon` requires JDK 25 and Gradle/Fabric network.
- Existing workflow: `.github/workflows/build-spatial-hud.yml`; normal pushes
  affecting the mod build/publish a branch-scoped
  `spatial-hud-build-<branch-with-slashes-replaced>` release on success.
  Shared `spatial-hud-latest` is main's curated feed. Hub tries the branch tag
  before the shared fallback: identify the actual downloaded artifact, not just
  its version/filename. Record workflow SHA, asset timestamp and JAR SHA-256.
- A manual workflow dispatch was denied (HTTP 403) in this batch; no run was
  started by it. Do not repeatedly retry or edit workflows to bypass permission.
  No current branch release existed at inspection. A later normal push-triggered
  build is separate evidence; verify its actual result before claiming a JAR.
- Python source contracts / Gradle compile success are not game startup,
  shader/HUD capture, flicker, occlusion, performance or owner acceptance.
- Current runbook: `.agent/runbooks/test-spatial-hud-f5w.md`; setup/build entry:
  `spatial-hud-template-26.3/README.md`; selected-HUD decision:
  `.agent/decisions/0001-selected-hud-only.md` (historical default caveat there).
