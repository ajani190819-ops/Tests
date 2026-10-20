# GitHub Actions Workflow Setup (1-Minute Copy Step)

Arena's GitHub integration cannot directly add or edit files inside `.github/workflows/` (GitHub restricts workflow file writes unless a token holds the special `workflows` permission).

Ready-to-use workflow files have already been written inside [`docs/ci/`](../ci/):

1. **[`docs/ci/ci.yml`](../ci/ci.yml) -> `.github/workflows/ci.yml`** *(recommended — adds automatic tests for OrcaSlicer plugins, `Orca-Plugins.bat`, docs sync, and `arena-link-windows` on every push & pull request)*:
   1. Open **Add file -> Create new file** on GitHub (`main` branch).
   2. Type `.github/workflows/ci.yml` as the file path.
   3. Paste the exact contents of [`docs/ci/ci.yml`](../ci/ci.yml) and click **Commit changes**.

2. **[`docs/ci/build-spatial-hud.yml`](../ci/build-spatial-hud.yml) -> `.github/workflows/build-spatial-hud.yml`** *(hardens the existing Spatial HUD workflow)*:
   - Restricts pushes to `main` and `arena/**` (plus `pull_request`), enables Gradle dependency caching + concurrency cancellation, runs JUnit 5 tests, and **only updates the rolling `spatial-hud-latest` release from `main`** while publishing branch test builds to `spatial-hud-build-<branch>` so scratch branches never overwrite `main` builds.
   - To apply: open `.github/workflows/build-spatial-hud.yml` on GitHub -> click the pencil (**Edit**) icon -> paste [`docs/ci/build-spatial-hud.yml`](../ci/build-spatial-hud.yml) -> **Commit changes**.
