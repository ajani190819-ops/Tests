# Spatial HUD — Project Handoff

**Last updated:** 2026-10-08  
**Working branch at handoff:** `arena/c83497e6-tests`  
**Latest implementation commit:** `0b4b1b3 Fix polygon toggle compilation`  
**Verified CI build:** [run 37808537092](https://github.com/ajani190819-ops/Tests/actions/runs/37808537092) — passed and published the rolling JAR.

This document is a concise continuity note for the next agent/chat. It describes the **currently released behavior**, rather than earlier superseded experiments.

## Current release

- Rolling-release download: <https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar>
- Asset after the latest successful build: `spatial-hud-1.0.0.jar`, 61,963 bytes, uploaded 2026-10-08 16:25 UTC.
- The local workspace has no JDK installed, so local Gradle cannot run here. CI uses Temurin JDK 25 and is the authoritative build verification.

## Render mode control

The authoritative visible control is **Setup → Render Mode Slider** (`renderModePicker`, values `1`–`4`):

1. Green — balanced captured warp
2. Blue — strong captured projective warp
3. Red — world-space texture
4. Purple — four-corner polygon diagnostic

The historical `renderMethod` enum is intentionally hidden and retained only for compatibility and direct key selection. Configuration migration is now version **20**.

## Method 4 — current, user-requested behavior

The most recent user feedback included a screenshot showing two shapes:

- a large, light-purple/white configured quadrilateral; and
- an unwanted dark captured HUD rectangle inside it.

The user explicitly asked for **only the lighter polygon** and asked for its corners to respond to camera pitch. The latest commits therefore changed Method 4 to a **single direct purple/white outline-and-handle diagnostic**:

- `SpatialHudPanelElement` draws the direct guide and returns before beginning private HUD texture capture.
- `SpatialHudElement` suppresses the selected Spatial HUD roots in Method 4, preventing either native or captured duplicate lower HUD output.
- `SpatialHudConfig.capturesTexture()` returns false in Method 4.
- The prior captured-HUD-to-mesh code still exists in `ExperimentalHudCapture`, but it is intentionally unreachable in current Method 4 flow. Do not re-enable it unless the user explicitly restores the earlier requirement to texture-map the HUD into the test polygon.

### Pitch response

Method 4 uses the eight saved percentage values exactly when the view pitch is level. By default it then responds live to pitch:

- looking **down** pulls the top edge inward/upward and opens the lower edge;
- looking **up** reverses that motion.

New Purple Polygon Test controls in `SpatialHudConfig`:

- `polygonFollowCameraPitch` — default `true`
- `polygonPitchResponsePercent` — bounded `0–100`, default `100`

Implementation is in `PolygonTestRenderer.respondToPitch(...)`; it reads the per-frame `SpatialHud.pitch`. The panel now calls `SpatialHud.updateViewPose()` before drawing the guide, so it is current-frame rather than one-frame stale.

## Enable/disable fix

The Setup **Enable Spatial HUD** toggle previously looked ineffective because `SpatialHud` had an independent static `enabled` value loaded only at startup. That duplicate state was removed.

- `SpatialHudConfig.get().enabled` is now the sole live source of truth.
- Disabling in Setup immediately lets wrapped roots delegate back to vanilla and hides Method 4.
- The separately bindable **Toggle Spatial HUD** key changes and saves that same setting, then logs `Spatial HUD toggled on/off.`
- The keybinding is intentionally unbound by default; users must bind it in Minecraft Controls if they want a keyboard shortcut.

## Relevant files

- `src/client/java/dev/arena/spatialhud/SpatialHudConfig.java` — config fields, slider mapping, v20 migration, capture policy.
- `src/client/java/dev/arena/spatialhud/SpatialHud.java` — live enable state, keybinding handling, camera pitch sampling.
- `src/client/java/dev/arena/spatialhud/SpatialHudPanelElement.java` — per-frame panel behavior; Method 4 single-guide short circuit.
- `src/client/java/dev/arena/spatialhud/SpatialHudElement.java` — selected vanilla root wrapper and Method 4 root suppression.
- `src/client/java/dev/arena/spatialhud/PolygonTestRenderer.java` — direct four-corner guide and pitch transform.
- `src/client/java/dev/arena/spatialhud/ExperimentalHudCapture.java` — private texture capture used by Modes 1–3. Contains no currently active Method 4 path.
- `src/main/resources/assets/spatialhud/lang/en_us.json` — config labels/tooltips and method explanations.
- `README.md` — current public behavior documentation.

## Important unresolved validation point

The user supplied a screenshot of the old two-polygon behavior, but has **not yet confirmed testing the 61,963-byte rolling JAR from the latest successful build**. If they still see the dark rectangle after replacing the JAR and restarting Minecraft, request a fresh `latest.log` from that exact test session and check for duplicate/older Spatial HUD JARs in the mods folder.

The prior (now superseded) Method 4 design captured the lower HUD and warped it into the configured quad. The active design is intentionally guide-only because the user’s most recent wording was “just the lighter one should show.” Confirm the desired final direction before doing more Method 4 texture work.

## Verification performed

Before the latest build:

- JSON parsing passed.
- Custom static assertions passed for single-surface Method 4, pitch settings, config migration, and live enable state.
- `git diff --check` passed.
- The first CI attempt for `2b31a20` failed only due to Java local-variable shadowing in `SpatialHud.onInitializeClient`; `0b4b1b3` renamed the inner variable and CI run `37808537092` passed fully.
