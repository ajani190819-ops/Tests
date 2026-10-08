# Current state

- Last updated: 2026-10-08
- Session branch: `arena/b4016c28-tests`
- Read when: starting any task in this repository.

## Active work

### Spatial HUD updater — a real build picker

- Status: implemented and pushed; **needs two owner actions** (see below).
  Cannot be verified from here: no Windows, no PowerShell, no JDK.
- The owner asked to pick a branch build in the updater, not only the newest
  build. GitHub Actions now publishes one release per branch
  (`spatial-hud-build-<branch>`, e.g. `spatial-hud-build-arena-b4016c28-tests`,
  and `spatial-hud-build-main`), each holding that branch's newest jar under the
  stable asset name; the rolling `spatial-hud-latest` jar is unchanged so the
  plain download link and older updater copies keep working.
- `Update-SpatialHUD.ps1` (helper, version marker 2) fetches
  `/releases?per_page=100` once and lists `spatial-hud-build-*`: **main first,
  then the five most recently built branches**, with [A] all, [T] type a branch
  name, [R] newest build any branch, [M]/Enter keep. A listed branch always has
  a published jar; the choice is remembered in `build-choice.txt` (tag + branch)
  and re-verified with the release API before it is saved.
- `Update-SpatialHUD.bat` now tries two helper URLs (`main`, then this session's
  branch) and runs the copy with the higher `SpatialHUD-Helper-Version` marker,
  falling back to the plain `main` fetch if that fails. That is why one
  re-download of the .bat is needed now and never again.
- **Owner actions** (both in `PASTE-ME-CI-SETUP.md`): (1) paste the updated
  `.github/workflows/build-spatial-hud.yml` on this branch — the agent
  connection cannot touch workflow files, and the current file still triggers
  only for the previous session's branch; (2) replace the saved
  `Update-SpatialHUD.bat` once with the current copy.
- Verified here: the publish shell block was executed locally with a stubbed
  `gh` (fresh build, re-build, and no-jar paths); the workflow YAML parses; the
  live release API returns the fields the picker reads. Not verified: the
  PowerShell helper and the .bat (no PowerShell/Windows available).

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
- Blocked on the owner: the same workflow paste builds it, then install the
  jar in the F5W profile and report per
  `.agent/runbooks/test-spatial-hud-f5w.md`.

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

- Status: implemented, including the branch build picker described above.
- Defaults to `%APPDATA%\ModrinthApp\profiles\F5W\mods` and remembers the
  folder, the chosen build, and the optional folder opener under
  `%LOCALAPPDATA%\SpatialHudUpdater`. The picker offers `main` plus the five
  most recently built branches, and only lists branches with a published jar.

## Non-negotiable constraints

- Capture only the selected lower HUD. Never cancel or broadly reroute GUI
  rendering; the capture redirect stays private and object-identity scoped.
- Do not claim F5W, Iris, AppleSkin, or Detail Armor compatibility without a
  real test result.
- AppleSkin and Detail Armor Bar Reconstructed pixels belong in the same
  capture as their vanilla root.
- Keep all work on the branch this session was handed; push only there.

## Next action

1. Owner: apply both actions in `PASTE-ME-CI-SETUP.md` (paste the workflow on
   this branch; replace the saved `Update-SpatialHUD.bat`).
2. Owner: in the updater, choose option **2**, pick
   `arena/b4016c28-tests`, then option **1** to install it into the F5W profile
   (remove any older `spatial-hud` jar first).
3. Owner: test Method 4 as `.agent/runbooks/test-spatial-hud-f5w.md` describes
   — the captured hotbar/bars/XP/held-item text warped into the four purple
   corners, border and handles on those corners, the whole surface following
   camera pitch, nothing left at the vanilla HUD position, and the vanilla HUD
   (not an empty outline) if the capture fails.
4. Owner: send `latest.log` plus a screenshot from that exact session, and say
   whether the build menu showed the branch you picked.
5. If the owner pasted the workflow file (action 1), the branch now has one
   commit the sandbox does not: `git fetch origin` and rebase before pushing
   again, or the push is rejected as non-fast-forward. The pasted file also
   means the local `.github/workflows/` copy is now out of date in the sandbox;
   never try to push that path.

## Evidence

- Supplied F5W log: `latest.log` (earlier session, not this change).
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
- Continuity note: `spatial-hud-template-26.3/AGENT_HANDOFF.md`.
- Build instructions for the owner: `PASTE-ME-CI-SETUP.md`.
