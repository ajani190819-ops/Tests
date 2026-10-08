# Build setup — make GitHub build the mod

**Situation on 2026-10-08:** the build file exists and works, but it is still
set to start only for the **previous** session's branch
(`arena/c83497e6-tests`). This session's code is already pushed to
`arena/b4016c28-tests`, so nothing has built automatically yet.

The agent's GitHub connection is not allowed to edit files under
`.github/workflows/` — that is a GitHub rule for app connections, not a bug in
the repo. So this one line needs a human. Pick either option below.

**Download link for the newest successful build (both options):**

https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar

## Option A — start one build now, nothing to paste

1. Open
   https://github.com/ajani190819-ops/Tests/actions/workflows/build-spatial-hud.yml
2. On the right, click **Run workflow**.
3. In the **Branch** dropdown choose `arena/b4016c28-tests`, then click the
   green **Run workflow** button.
4. Wait 3–5 minutes. The new build appears at the top of the list; a green
   check means it published the jar.

Use this when you just want the jar in your `mods` folder today.

## Option B — stop doing that by hand (recommended, ~30 seconds)

Change the branch filter so every agent push builds by itself, in this session
and in every future one.

1. Open that file in the editor, on our branch:

   https://github.com/ajani190819-ops/Tests/edit/arena/b4016c28-tests/.github/workflows/build-spatial-hud.yml

2. Select everything in the text box, delete it, and paste **all** of this:

   ```yaml
   name: Build Spatial HUD

   on:
     push:
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

         - name: Publish jar to rolling release
           if: success()
           env:
             GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
           run: |
             rm -f build/libs/*-sources.jar
             if ! gh release view spatial-hud-latest > /dev/null 2>&1; then
               gh release create spatial-hud-latest \
                 --target "$GITHUB_SHA" \
                 --title "Spatial HUD — latest build" \
                 --notes "Automated build from commit ${GITHUB_SHA::7}. Minecraft 26.3, Fabric." \
                 --prerelease
             fi
             gh release upload spatial-hud-latest build/libs/spatial-hud-*.jar --clobber
   ```

3. Click **Commit changes...**, keep **Commit directly to the
   `arena/b4016c28-tests` branch**, then click **Commit changes**.

4. That commit starts a build by itself. Done — from then on, every change the
   agent pushes to an `arena/...-tests` branch (or to `main`, after a merge)
   builds and updates the download link automatically.

## What the automation does

- On every matching push, GitHub starts a fresh Linux machine, installs JDK 25,
  runs the Gradle build, and attaches the finished `spatial-hud-1.0.0.jar` to
  the rolling release above.
- If a build fails, the agent reads the log from GitHub, fixes the code, and
  pushes; the next build starts by itself.
- Your only job after that: click the download link, drop the jar into your
  instance's `mods` folder (remove any older `spatial-hud` jar first).
