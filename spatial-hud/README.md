# Spatial HUD

**Spatial GUI, but for your HUD.** Renders the hotbar, hearts, hunger, armor,
air, XP bar, mount bars and held-item name on a tilted 3D plane floating in
front of you in the world. Client-side only, works on any server.

Press **H** in-game to toggle. Config lives at `config/spatialhud.json`
(distance, plane width/height, tilt, sway, per-element toggles).

## How it works (the interesting part)

1. A mixin cancels vanilla's *flat* bottom-HUD rendering (and only that —
   crosshair, chat, potion icons etc. stay untouched).
2. At the end of the world render pass, we install a custom projection:
   `perspective × planeTransform × guiOrtho`, model-view = identity.
3. We then call vanilla's **own** HUD render methods through `@Invoker`
   accessors. To that code, nothing has changed — it still "draws at pixel
   coordinates" — but those pixels now land on a quad floating in the world
   with real perspective and a sway effect on fast turns.

No framebuffers, no texture copies, no per-pixel work. The per-frame cost is
one extra pass of a few hundred 2D quads — negligible by design.

## Building

Requires JDK 21+ and an internet connection (Gradle downloads Loom, the
Minecraft jar, yarn mappings and Fabric API on first run):

```
cd spatial-hud
gradlew build
```

The jar lands in `build/libs/spatial-hud-0.1.0.jar` — drop it in `mods/`
alongside Fabric API.

First run: edit `gradle.properties` and set `minecraft_version`,
`yarn_mappings`, `loader_version`, `fabric_version` to the exact values
listed for your target version at https://fabricmc.net/develop.

## ⚠ Verify points (this is first-draft, uncompiled code)

Written without a compiler — if the build or launch fails, it will almost
certainly be one of these. Each is isolated and quick to fix:

1. **InGameHud method signatures** (`InGameHudAccessor` / `InGameHudMixin`) —
   names are stable across versions, but a parameter may have been added or
   removed on 26.2 (most likely candidates: `renderHotbar`'s `RenderTickCounter`,
   `renderStatusBars`'s args). The build/mixin error will name the exact method.
2. **`WorldRenderContext.tickCounter()`** — older Fabric API exposed
   `tickDelta()` instead. Fallback already coded in `HudPlaneRenderer`.
3. **`RenderSystem.getModelViewStack()`** — its `MatrixStack` changed method
   names around 1.20.5 (`pushMatrix`/`popMatrix`/`loadIdentity` vs `push`/`pop`).
4. **`VertexSorter`** import path (isolated in `VertexSorterHolder`).
5. **`DrawContext` constructor** — `(MinecraftClient, VertexConsumerProvider)`.
6. **`player.getJumpingMount()` / `hasVehicle()`** yarn names.

## Known limitations (v0.1)

- **Third-party HUD elements are not spatialized yet.** Mods like Detail
  Armor Bar Reconstructed, Bedrock Hotbar, DualBar or Durability Warner draw
  through their own hooks during the normal GUI pass, so they will still
  render flat on screen (or vanish if they mixin into the same vanilla
  methods). Test first with those disabled. A v0.2 could capture the whole
  HUD strip to a texture (Exordium technique) to catch *everything* any mod
  draws down there.
- No perspective on the panel's internal layout (elements render orthographically
  onto the plane — the plane itself sits in perspective).
- Spectator mode: vanilla hides these elements anyway; behavior follows vanilla.
- First-person only for now (a third-person anchored mode like Spatial GUI's
  is a natural v0.3).

## Iterating

This mod is built to be iterated with someone who can run the game:

1. Build → launch → observe.
2. If the build fails: paste the Gradle error.
3. If the game crashes on launch: paste the mixin error from the log.
4. If it runs but looks wrong (plane clipped, upside-down, too close, sway
   nauseating): describe it or tweak `config/spatialhud.json` values — every
   geometric property is a config field, no rebuild needed.
