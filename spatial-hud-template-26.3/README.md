# Spatial HUD

Fabric client mod for the **selected lower HUD only**. Ordinary menus, chat,
maps, debug overlays and unrelated mod GUIs are not capture targets.

## Current source versus the saved test request

The current source version is **1.0.5**, targeting **Minecraft 26.3**, Fabric
Loader **0.19.5**, Fabric API **0.162.0+26.3**, Cloth Config **26.3.158** and
**Java 25**. See `gradle.properties`, `build.gradle` and `fabric.mod.json` for
the authoritative dependency requirements. Mod Menu is optional.

**Important handoff mismatch:** the saved HUD-01 request still calls for
Classic Affine, Captured Mesh and World-Space Texture F5W acceptance. Those are
not three selectable methods in this source. `SpatialHudConfig` retains only
`WORLD_SPACE_TEXTURE` and `POLYGON_TEST` enum identities, and
`selectedRenderMethod()` currently always returns `POLYGON_TEST` — the
**Purple Horizontal Panel**. The method picker is excluded from the settings
UI, Method 3 is shelved, and old Method 1/2 selectors are absent. Do not report
three-method acceptance from this build, change hidden JSON to pretend those
paths are available, or restore retired methods without an owner decision.
The original three-method request remains open for owner scope reconciliation.

Press **H** for settings/guide; opening settings must not toggle enabled state.
Current tabs are Guide, Setup, Presets, Panel, Wiggle and HUD Contents. The
purple panel has Waist, Face, Custom 1 and Custom 2 presets; placement, width,
heading, curvature, wiggle, occlusion and companion-HUD behavior need actual
F5W validation. There is no current fixed 30-degree look-down acceptance gate.
A capture failure can restore vanilla with an error indicator; that is a
failure report, not evidence of an archived affine fallback passing.

## Build and identify the exact artifact

With JDK 25 and the normal Gradle/Fabric dependency network available:

```sh
./gradlew build test --no-daemon
```

The existing **Build Spatial HUD** GitHub workflow builds pushes affecting this
directory and publishes a branch-scoped `spatial-hud-build-<branch-with-slashes-replaced>`
release after success. The shared `spatial-hud-latest` feed is curated from
`main`, not proof of an active Arena branch build. Use the actual branch tag,
workflow commit, asset timestamp and JAR SHA-256; a source version or old
rolling filename alone does not establish provenance. Never keep duplicate
Spatial HUD JARs in the game profile. Generated JARs/caches stay out of Git.

The local Python source-contract suite and a green Gradle build do **not** prove
Minecraft startup, renderer compatibility, captured pixels, performance or
physical-screen correctness. Follow
[the source-aware F5W runbook](../.agent/runbooks/test-spatial-hud-f5w.md), record
current-panel observations separately, and retain the original HUD-01 scope
question. No renderer code is changed by this documentation repair.

The [documentation archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit)
retains the longer pre-migration narrative; Git source and the current runbook
override stale method descriptions there.
