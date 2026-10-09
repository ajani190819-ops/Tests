# Spatial HUD render roadmap: Method 3 and Method 4

Status: **planning.** Written 2026-10-09. Nothing in this file is verified in the
game yet, except where a log is named.

## Why this is step by step

Both methods fail at the same point today: the world-space draw in
`WorldSpaceHudRenderer.renderPlane` throws `Close the existing render pass before
performing additional commands` (`.agent/evidence/logs/latest.log`, 22:27:24).
Until a draw succeeds, nothing downstream can be seen, so each stage below is one
build, one game test, and one log. A stage is done only when its acceptance
check passes in the game.

Method 4 (purple horizontal panel) goes first, because its geometry is the
simplest: a flat sheet at a fixed point near the feet. Method 3 follows with the
same steps.

## What exists today

| Piece | Where | State |
|---|---|---|
| HUD capture into a private texture | `ExperimentalHudCapture` | Works. The log says the hotbar root was extracted into its texture. |
| Corner geometry, Method 3 | `WorldSpaceHudRenderer.computePlaneState` | Written. Not seen in game. |
| Corner geometry, Method 4 | `WorldSpaceHudRenderer.purplePanelState` | Written. Not seen in game. |
| Vertex staging during extraction | `WorldSpaceHudRenderer.extractPlane` / `stagePlaneGeometry` | Written. Built by CI (`7b7e0f2`). Not confirmed in game. |
| Draw in the level pass | `WorldSpaceHudRenderer.renderPlane` / `drawToLevelTarget` | **Blocked.** `drawToLevelTarget` opens its own render pass inside the same callback where the upload failed, so it is likely refused the same way. Unconfirmed. |
| Failure marker | `SpatialHudPanelElement.drawFailureIndicator` | Works. Red for Method 3, purple for Method 4. |

## Stage 0: prove that anything can be drawn in the world

**Goal:** one plain coloured quad appears in the world at the Method 4 spot. No
capture texture yet.

**Why first:** the render pass refusal is the single blocker. Stage 0 finds a
route that the game accepts.

**Routes to try, in order:**

1. **Submit through the level** (preferred). Fabric passes a
   `SubmitNodeCollector` in `LevelRenderContext.submitNodeCollector()`. Geometry
   submitted there is drawn inside the level's own passes, so no extra render pass
   is opened. Exact Minecraft 26.3 method names and render types must be checked
   against the compiled jar. The CI build is the compile check.
2. **Keep the current draw, but only outside the level pass.** Try a later event
   such as `LevelRenderEvents.END_MAIN`. The Fabric docs say END_MAIN is "at the
   end of the main render pass", so this is unconfirmed.
3. **Draw after the level, in the GUI phase.** This is always pass-free, but it
   is not depth-tested against the world, so it cannot be occluded. Last resort.

**Status (build after `d809115`):** route 1 is implemented. `WorldSpaceSolidQuad` submits the Method 4 corners with `context.submitNodeCollector().submitCustom(SubmitRenderPhases.SOLID, ...)` in `LevelRenderEvents.COLLECT_SUBMITS`, and draws them with `RenderTypes.debugFilledBox()`. The API calls follow Fabric's own test mods (`FeatureRendererTest`, `LecternRendererMixin`), checked at Fabric tag `0.162.0+26.3`. Only `new PoseStack()` was not seen in those tests, so CI is the check for it. The old `AFTER_TRANSLUCENT_TERRAIN` draw is no longer registered. Only Method 4 is drawn.

**Acceptance:**
- A solid quad is visible at the Method 4 spot.
- `latest.log` has no `Close the existing render pass` line for that draw.
- F8 and F9 still switch methods, and the vanilla HUD still shows when the
  capture is off.

**Log to send:** `latest.log` after one F9 press.

## Stage 1: an outline with correct perspective (Method 4 first)

**Goal:** a thin border drawn along the four corners of the Method 4 panel. It
must look like a flat sheet in the world, not a flat sticker on the screen.

**Acceptance:**
- The four outline corners land on the four corners that `purplePanelState`
  computes. Check by standing still and comparing against a fixed block.
- Walk toward and away from the panel: the outline scales with distance.
- Look straight down: the outline looks like a square (face-on). Look at a
  shallow angle: it foreshortens, with the far edge shorter.
- Put a block between the player and the panel: the outline is hidden behind it.

**Not in this stage:** texture, HUD elements, colours beyond one.

