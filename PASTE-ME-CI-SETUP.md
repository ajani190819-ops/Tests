# ONE-TIME SETUP — let GitHub build the mod for us (~60 seconds)

**Why:** GitHub won't let the agent's connection create automation files (a
security rule for app tokens). Only a human logged into the website can do
it. You do this **once** — after that, every build happens in the cloud:
no JDK, no Gradle, no command prompt, no ZIPs on your machine ever again.

## Steps

1. Open this link (it opens the "create new file" editor on our branch):
   https://github.com/ajani190819-ops/Tests/new/arena/c83497e6-tests

2. In the **file name box** (where it says `Name your file...`), type exactly:
   ```
   .github/workflows/build-spatial-hud.yml
   ```
   (the editor creates the folders automatically as you type the slashes)

3. In the big **file contents box**, paste ALL of this:

   ```yaml
   name: Build Spatial HUD

   on:
     push:
       branches: [arena/c83497e6-tests]
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

4. Click **Commit changes...** → **Commit directly to the `arena/c83497e6-tests` branch** → **Commit changes**.

5. Done. Committing the file automatically starts the first build.
   Tell the agent it's done — it watches the build, fixes any errors, and
   re-runs until a working jar appears here:

   **Download link (after first successful build):**
   https://github.com/ajani190819-ops/Tests/releases/tag/spatial-hud-latest

## What the automation does

- On every code change to `spatial-hud-template-26.3/`, GitHub spins up a
  fresh Linux machine, installs JDK 25, runs the Gradle build, and attaches
  the finished `spatial-hud-1.0.0.jar` to the rolling release above.
- If a build fails, the agent reads the build log from GitHub, fixes the
  code, and pushes — the next build starts automatically. You watch progress
  (if you want) under the repo's **Actions** tab.
- Your only job from then on: click the download link, drop the jar in
  your instance's `mods` folder.
