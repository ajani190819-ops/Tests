# 2026-10-08 — Method 4 gets the captured HUD back

- Branch: `arena/b4016c28-tests`. Implementation commit: `4f7162f`.
- Built/run: no. See below.

## What the owner asked for

The handoff's open question (“confirm the desired final direction before doing
more Method 4 texture work”) was answered directly: *texture the hood into that
purple 4 corner thing that has pitch response.* So the outline-only diagnostic
from the previous session was withdrawn and the capture-to-quad design returned.

## What changed

- Method 4 starts the private capture again and warps the selected lower HUD
  onto its four GUI corners, sharing one quad definition with the pitch
  transform.
- The purple interior is painted under the captured HUD; the border and corner
  handles are painted over it, all inside the same texture, so they land on the
  configured corners.
- Method 4 samples the bottom 72 GUI pixels of the strip instead of the plane's
  184 px source rectangle.
- A capture failure now shows the vanilla HUD rather than hiding it.

## What went wrong with the build

The agent connection could not push a change to `.github/workflows/**`
(`refusing to allow a GitHub App to create or update workflow ... without
workflows permission`) and could not dispatch `Build Spatial HUD` either
(`HTTP 403: Resource not accessible by integration`). A first push attempt that
included the workflow edit was rejected as a whole; the commit was rebuilt from
the base commit so the branch contains code and docs only.

Consequence: the workflow still starts only for `arena/c83497e6-tests`, and the
rolling jar is still the previous revision. `PASTE-ME-CI-SETUP.md` now carries
both remedies for the owner.

## Lesson for the next session

- Editing `.github/workflows/**` from this sandbox is impossible by design.
  Check for this **before** promising the owner a test build, and prefer
  `PASTE-ME-CI-SETUP.md` in the plan from the start.
- `git checkout HEAD -- <path>` restores from the commit that already has the
  change; when dropping an edit, restore from the base commit instead.