**Status (build after `850b58f`, border added):** the purple fill is shrunk to the inner rectangle, and a white border ring (3% of the panel's width and height) is drawn around it as four solid strips. Both go through the same level submit route as stage 0. **Result: passed** (user report, 2026-10-09). The user confirmed the outline test: the perspective looks right, and blocks in the way hide the panel. The user did not send a separate note for the corner match or the walk test, so they are covered by the same report. No log was sent for this build.

## Stage 2: positioning (Method 4)

**Goal:** the panel sits where the config says.

**Settings in scope:** `horizontalPanelDistance` (1.25), `horizontalPanelHeight`
(0.9), `horizontalPanelAngle` (90, range 20–90), `planeWidth`.

**Status (2026-10-09):** stage 1 is passed. This stage needs no code change: the four settings above and the heading option (`horizontalPanelAnchor`) already exist in the build. The work is the game test below, then the table. If a value does not behave as described, record it here before any code change.

**Result: passed** (user report, 2026-10-09). Distance, height, angle and heading all work as described.

**Known issue (open):** while walking, the panel stutters and jitters forward and back, at a lower rate than the rest of the game (the user sees 50–200 FPS, but the panel moves slowly). It is smooth when standing still. Cause, from the code: Method 4 reads `player.position()`, `yBodyRot` and `getYRot()` (`WorldSpaceHudRenderer.purplePanelState`). Those change once per game tick (20 per second). The camera uses values interpolated to each frame. Planned fix: interpolate the panel's position and heading with the frame's partial tick, as the camera does. This is a fix to an accepted stage, so it goes in its own build before stage 3.

**Test (one value at a time, the others at default):** set each value, look at the panel from about 3 blocks away, and note the result.

| Setting | Value tried | Result |
|---|---|---|
| Panel Distance | 0.5 / 1.25 (default) / 3.0 | |
| Panel Height Above Feet | 0.4 / 0.9 (default) / 1.5 | |
| Panel Angle | 20 / 60 / 90 (default) | |
| Method 4 Heading | Player Body (default): turn your head, the panel stays. Camera Yaw: turn your head, the panel turns with it. | |

**Acceptance:**
- Distance changes move the panel toward and away from the feet.
- Height moves it up and down.
- Angle 90 is flat, and face-on when looking straight down. Lower angles tilt
  the near edge toward you.
- Heading: with `horizontalPanelAnchor` = Player Body (the default), turning your head does not move the panel. With Camera Yaw, it turns with your horizontal view. Method 3 has its own setting (`worldSpaceAnchor`).

**Output:** a short table of tested values, written into this file.

## Stage 3: put the capture on the outline (Method 4)

**Goal:** the captured bottom 72 px band of the HUD fills the outline.

**Acceptance:**
- The band fills the outline with no stretching. The aspect matches `purpleSourceRect`.
- The hotbar is readable from normal play distance.
- Turning the head does not change the picture.

**Depends on:** Stage 0 (a draw that works). The capture itself already works.

## Stage 4: map HUD elements onto the panel

**Goal:** each HUD element (hotbar, health, hunger, armour, air, experience, held
item name) can be placed on the panel on its own.

**Steps:**
1. Measure the captured band in a screenshot. Record each element's pixel box.
2. Draw one element at a time with a fixed box. Confirm each one lands in the
   right place.
3. Add config to show or hide each element.

**Acceptance:** each element appears in its own region, and turning it off hides
only that element.

## Stage 5: Method 3 (the real flat plane, with the same steps)

Repeat stages 1–4 for `computePlaneState`. The differences: Method 3 is anchored to
the eye or the body (`worldSpaceAnchor`), uses `virtualYaw`, `virtualPitch`,
`virtualRoll`, and the offsets, and samples the full capture envelope
(`VirtualHudPlane.forGui`). Its acceptance checks are the same, with its own
config names.

## Stage 6: hardening

- Remove the diagnostic-only code paths once the draw is stable.
- Keep the failure indicator and the vanilla fallback.
- Check frame cost (one buffer upload per frame).
- Update `docs/spatial-hud/CI-SETUP.md`, `.agent/facts/spatial-hud.md`, and the
  README with the final behaviour.

## Working rules for every stage

1. One stage per build. Do not start the next stage until the user confirms the
   acceptance check.
2. Each build is installed with `Update-SpatialHUD.bat`. The user sends
   `latest.log` after each game test. The latest log must come from the build
   under test; check the build time against the log time.
3. Every log line that matters is quoted in the stage's notes, not paraphrased.
4. A stage that fails is recorded here with the log line, before the next attempt.

## Open decisions

- **Stage 0 route:** route 1 (submit through the level) is preferred. Route 2 or 3
  if route 1 cannot be made to compile against 26.3.
- **Start method:** Method 4 first, as planned above, unless you want Method 3 first.
- **Outline style:** one solid colour per method (purple for Method 4), or a
  single neutral colour for testing.

## Log history

| Date | Build | Result | Log |
|---|---|---|---|
| 2026-10-08 22:26 EDT (2026-10-09 02:26 UTC) | Probably `fd365ba` (built 02:09 UTC, 51,041 B). Not confirmed. | Both methods fail at the draw: `IllegalStateException: Close the existing render pass`, thrown from `StagedVertexBuffer.upload` inside `renderPlane`. | `.agent/evidence/logs/latest.log` (22:27:24) |
| After 2026-10-09 02:46 UTC | Installed by the new installer. Probably `3708186` or later. Not confirmed. | The updater worked. No game log from that build has been sent. | — |
