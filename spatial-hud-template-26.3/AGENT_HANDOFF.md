# Spatial HUD — Project Handoff

**Last updated:** 2026-10-08
**Working branch at handoff:** `arena/b4016c28-tests`
**Latest implementation commit:** `4f7162f Method 4: warp the captured lower HUD onto the purple quad`
**CI build for that commit:** none yet — see “Build is blocked on you” below.

This document is a concise continuity note for the next agent/chat. It describes
the **currently released behavior**, plus the change that has not been built yet.

## Build and updater are blocked on you (two short actions)

The rolling jar still contains the **previous** revision: the Method 4 change
and the updater change are pushed but never compiled.

- `spatial-hud-1.0.0.jar`, 61,963 bytes, uploaded 2026-10-08 16:30 UTC, is the
  old build.
- Nothing built automatically because (1) the workflow still starts only for
  the previous session's branch (`arena/c83497e6-tests`), and (2) the agent's
  GitHub connection has no `workflows`/Actions permission, so it can neither
  edit `.github/workflows/**` nor dispatch a run.
- `PASTE-ME-CI-SETUP.md` (repository root) is the current, authoritative
  instruction: **action 1** paste the updated workflow file on this branch
  (that commit builds by itself and creates the per-branch releases), **action
  2** replace the saved `Update-SpatialHUD.bat` once so the menu has the build
  picker.
- The local workspace has no JDK, no PowerShell and no Windows; Gradle also
  needs Maven hosts this sandbox cannot reach. CI is the only way to compile.
- Roll link:
  <https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar>

## Render mode control

The authoritative visible control is **Setup → Render Mode Slider**
(`renderModePicker`, values `1`–`4`):

1. Green — captured lower HUD on the balanced GUI-space mesh
2. Blue — the same capture on the stronger 32 × 24 mesh (default)
3. Red — the same capture on a real level quad
4. Purple — the same capture warped onto four GUI-space corners, pitch-responsive

The historical `renderMethod` enum stays hidden and is retained only for
compatibility and direct key selection. Configuration migration is version 20.

## Method 4 — current, user-requested behavior

The owner's instruction for this revision was: *“texture the hood into that
purple 4 corner thing that has pitch response.”* Method 4 is therefore a texture
mode again, not the outline-only diagnostic that the previous session shipped.

- `SpatialHudPanelElement` calls `ExperimentalHudCapture.beginFrame(...)` for
  Method 4 **before** the wrapped vanilla roots extract, then
  `capturePanelDecorations(...)`. The selected lower HUD feeds the private
  texture exactly as it does in Methods 1–3.
- `ExperimentalHudCapture.compositePolygonTestMesh(...)` warps that texture
  onto `PolygonTestRenderer.quad(cfg, guiWidth, guiHeight)`, so the HUD follows
  the same four corners and the same live pitch response as the outline.
- `SpatialHudConfig.capturesTexture()` returns `true` in every mode now.
- Purple identity is captured with the HUD: a translucent interior is painted
  **before** the roots extract (so hotbar/bars/text stay readable on it), then a
  3 px border and four inward corner handles are painted **over** them. Because
  the whole source band is mapped onto the quad, the border and handles land on
  the configured corners. `Show Backing Panel` controls only the interior tint.
- Method 4 samples the bottom **72** GUI pixels of the strip
  (`ExperimentalHudCapture.POLYGON_SOURCE_HEIGHT`), clamped to the real GUI —
  not the plane's 184 px source rectangle. The extra height is empty sky, and
  the plane's 4 px below the screen are clipped away and transparent, so using
  the tall rectangle leaves the HUD in the quad's bottom quarter with a
  see-through sliver at its base. This number is the first knob to revisit if the
  owner says the HUD looks too small or that a modded bar is missing.
- Fallbacks: if the capture is not running, `PolygonTestRenderer.drawGuide(...)`
  draws the plain outline and handles, and `SpatialHudElement` draws the
  untouched vanilla root instead of hiding the HUD. A partial capture can never
  leave the player with no HUD, and one failed capture is latched per session
  (`failedMethod`) while the selected method is kept.
- The warped surface is composited over the GUI inside its four corners, so it
  can cover whatever else is there (for example the crosshair region). Methods
  1–3 have the same property over their own area.

### Pitch response

Method 4 uses the eight saved percentage values exactly when the view pitch is
level. By default it then responds live to pitch:

- looking **down** pulls the top edge inward/upward and opens the lower edge;
- looking **up** reverses that motion.

