# 2026-10-08 — The updater gets a real build picker

- Branch: `arena/b4016c28-tests`. Build: not yet (the owner must paste the
  workflow; the agent connection cannot edit `.github/workflows/**`).

## What the owner asked for

"The updater should show me the main branch and then the five most recent
branches, so that I can actually pick this new branch."

The Orca updater already had this shape (`main` plus the five newest branches,
with [A]/[T]), so the Spatial HUD updater now matches it, with one difference:
the list is built from **published builds** rather than live branches, so every
entry is certain to have a jar.

## Design chosen

- One release per branch (`spatial-hud-build-<branch>`) holding that branch's
  newest jar under the normal asset name, plus the untouched rolling release.
- The helper reads `/releases?per_page=100` once, filters the branch releases,
  sorts main first then by build recency, and shows five.
- The remembered value stays the existing release tag, so the install path,
  the "advanced feed" option, and older helpers keep working unchanged.
- The .bat picks the newer of two helper copies by a version marker, which
  removes the "downloaded .bat fetches its menu from a dead branch" trap that
  this session hit.

## Verified / not verified

- Executed the workflow's publish shell block locally with a stubbed `gh`:
  fresh build creates and uploads to both releases, a re-build only
  uploads/edits, and a missing jar fails loudly with exit 1.
- Workflow YAML parses; the live release API returns every field the picker
  reads (`tag_name`, `body`, `assets[].name/size/updated_at`).
- Not verified: the PowerShell helper and the batch file, because this sandbox
  has no Windows and no PowerShell. Review only.

## Follow-up worth knowing

The build picker can only list a branch after that branch has built, so `main`
appears once main has been built, and this branch appears after the paste makes
it build. That is deliberate: a branch with no compiled jar is never offered.

## Note for the next session

The owner's paste is a real commit on this branch, so the sandbox copy falls
behind: `git fetch origin` and rebase before the next push, and never include
`.github/workflows/**` in a push from here (the connection is refused outright).
