# Current state

- Last updated: 2026-10-08
- Session branch: `arena/b4016c28-tests`
- Read when: starting any task in this repository.

## Where things stand

- **Spatial HUD ships two methods only**: Method 3 (Real 3D Panel) and Method 4
  (Purple Horizontal Panel). Methods 1 and 2 are archived in
  `archive/spatial-hud-methods-1-2/` with a vetted record and source snapshot.
  Decision: `.agent/decisions/0002-methods-3-and-4-only.md`.
- **Method 4 is implemented** as a flat world panel with its normal along Y. It
  reuses Method 3's capture texture and quad, and frames the bottom 72 GUI pixels
  of the strip. Distance, height, and angle are configurable (`horizontalPanel*`,
  category `purple`). Not yet run in game.
- **Failure behaviour**: a capture failure shows the vanilla HUD plus a small red
  square above the hotbar's top-left corner. There is no affine fallback, and no
  purple outline on failure.
- **Repo reorganised** (staged `git mv`): logs to `.agent/evidence/logs/`, the v0
  draft and demo mod and reference jar to `archive/`, the Orca audit and the
  Spatial HUD CI and compatibility docs to `docs/`, the handoff to
  `.agent/history/`, and the Electron app to `apps/arena-link-windows/`. The
  references were fixed in the same change.
- **Not compiled here.** This sandbox has no `javac` or JDK. The Java files parse
  with tree-sitter, and `en_us.json` validates (68 keys). CI on push is the compile
  check. The last CI runs that succeeded predate the Method 4 edits.
- **Installer**: `spatial-hud-template-26.3/Update-SpatialHUD.bat` is the only
  installer, one file (no `.ps1` helper any more). It must be re-downloaded once
  from `arena/b4016c28-tests`; older saved copies are stale. Windows run not yet
  confirmed. `tests/test_spatialhud_installer.py` checks the static contract.
- **Manual workflow dispatch is unavailable** (HTTP 403 from this sandbox). A push
  to the branch is how a build starts.

## Open questions for the user

1. Which render mode the slider was on in the last test (probably 4), and whether
   the HUD was visible in game.
2. From `spatialhud.json`: the values of `renderMethod` and `enabled`.
3. Whether the updater on the PC lists `arena/b4016c28-tests` and installs the
   branch build.

## Non-negotiable constraints

- Capture only the selected lower HUD. Never cancel or broadly reroute GUI
  rendering. The capture redirect stays private and object-identity scoped
  (decision 0001).
- Do not claim F5W, Iris, AppleSkin, or Detail Armor compatibility without a real
  test result.
- AppleSkin and Detail Armor Bar Reconstructed pixels belong in the same capture as
  their vanilla root.
- Work on `arena/b4016c28-tests` only. Before any push, `git fetch origin` and
  rebase. Never force-push.

## Next action

1. Commit and push the session branch (after fetch and rebase). The push starts a
   build, and the updater can then install the branch build.
2. Ask the user the open questions above.
3. Owner runs the F5W test in `.agent/runbooks/test-spatial-hud-f5w.md`, using the
   branch build, and sends `latest.log` from that session.

## Evidence and reference

- Logs: `.agent/evidence/logs/` (`latest.log` = `l2atest.txt` at commit `c8ccc42`;
  crash report from 2026-10-08).
- Technical summary: `.agent/facts/spatial-hud.md`.
- Runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Build and updater process: `docs/spatial-hud/CI-SETUP.md`.
- Compatibility matrix: `docs/spatial-hud/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
