# Build setup — make GitHub build the mod

**Situation on 2026-10-08:** the build file exists, but it is still set to start
only for the **previous** session's branch (`arena/c83497e6-tests`), and it only
publishes one "newest build" JAR. So the newest code is pushed but never
compiled, and the updater has no per-branch builds to offer.

The agent's GitHub connection is not allowed to edit files under
`.github/workflows/` (a GitHub rule for app connections) and cannot start a
build either, so these two short actions are yours. Neither one needs a
terminal, a JDK, or Gradle.

## Action 1 — paste the build file (needed for the build to exist at all)

1. Open this file in the editor, **on our working branch**:

   https://github.com/ajani190819-ops/Tests/edit/arena/b4016c28-tests/.github/workflows/build-spatial-hud.yml

2. Select everything in the text box, delete it, and paste **all** of this:

```yaml
name: Build Spatial HUD

on:
  push:
    # Every session gets a new arena/<id>-tests branch name, so match the whole
    # family (plus main) instead of one hardcoded branch that stops matching.
    branches: [main, 'arena/*-tests']
    paths:
      - 'spatial-hud-template-26.3/**'
      - '.github/workflows/build-spatial-hud.yml'
  workflow_dispatch:

permissions:
  contents: write

jobs:
  build:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: spatial-hud-template-26.3

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Set up JDK 25
        uses: actions/setup-java@v4
        with:
          distribution: temurin
          java-version: 25
          cache: gradle

      - name: Build
        run: |
          chmod +x gradlew
          ./gradlew build --no-daemon

      - name: Publish the jar to the rolling release and to this branch's build
        if: success()
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        run: |
          set -euo pipefail
          rm -f build/libs/*-sources.jar
          JAR="$(ls build/libs/spatial-hud-*.jar 2>/dev/null | head -n 1 || true)"
          if [ -z "$JAR" ] || [ ! -f "$JAR" ]; then
            echo "No Spatial HUD jar was produced in build/libs; nothing published." >&2
            exit 1
          fi
          REF="$GITHUB_REF_NAME"
          SAFE="${REF//\//-}"
          STAMP="$(date -u +%Y-%m-%dT%H:%MZ)"
          echo "Branch $REF (asset suffix $SAFE), commit ${GITHUB_SHA:0:7}"

          # 1. The rolling release. Its asset name stays spatial-hud-1.0.0.jar,
          #    so the plain download link and older updater copies keep working.
          if ! gh release view spatial-hud-latest > /dev/null 2>&1; then
            gh release create spatial-hud-latest \
              --target "$GITHUB_SHA" \
              --title "Spatial HUD — latest build" \
              --notes "Automated build." \
              --prerelease
          fi
          gh release upload spatial-hud-latest "$JAR" --clobber
          gh release edit spatial-hud-latest \
            --notes "Newest build: $REF at commit ${GITHUB_SHA:0:7} ($STAMP)."

          # 2. One release per branch, so the updater's build picker can offer a
          #    real choice: main first, then the newest branches.
          BUILD_TAG="spatial-hud-build-$SAFE"
          if ! gh release view "$BUILD_TAG" > /dev/null 2>&1; then
            gh release create "$BUILD_TAG" \
              --target "$GITHUB_SHA" \
              --title "Spatial HUD — $REF" \
              --notes "Automated build." \
              --prerelease
          fi
          gh release upload "$BUILD_TAG" "$JAR" --clobber
          gh release edit "$BUILD_TAG" \
            --title "Spatial HUD — $REF" \
            --notes "$(printf 'Automated Spatial HUD build.\nBranch: %s\nCommit: %s\nBuilt: %s\n' "$REF" "${GITHUB_SHA:0:7}" "$STAMP")"
```

3. Click **Commit changes...**, keep **Commit directly to the
   `arena/b4016c28-tests` branch**, then click **Commit changes**.

4. That commit starts a build by itself (the file it changes is in its own
   trigger list). Watch it under the repo's **Actions** tab. A green check
   means the jar was published.

Once this is pasted, every future push to `main` or to any `arena/...-tests`
branch builds and publishes automatically — this is a one-time paste.

## Action 2 — replace your saved updater once (needed for the new menu)

The `.bat` file you already have loads its menu from the **previous** session's
branch, which is why your menu has no "choose the build" option yet. Replace
that one file with the current copy.

**Easiest route (browser):**

1. Open
   https://github.com/ajani190819-ops/Tests/blob/arena/b4016c28-tests/spatial-hud-template-26.3/Update-SpatialHUD.bat
2. Click the **download** icon near the top-right of the file view
   (**Download raw file**), or right-click the **Raw** button and choose
   **Save link as…**.
3. Save it over your existing `Update-SpatialHUD.bat` — usually in your
   **Downloads** folder. Keep the name exactly `Update-SpatialHUD.bat`.

**Alternative route (one line, no browser saving):** press **Windows key**, type
`powershell`, open it, paste this, and press Enter. It writes the file straight
into your Downloads folder:

```powershell
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/b4016c28-tests/spatial-hud-template-26.3/Update-SpatialHUD.bat' -OutFile "$env:USERPROFILE\Downloads\Update-SpatialHUD.bat"
```

**How to tell the new menu loaded:** its top lines include a **`Build:`** row
(saying `Newest successful build, any branch` or a branch name), and the menu
has **seven** numbered options. The old menu has no `Build:` row.

Double-click the saved file. Its menu now has:

```text
 [1] Install or update Spatial HUD now
 [2] Choose the build - main or one of the newest branches
 [3] Change the Modrinth mods folder
 [4] Configure optional folder opener (OneCommander / none)
 [5] Check the chosen build details
 [6] Advanced: use a different release feed
 [7] Restore the default folder, build, and no-opener setting
 [Q] Quit
```

Choose **2**, pick the branch you want, then choose **1** to install it. The
choice is remembered, so later runs install that same branch until you change
it. **[R]** inside the list goes back to "newest build on any branch".

While no branch has built yet, option **2** says so instead of showing an empty
list — that is expected until action 1 finishes a build, because a branch can
only be offered once its jar exists. Option **1** still installs the newest
build on any branch in the meantime, and it replaces the Spatial HUD jar in your
F5W `mods` folder by itself (close Minecraft first).

This one replacement is permanent: the file reads the newer of its two helper
copies by a version marker, so it keeps working after the branch is merged into
`main` without another download.

## What the pasted file does

- Builds on a fresh Linux runner with Temurin JDK 25 (the same as before).
- Keeps the **rolling release** `spatial-hud-latest` with the stable asset name
  `spatial-hud-1.0.0.jar`, so the plain download link and any older updater copy
  keep working:

  https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

- Additionally publishes **one release per branch**, tagged
  `spatial-hud-build-<branch>` (for example
  `spatial-hud-build-arena-b4016c28-tests`, and `spatial-hud-build-main` once
  main itself has been built). Each one carries that branch's jar under the same
  file name, and its release notes record the branch, commit, and build time.
  Every build of that branch replaces the asset, so the entry always holds the
  newest jar for that branch.
- The updater's build picker lists those releases: **main first, then the five
  most recently built branches**, with **[A]** for all of them, **[T]** to type
  a name, and **[R]** for the newest build on any branch. A branch only appears
  after it has built successfully, so a listed branch always has a real jar.

## If a build fails

The log is public: repo → **Actions** → the red run → the failing step. Send the
agent the failing step's message; the code fix is normally a one-line change.
