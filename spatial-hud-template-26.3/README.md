# Spatial HUD (build project — canonical)

**Spatial GUI, but for your HUD.** Renders the hotbar, hearts, hunger, armor,
air, XP bar, mount bars and held-item name on a tilted 3D plane floating in
front of you. Client-side only. Press **H** in-game to toggle. Config:
`config/spatialhud.json` (distance, planeWidth, height, tilt, sway, element
toggles).

This folder (`spatial-hud-template-26.3/`) is the **active build project** —
official Fabric template for MC 26.3 + our sources. The `spatial-hud/` folder
in the repo root is the older yarn-era draft, kept for reference.

## Fix round 1 (current state)

The original draft was written against **Yarn mappings** — dead past MC
1.21.10. Your 26.3 toolchain uses **Mojang mappings**, so every MC class
reference failed to compile. All sources have been rewritten to official
Mojang names (`Minecraft`, `Gui`, `GuiGraphics`, `KeyMapping`, `DeltaTracker`,
`PoseStack`, `MultiBufferSource`, ...), and `build.gradle` now pins
`mappings loom.officialMojangMappings()` explicitly.

## Requirements — READ THIS ONE

- **JDK 25** (this template compiles with `--release 25`; JDK 21 will fail
  with "invalid source release: 25"). Install Temurin 25 from
  https://adoptium.net (Windows x64 MSI). If you have multiple JDKs and
  Gradle picks the wrong one, set `JAVA_HOME` to the JDK 25 folder.
- Internet for the first Gradle run.

## Building

```
gradlew build
```

Jar: `build\libs\spatial-hud-1.0.0.jar` (not the `-sources` one). Drop into
your instance's `mods/` folder. Requires Fabric Loader 26.3 profile + Fabric
API (your pack already has both).

For the first test, disable HUD-overlap mods (Detail Armor Bar Reconstructed,
Armor Indicator, Bedrock Hotbar, DualBar, Durability Warner HUD) so the plane
is clean.

## Verify points (round 2 risks, in order of likelihood)

These are now **runtime** risks, not build risks — the code compiles, but
26.3's exact internals are unverified:

1. **Mixin signatures at launch** (`GuiAccessor`/`GuiMixin`) — if the game
   crashes during startup with "Critical injection failure" or "method ... not
   found in class ...Gui", one of the six method signatures drifted. The crash
   log names the exact method. → paste it.
2. **`GuiGraphics(Minecraft, MultiBufferSource)` constructor** — compile
   error if changed (unlikely; stable for years).
3. **`PoseStack` method names** — `pushPose/popPose/setIdentity`.
4. **`gameMode.hasStatusBars()` / `player.jumpableVehicle()` / `getTimer()`**
   — used for the survival-vs-mount dispatch; if any fails to compile, paste
   the error (these have fallback-free single call sites).
5. **Visual result** — if the plane is upside-down/mirrored/too close, that's
   config values or one matrix sign; describe what you see, not a crash.

## Self-protection

If the renderer ever throws during play, the mod logs the error once, writes
`enabled=false` into the config, and the flat vanilla HUD returns — it will
never crash your game over a HUD.