Controls in `SpatialHudConfig`: `polygonFollowCameraPitch` (default `true`) and
`polygonPitchResponsePercent` (bounded `0`–`100`, default `100`).
`PolygonTestRenderer.respondToPitch(...)` reads the per-frame `SpatialHud.pitch`,
and `SpatialHudPanelElement` calls `SpatialHud.updateViewPose()` before drawing,
so the value is current-frame rather than one frame stale. The composite reads
the same `quad(...)` call, which is why the texture moves with the outline.

## Updater: choosing a build (added 2026-10-08)

The owner asked to pick a branch build from the updater instead of only taking
the newest jar. Menu option **2** now offers:

- `main` first, then the five most recently built branches;
- [A] every branch with a published build, [T] type a branch name,
  [R] the newest build on any branch, [M]/Enter keep the current choice.

Mechanics the next session must not break:

- `spatial-hud-build-<branch>` releases are created and updated by the workflow;
  the picker lists them from `/releases?per_page=100` and reads the branch name
  from the release notes line `Branch: <name>` (falling back to the tag suffix).
  A branch with no published jar never appears.
- The remembered choice stays a release tag, so install, "check details", and
  the advanced feed option all reuse the existing code path.
- `Update-SpatialHUD.bat` fetches its helper from `main` and from this session's
  branch, keeping the higher `SpatialHUD-Helper-Version` marker (missing = 0).
  If a future session changes the helper, it must bump that marker and add its
  own branch to the candidate list in both .bat files.
- The rolling `spatial-hud-latest` release and the asset name
  `spatial-hud-1.0.0.jar` must never change: they are the plain download link
  and the install path of updater copies already saved on the owner's PC.

## Enable/disable behavior (unchanged)

- `SpatialHudConfig.get().enabled` is the sole live source of truth; disabling
  lets wrapped roots delegate back to vanilla immediately.
- **H** opens settings and never changes the enable state.
- **Toggle Spatial HUD** is intentionally unbound by default and changes and
  saves that same setting, then logs `Spatial HUD toggled on/off.`

## Relevant files

- `src/client/java/dev/arena/spatialhud/SpatialHudConfig.java` — config fields,
  slider mapping, v20 migration, capture policy.
- `src/client/java/dev/arena/spatialhud/SpatialHud.java` — live enable state,
  keybinding handling, camera pitch sampling.
- `src/client/java/dev/arena/spatialhud/SpatialHudPanelElement.java` — starts
  the capture per mode; Method 4 wiring and guide fallback.
- `src/client/java/dev/arena/spatialhud/SpatialHudElement.java` — selected
  vanilla root wrapper; Method 4 capture feed and native fallback.
- `src/client/java/dev/arena/spatialhud/PolygonTestRenderer.java` — the single
  quad definition, the pitch transform, and the fallback guide.
- `src/client/java/dev/arena/spatialhud/ExperimentalHudCapture.java` — private
  capture, Method 2 mesh, Method 4 polygon mesh, purple decoration drawing.
- `src/client/java/dev/arena/spatialhud/WorldSpaceHudRenderer.java` — Method 3.
- `src/main/resources/assets/spatialhud/lang/en_us.json` — config labels,
  tooltips, method descriptions (updated for the new Method 4).
- `README.md` — public behavior documentation (updated).
- `Update-SpatialHUD.ps1` / `Update-SpatialHUD.bat` / `Get-Latest-SpatialHUD.*`
  — the Modrinth updater, now with the build picker.
- Repository root `PASTE-ME-CI-SETUP.md` — the exact workflow file to paste and
  the two owner actions; workflow edits need a human.

## Important unresolved validation point

Nothing about this revision has run in Minecraft. When the jar exists and the
owner tests it in F5W, ask for:

1. a screenshot or clip of Method 4 with the HUD inside the quad;
2. `latest.log` from that exact session (check for capture-stage errors and for
   a duplicate older `spatial-hud` jar in the mods folder);
3. whether the quad's HUD looks too small (band height) and whether any modded
   bar is missing.

If the owner instead reports the **old** dark rectangle plus a light polygon,
that is the previous revision's screenshot, not this build — confirm the jar
size and timestamp first.

## Verification performed

For `4f7162f`, before it was pushed:

- `en_us.json` re-parsed as JSON after editing; all six text replacements were
  verified to apply exactly once.
- Static structure check over every Java file in the mod (brace/paren/bracket
  balance after stripping strings and comments).
- `git diff --check` passed.
- Source-level review of the whole Method 4 path: panel start of capture, root
  feed, composite call, quad definition, fallback branches.

**Not done: no compilation and no runtime test** — there is no JDK here and CI
could not be started. The next session must read the CI log before claiming the
change works.
