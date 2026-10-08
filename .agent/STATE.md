# Current state

- Last updated: 2026-10-08
- Session branch: `arena/b4016c28-tests`
- Read when: starting any task in this repository.

## Active work

### Spatial HUD — Method 4 textures the captured HUD into the purple quad

- Status: implemented and pushed (`4f7162f`). **Not built and not tested at
  runtime.** The agent's GitHub connection has no `workflows` or Actions
  permission, so it could neither change the workflow trigger nor dispatch a
  run; the jar does not exist yet.
- The owner asked for the four-corner purple surface to carry the HUD itself,
  keeping the pitch response. Method 4 therefore captures the selected lower
  HUD again and warps it onto the four configured corners. The plain outline is
  now only the fallback used when the capture is not running.
- Pitch response is unchanged and shared: `PolygonTestRenderer.quad(...)` is
  the single quad definition used by both the capture mesh and the fallback
  guide.
- Method 4 maps only the bottom `POLYGON_SOURCE_HEIGHT` (72) GUI pixels of the
  strip, clamped to the real GUI, not the plane's 184px source rectangle.
  Mapping the taller rectangle puts the HUD in the quad's bottom quarter under
  mostly empty purple, and its below-screen bottom edge is clipped away and
  transparent.
- A capture failure no longer hides the HUD: `SpatialHudElement` draws the
  untouched vanilla root whenever the private capture is not running.
- Blocked on the owner: run the build (`PASTE-ME-CI-SETUP.md`, option A =
  one manual run, option B = fix the branch filter permanently), install the
  jar in the F5W profile, and report per `.agent/runbooks/test-spatial-hud-f5w.md`.

### Spatial HUD — standing model (re-read from the source 2026-10-08)

- One visible mode control: the Setup **Render Mode Slider**
  (`renderModePicker`, 1–4). Config migration is version 20.
  - 1 green = captured lower HUD through a balanced GUI-space mesh.
    (`CLASSIC_AFFINE` is only the enum name; it is not a flat fallback.)
  - 2 blue = the same capture through the stronger 32×24 mesh (default).
  - 3 red = the same capture on a level quad, one completed frame behind.
  - 4 purple = the same capture warped onto four percentage-positioned GUI
    corners, pitch-responsive.
- All four modes use the one private capture. `ExperimentalHudCapture` owns a
  private `GuiRenderState`, `GuiRenderer`, and `TextureTarget`, fed only by the
  wrapped `SpatialHudElement` roots; the two mixins identify that private
  renderer by object identity. No screen, chat, map, debug text, or unrelated
  mod GUI enters it (decision 0001).
- There is **no look-down-angle gate** in the current code.
  `VirtualHudPlane.intersectsViewport()` culls the physical panel only when it
  leaves the viewport or passes behind the camera; `minimumLookDownPitch`
  survives as an unused field that migration forces to 0. Method 4 is exempt
  from the viewport test because four GUI-space handles place its target.
- `SpatialHudConfig.get().enabled` is the only live enable state. **H** opens
  settings and never changes it; **Toggle Spatial HUD** is unbound by default;
  **F6/F7/F8** are direct keys for methods 1–3.
- A capture error latches a session fallback (`failedMethod`), keeps the
  selected method in the config, and logs the failing stage.

### Direct Modrinth updater

- Status: implemented; not the active task.
- Defaults to `%APPDATA%\ModrinthApp\profiles\F5W\mods` and remembers the
  choice. The eventual branch picker must offer `main` plus five recent
  successfully built branches, each with its own compiled jar.

## Non-negotiable constraints

- Capture only the selected lower HUD. Never cancel or broadly reroute GUI
  rendering; the capture redirect stays private and object-identity scoped.
- Do not claim F5W, Iris, AppleSkin, or Detail Armor compatibility without a
  real test result.
- AppleSkin and Detail Armor Bar Reconstructed pixels belong in the same
  capture as their vanilla root.
- Keep all work on the branch this session was handed; push only there.

## Next action

1. Run the build using `PASTE-ME-CI-SETUP.md`, then install the resulting
   `spatial-hud-1.0.0.jar` in the F5W profile, replacing every older
   `spatial-hud` jar.
2. Test Method 4 as `.agent/runbooks/test-spatial-hud-f5w.md` describes: the
   captured hotbar/bars/XP/held-item text warped into the four purple corners,
   the border and handles on those corners, the whole surface following camera
   pitch, nothing left at the vanilla HUD position, and the vanilla HUD (not an
   empty outline) if the capture fails.
3. Send `latest.log` plus a screenshot from that exact session.

## Evidence

- Supplied F5W log: `latest.log` (earlier session, not this change).
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
- Continuity note: `spatial-hud-template-26.3/AGENT_HANDOFF.md`.
- Build instructions for the owner: `PASTE-ME-CI-SETUP.md`.
