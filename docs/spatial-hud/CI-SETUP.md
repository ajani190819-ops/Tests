# How Spatial HUD builds reach your PC

- Status: **working** as of 2026-10-09. The one-time workflow paste is already on
  the working branch (commit `605ecf6`, owner). Runs `37864958183` and
  `37865568690` built this branch successfully.

## What happens on a push

1. A push to `main` or to any `arena/*-tests` branch that changes
   `spatial-hud-template-26.3/**` or `.github/workflows/build-spatial-hud.yml`
   starts `.github/workflows/build-spatial-hud.yml`.
2. GitHub Actions builds the jar with JDK 25 (Temurin) and `./gradlew build`.
3. The jar is published to two GitHub releases, each under the stable asset
   name `spatial-hud-1.0.0.jar`:
   - `spatial-hud-latest` — the rolling feed, always the newest build of any
     branch. The plain download link in `spatial-hud-template-26.3/README.md`
     points here.
   - `spatial-hud-build-<branch>` — one per branch, for example
     `spatial-hud-build-arena-b4016c28-tests`. The updater's build picker lists
     these.

## How the PC gets a branch build

1. Double-click your copy of `Update-SpatialHUD.bat`. It fetches the current
   PowerShell helper from GitHub on each run, so the menu stays up to date.
2. At **Choose the build to install**, pick the number beside
   `arena/b4016c28-tests` in the list. `[1]` is `main`, followed by the five most
   recently built branches. Use `[A]` to see every branch with a published build,
   or `[T]` to type a branch name. Press Enter or `[M]` to keep the current choice.
3. At **Modrinth mods folder**, press Enter to keep the remembered folder, or paste
   another one.
4. The updater checks the build, backs up the old jar, and replaces it. Then start
   the game from Modrinth.

## Limits

- The agent's GitHub connection **cannot** edit `.github/workflows/` and
  **cannot** start a run from the Actions API (`workflow_dispatch` returns 403).
  It triggers a build by pushing a commit that touches the mod folder.
- Builds are not checked by the agent: no Java or Minecraft libraries exist in
  its sandbox. A run's green check means the code compiled. A red check means
  the log must be read before the jar can be trusted.
- To start a build by hand, open the **Actions** tab on GitHub, choose
  **Build Spatial HUD**, and press **Run workflow** on the branch.
