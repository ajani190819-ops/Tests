# Current state

- Last updated: 2026-10-08
- Session branch: `arena/b4016c28-tests`
- Read when: starting any task in this repository.

## Where things stand

- **Spatial HUD ships two methods only**: Method 3 (Real 3D Panel) and Method 4
  (Purple 2.5D Panel). Methods 1 and 2 are archived in
  `archive/spatial-hud-methods-1-2/` with a vetted record and source snapshot.
  Decision: `.agent/decisions/0002-methods-3-and-4-only.md`.
- **Failure behaviour**: a capture failure shows the vanilla HUD plus a small red
  square above the hotbar's top-left corner. There is no affine fallback, and
  the Method 4 outline is no longer drawn on failure.
- **Repo reorganised** (staged `git mv`): logs to `.agent/evidence/logs/`, the v0
  draft and demo mod and reference jar to `archive/`, the Orca audit and the
  Spatial HUD CI and compatibility docs to `docs/`, the handoff to
  `.agent/history/`, and the Electron app to `apps/arena-link-windows/`. The
  references were fixed in the same change.
- **Not compiled.** This sandbox has no `javac` or JDK. The Java files parse with
  tree-sitter, and the lang JSON validates (80 keys). CI on push is the compile
  check.
- **Updater**: `Update-SpatialHUD.bat` is the only updater. Its build picker lists
  `main` and the five most recently built branches. The branch build exists:
  workflow runs `37864958183` and `37865568690` succeeded, and release
  `spatial-hud-build-arena-b4016c28-tests` holds `spatial-hud-1.0.0.jar`. Not yet
  confirmed that the updater on the PC lists it.
- **Manual workflow dispatch is unavailable** (HTTP 403 from this sandbox). A push
  to the branch is how a build starts.

## Open questions for the user

1. **Method 4 geometry**: the agreed spec is a horizontal panel (normal along Y)
   with the purple border and HUD inside, world-anchored to the player with
   configurable distance, height, and angle, visible at a shallow look-down pitch.
   It is not implemented. Confirm before changing it.
2. Which render mode the slider was on in the last test, and whether the HUD was
   visible in game.
3. From `spatialhud.json`: the values of `renderMethod` and `enabled`.
4. Delete `Get-Latest-SpatialHUD.bat` in the mod folder? It is a deprecated
   redirect, kept until confirmed.

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

1. Commit and push the session branch (after fetch and rebase), which starts a
   build the updater can pick up.
2. Ask the user the open questions above. Do not change Method 4 geometry until
   the user confirms.
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
