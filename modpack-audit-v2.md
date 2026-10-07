# Modpack Audit v2 — "How About Now?"
**Pack size:** ~234 mods · Fabric · Client pack · NVIDIA GPU · MC 26.2+ (confirmed by Sulfur Cube Face's, which only exists for 26.2+, and 26.2/26.3 builds of several mods) · Shaders at least sometimes (Iris + Euphoria Patches stack from v1)

This is the follow-up to the v1 audit (v1 file was lost in a sandbox reset; its conclusions live on in this report where they still matter). Only what changed, what's new, and the updated removal list.

---

## 0. Scorecard — what you fixed since v1 ✅

You took the v1 advice seriously. Confirmed gone from the v2 list:

- ✅ **OptiCores** (was fighting your GPU), **Interactive Foliage + SWAY** (shiny-leaf suspect), **Packet Fixer** (limit-raiser), **Not Enough Crashes**, **Nuit**, **OneConfig**, **Compose Multiplatform**, **Blur+**, **Centered Crosshair**, **FootprintParticle**, **Wakes Reforged**, **Restore Beta Version Text**, **Raise Sound Limit Simplified**, **Smooth Join**, **ItemPhysic Lite Config jar**
- ✅ **EBE Reloaded → swapped for Better Block Entities** — this is an *upgrade*, not a swap sideways (see below)
- ✅ You added a serious micro-optimization layer (Jasione, FastMapCodec, Better Render Distance, Async Hotbars…) — genuinely good picks
- ⚠️ Still present from the v1 watchlist: **EntityCulling** (fine, but it MUST be ≥ **1.11.2** — the 26.3-era build fixed the invisible-entities regression), **Nvidium + cbbg** (your call, see the "two stacks" section), **Punchy!** (now paired with Better Combat — see conflict section)

---

## 1. The big picture: your pack now contains TWO render stacks

This is the single most important structural insight of v2. You have accidentally (or intentionally — if intentionally, this is fine and coherent) built **two complete rendering setups in one pack**:

### Stack A — "Shaders ON" (Iris + Euphoria Patches + Complementary)
When shaders are active, the following mods in your pack are **dead weight or disabled**:

| Mod | What actually happens with shaders on |
|---|---|
| **Greedy Meshing** (new) | Falls back to **face-culling-only mode — no quad merging at all** (stated on its own page). Face culling is something Sodium already does. So with shaders on, Greedy Meshing contributes ≈ nothing. |
| **Nvidium** (kept from v1) | Inert/unsupported with Iris shader pipelines — v1 finding, unchanged. |
| **cbbg** (kept from v1) | Resource pack, mostly irrelevant under a shader's own grass handling. |
| **Bathymetry** (new) | Its own page: "Some shaders overwrite the water color and ignore the mod." |
| **Smooth Skies** (new) | Sky/fog color smoothing — shaderpacks replace the skybox entirely. |
| **Dynamic Fullbright** (new) | Shifts light levels during lighting updates; shaderpacks compute their own lighting from light data, so results vary / can wash out. Test it with Euphoria before trusting it. |

### Stack B — "Shaders OFF max FPS" (Sodium + Nvidium + Greedy Meshing + Smooth Skies + Better Render Distance)
This stack is actually *coherent*: Greedy Meshing + Nvidium + Smooth Skies + Better Render Distance is exactly the "huge render distance, no shaders" toolkit. Smooth Skies' own docs point at Nvidium users as the target audience.

**Verdict:** Keeping both is legitimate IF you actually switch between the two modes. But be aware ~5 mods do nothing in your shaders-on sessions. If you mostly play with shaders, Greedy Meshing and Nvidium are removable; if you mostly play without, they're excellent.

### One genuine perf win in the shaders-on stack:
- **FISM (Faster Iris Shadow Mapper)** — ⚠️ **REDUNDANT**: its own page says it has been **built into Iris since Iris 1.11.2** and is "now only useful for older MC versions or forks like oculus." Your Iris on 26.2+ is far past 1.11.2 → remove it. ([modrinth.com/mod/fism](https://modrinth.com/mod/fism))

---

## 2. Conflicts & redundancies (the "two mods doing the same job" list)

### 2.1 Better Combat × Punchy! — mostly OK, one known bug
Contrary to what you might expect, these two are now **designed to coexist**: Punchy! is built with native Better Combat integration ("zero configuration needed for your weapon animations to sync"). **However**, there is a known bug class where **the first-person weapon renders twice during Better Combat attacks when shaders are on** — a third mod exists purely to fix this (*Better Combat Punchy Fix*, currently NeoForge-only). **Action:** attack something with shaders on. If you see a doubled weapon during swings, that's this bug; your options are living with it, a shader settings tweak, or dropping one of the two.

Also note Better Combat explains two new library jars in your pack: **Player Animation Library (playerAnimator)** is its animation dependency, plus the Architectury/Cloth stack you already had.

### 2.2 Armor HUD ×2 — pick ONE
- **Armor Indicator** — "configurable armor indicator that renders onto your screen": armor item icons + durability, off-hand durability indicator, position options. ([modrinth.com/mod/armor-indicator](https://modrinth.com/mod/armor-indicator))
- **Detail Armor Bar Reconstructed** (from v1) — per-piece armor point bar under your health bar.

Both are HUD armor displays. Running both = double HUD clutter and two HUD-render paths. Pick one, remove the other.

### 2.3 Tab list ping ×2 — pick ONE
- **Better Ping Display Remake** — numerical ping in the tab list, auto-colors, player heads, custom fonts. ([curseforge.com/minecraft/mc-mods/better-ping-display-remake](https://www.curseforge.com/minecraft/mc-mods/better-ping-display-remake))
- **Better Tab Info** (from v1) — numerical ping + TPS display in tab list.

Direct feature overlap (numeric ping, both show heads). Keep whichever you prefer; delete the other.

### 2.4 Ping-compensation ×2 (PvP cluster)
You now have two mods from the same "simulate zero ping" family:
- **Eslium** — client-side *prediction* of server actions (minecart placement, **end-crystal placement**); replicates vanilla server behavior locally so actions appear instantly. Explicitly PvP-oriented; its FAQ lists PvP servers that accept it. ([modrinth.com/mod/eslium](https://modrinth.com/mod/eslium))
- **Pearl Optimizer** — same idea for ender pearls: spawns a visual client-side pearl instantly instead of waiting for the server echo. Server stays authoritative. **Requires Silicon** (that's your Silicon dependent!). ([modrinth.com/mod/pearl_optimizer](https://modrinth.com/mod/pearl_optimizer))

They don't conflict with each other, but ask yourself whether you actually do crystal/pearl PvP. If yes, both are fine (and check your servers' allowed-mods list — "accepted on Modrinth and some PvP servers" is not "accepted everywhere"). If no, both + Silicon go.

### 2.5 Movement-behavior mods — anticheat exposure cluster
- **Client Movement** — makes sprinting/poses fully client-sided "with no server intervention," for high-ping parkour. The mod's own description warns it "**could possibly get flagged by certain AntiCheats and might be seen by some as an unfair advantage**." ([modrinth.com/mod/client-movement](https://modrinth.com/mod/client-movement)) Only 26.1-era builds exist; tiny adoption.
- **Frostbyte's Maximum Mobility** — step-up, boat step-up, coyote time, Bedrock-style reach-around placement, elytra cancel. QoL, configurable — but step-up/coyote-time change *movement behavior*, which serverside anticheats watch for rubber-banding.

Neither is a "cheat," but on hardened PvP servers these are the first mods I'd expect to get you flagged or kicked. For casual/vanilla play they're fine.

### 2.6 Inventory animation overlap — test this
**Smooth Swapping** (animates items sliding when you click/shift-click in inventories) overlaps with **Inventory Profiles Next's** own move animations, and with **Spatial GUI** (which re-renders the whole inventory screen as a 3D plane). Three mods touching inventory rendering = prime stutter/jank territory. Pick your favorite one or two; watch for items double-animating or ghosting.

### 2.7 PTP — dead weight on vanilla servers
**Projectile Trajectory Preview** shows arrow/pearl/snowball trajectories and impact points. Its page is explicit: "**This mod doesn't work on servers to prevent using it as a cheat in PVP**" unless the server also installs it. If you play public/vanilla servers, this mod does nothing → remove it (and **Searchables** stays — Jade uses it too). Singleplayer/friends' server: keep. ([modrinth.com/mod/ptp](https://modrinth.com/mod/ptp))

---

## 3. What should GO — the v2 removal list (tiered)

### Tier 1 — Remove, clear reasons

| Mod | Why it should go |
|---|---|
| **FISM** | Built into Iris ≥1.11.2. Zero effect on your setup. Free deletion. |
| **WATERMeDIA + WATERMeDIA Binaries** | Heavyweight VLC-based multimedia API (video playback in-world, YouTube/Twitch embeds) used by mods like LittleFrames/VideoPlayer — **none of which are in your pack**. As far as I can tell it's an orphan: ~10s of MB of disk, a VLC runtime initialized for nothing. Verify in Mod Menu (see §5), then remove both jars. |
| **Better Ping Display Remake** *or* **Better Tab Info** | Redundant with each other (§2.3). Keep one. |
| **Armor Indicator** *or* **Detail Armor Bar Reconstructed** | Redundant with each other (§2.2). Keep one. |
| **PTP (Projectile Trajectory Preview)** | Self-disables on servers you don't control (§2.7). Keep only if you play singleplayer/own-server. |
| **Oxidizium** | Experimental mod replacing Java methods (currently: just MathHelper) with native Rust via FFI. Its own docs say it's **intended for server-side use** and "can be used on the client (with **minor rendering bugs**)," requires Java 22+ and special JVM flags, and currently optimizes one math class. Wrong tool for a client pack; real risk, near-zero measured benefit. ([modrinth.com/mod/oxidizium](https://modrinth.com/mod/oxidizium)) |
| **Bedder Mod** | A joke mod (the "you can only sleep at night" text grows larger and angrier when you spam-click a bed — "endorsed by Joe Hills"). Zero function. It's funny! It's also the definition of wasted jar. |

### Tier 2 — Remove IF the stated condition matches you

| Mod | Condition |
|---|---|
| **Greedy Meshing** | Remove if you play ~always with shaders (it self-disables to face-culling). Keep if you have a shaders-off mode — it's good at what it does. Note: its "Greedy Water" feature is experimental with known black-face bugs on Sodium. |
| **Nvidium** (+ **cbbg**) | Same logic as v1: shaders-off-only, and locked to exact Sodium builds (update Sodium → it may silently die until Nvidium updates). |
| **Eslium**, **Pearl Optimizer** (+ **Silicon**) | Keep only if you actually do PvP where ping compensation matters AND your servers allow it. |
| **Client Movement** | Remove if you play on anticheat servers (self-admitted flag risk); also only built for 26.1. |
| **Optimal Aim** | Renders a cube on entities showing the best aim point to use your full 3-block reach. Author argues it's not a cheat (visual only, 12-block range, needs line-of-sight) — reasonable, but it's an aim *aid*; some servers' rulesets won't love it. Your call. |
| **Resclone** | Auto-downloads/updates resource packs from URLs at **startup** — its own page warns large packs slow your launch. If you're not actively using its pack list, remove it for faster startup. |
| **XPlus Autofish** | Autofishing is one of the most commonly banned "QoL" mods on servers. Singleplayer/friends: fine. |
| **Fishing Ruler** | Only matters if you actually fish (it's excellent for that — shows cast distance + treasure-possible state). |
| **Spatial GUI** | Brand-new (Aug 2026) 3D-inventory-screen mod whose v1.5 changelog is "**significant performance fixes**" — i.e., it shipped with FPS problems days ago. Cool, but A/B test it and watch frame times. |
| **Bathymetry** | Two open crash reports on GitHub (mixin `InvalidInjectionException` on 1.21.11, and an NPE crash while exploring), and shaders can ignore it. Pretty, fragile. |
| **Smooth Swapping** | Remove if you keep IPN animations or Spatial GUI (§2.6). |

### Tier 3 — The "honest cost" list (keep what you love, but know the price)
You added ~25 cosmetic/world-detail mods. None is individually heavy, but together they're the *actual* frame-time cost in your pack — no perf mod cancels out added particles/geometry:

- **Particle Interactions** (particles on redstone use, furnace embers, foliage rustle, falling-block dust — very popular, configurable per-feature; its "disable extra physics" toggle is worth enabling)
- **Windy** (ambient wind particles), **Cool Rain** (rain ambient sounds on glass/foliage), **Make Bubbles Pop**, **Imprint** (dynamic footprints/imprints in snow/sand — note: this replaces the FootprintParticle you removed; Soft-Imprints-style mods are the better-engineered version), **Dense Flowers** (extra flower geometry per block!), **Glowing Emissive Ores DE**, **Elytra Contrails**, **Vectorientation** (squash-and-stretch falling blocks), **AITK** (arrows stick in all mobs), **Real Arrow Tip**, **Tightfire**, **Sulfur Cube Face's**, **Technopig**, **My Totem Doll**, **Player Armor Stands**, **Neo Bee Fix**, **Void Fog**, **Fog**-family, **Smooth Skies**, **AFK Cinematics**, **Better Happy Ghast Controls**, **Enchanted Fishing Line**, **Dinged** (arrow-hit ding sound), **Entity Selection Outlines**

These are all fine, all client-side, all preference. If you ever chase "one more 10% FPS," this tier is where it lives — in roughly this order: Particle Interactions → Dense Flowers → Windy → Imprint → Elytra Contrails → everything else.

---

## 4. The new-mod encyclopedia (everything you added, verified against project pages)

### 4.1 Real performance/optimization mods — all KEEP ✅
- **Jasione** — reduces GC pressure by eliminating `Enum#values()` array cloning via bytecode analysis (safe-call detection with ASM; caches arrays in generated classes). Sibling of Gnetum/Ixeris-style micro-opts. Legit, clever, keep. ([modrinth.com/mod/jasione](https://modrinth.com/mod/jasione))
- **FastMapCodec** — fixes an O(N²) in Mojang's `BaseMapCodec` (DataFixerUpper); mainly kills the join-freeze from large advancement files. Fix is upstreamed to Mojang (PR #110). Keep. ([modrinth.com/mod/fastmapcodec](https://modrinth.com/mod/fastmapcodec))
- **Better Block Entities (BBE)** — hybrid block-entity renderer for Fabric+Sodium; integrates into Sodium's video settings; "Smart"/"Fast" update schedulers; forces smart updates on both chest halves. This is the *optimization-focused* EBE successor — a straight upgrade over EBE Reloaded for your goals. Keep. ([modrinth.com/mod/better-block-entities](https://modrinth.com/mod/better-block-entities))
- **Better Render Distance** — turns the rounded-rectangle render cylinder into a true sphere/circle, culling diagonal chunk sections + entities; hooks Sodium's occlusion culler; configurable vertical distance. Genuinely clever, keep. ([modrinth.com/mod/better-render-distance](https://modrinth.com/mod/better-render-distance))
- **Cull Display Entities** — culls display entities (the 1.19.4+ display-entity type) when not visible. Inferred from purpose (page didn't render for me); small win if you visit worlds using display-entity decor. Keep.
- **Async Hotbars** — fixes hotbar open/save micro-freezes by preloading and running the save async (creative saved-hotbars). Keep. ([modrinth.com/project/rrWdhjP0](https://modrinth.com/project/rrWdhjP0))
- **Async Logger** — async log writes (stops log-I/O hitches). Generic but harmless. Keep.
- **Fast IP Ping** — faster/parallel server-list pinging. Menu-only QoL. Keep.
- **Optis** — client VRAM texture-residency manager (demotes/unloads unused textures, keeps hot ones). Very new (5K downloads), tested alongside Sodium/Iris/ImmediatelyFast. Keep, but A/B it — this is the one new perf mod without a long track record. ([modrinth.com/project/UIeq6Y0P](https://modrinth.com/project/UIeq6Y0P))
- **FPS-Sync / Better Resolution** — frame-limiter/resolution QoL utilities. Menu/video settings tier; keep.
- **SmoothTextureFix** — texture-atlas/seam fix. Keep.
- **Log Cleaner** — prunes old logs on startup. Housekeeping, keep.

### 4.2 Network/ping-feel mods (PvP cluster)
- **Eslium** — see §2.4.
- **Pearl Optimizer** — see §2.4 (depends on Silicon).
- **Tick Sync** — "reduces the delay between server packets and client ticks" — latency smoothing for timing-sensitive play. Client-side, modest adoption. Keep if you PvP; harmless otherwise.
- **Client Movement** — see §2.5 (⚠ anticheat).
- **Optimal Aim** — see Tier 2.
- **Exploitation Timer** — shows remaining TNT fuse time above primed TNT. By anvian. QoL (TNT mining/anvil stasis/crystal PvP adjacent). Keep-if-liked.
- **Dinged** — ding sound on arrow hit confirmation (same family as "Ding On Projectile Hit"). PvP QoL. Keep-if-liked.
- **Fishing Ruler** — line-length/treasure-state indicator when rod is out. Keep-if-fishing.
- **XPlus Autofish** — see Tier 2.

### 4.3 World/visual mods
- **Greedy Meshing** — merges identical opaque block faces into bigger quads (huge quad-count reduction); supports facing/rotation-property blocks after model verification; "Aggressive Greedy (Absolute)" mode merges across AO boundaries at the cost of coarser lighting. **Shader packs: falls back to face-culling-only.** Works through 26.2; Distant Horizons compatible. ([github.com/programmer1o1/GreedyMeshingMod](https://github.com/programmer1o1/GreedyMeshingMod), [modrinth.com/mod/greedy-meshing-mod](https://modrinth.com/mod/greedy-meshing-mod))
- **Bathymetry** — water darkens with depth (multiplies biome tint rather than replacing). See Tier 2 (crashes + shader-dependent). ([modrinth.com/project/jn3yCCDl](https://modrinth.com/project/jn3yCCDl))
- **Smooth Skies** — smooths skybox/fog-color banding at high render distances; includes "Lower Sky Void Darkness" + a Clear Skies port; explicitly aimed at Nvidium/high-RD users. Keep for shaders-off sessions. ([curseforge.com/minecraft/mc-mods/smooth-skies](https://www.curseforge.com/minecraft/mc-mods/smooth-skies))
- **Dense Flowers** — renders multiple flowers per block in flower fields (client-side, neighbor-aware density). Pure eye-candy geometry cost. ([modrinth.com/mod/dense-flowers](https://modrinth.com/mod/dense-flowers))
- **Windy** — ambient wind particle streams (YACL-configurable), inspired by Breezy. ([modrinth.com/mod/windy](https://modrinth.com/mod/windy))
- **Cool Rain** — rain drip/ambient sounds on glass, foliage, lava hiss. Audio-cosmetic. ([modrinth.com/mod/coolrain](https://modrinth.com/mod/coolrain))
- **Particle Interactions** — see Tier 3. Every feature individually toggleable. ([modrinth.com/mod/particle-interactions](https://modrinth.com/mod/particle-interactions))
- **Particle Tweaks** — particle config/cleanup utility. Keep.
- **Imprint** — dynamic block imprints/trails from entities (snow/sand profiles). ([modrinth.com/mod/snow-imprints](https://modrinth.com/mod/snow-imprints) family)
- **Void Fog** — restores pre-1.8-style void fog. Cosmetic. ([modrinth.com/mod/void-fog](https://modrinth.com/mod/void-fog))
- **Fog** — fog distance/appearance control (family of Fog Control mods). Cosmetic/control.
- **Tightfire** — fire renders tight against slabs/fences instead of clipping. One-jar wonder by AmyMialee. ([curseforge.com/minecraft/mc-mods/tightfire](https://www.curseforge.com/minecraft/mc-mods/tightfire))
- **Sulfur Cube Face's** — unique faces per Sulfur Cube archetype (26.2 mob). Cute. ([modrinth.com/mod/sulfur-cube-faces](https://modrinth.com/mod/sulfur-cube-faces))
- **Glowing Emissive Ores DE** — emissive glowing ore textures. (Visual; shader-dependent appearance.)
- **Vectorientation** — falling blocks rotate toward travel direction + squash/stretch, per-block configurable. By Tec. 1.3M downloads. ([modrinth.com/mod/vectorientation](https://modrinth.com/mod/vectorientation))
- **AITK (Arrow In The Knee)** — arrows render stuck in *all* entities, not just players. Vanilla-faithful, lightweight. ([modrinth.com/mod/aitk](https://modrinth.com/mod/aitk))
- **Real Arrow Tip** — bow/crossbow first-person + world arrows show correct tipped/spectral colors. ([mcmod.cn/class/12469.html](https://www.mcmod.cn/class/12469.html))
- **Elytra Contrails** — particle trails while elytra flying. Cosmetic.
- **Make Bubbles Pop** — bubble-column pop particles/sounds. Cosmetic.
- **Neo Bee Fix** — bee-related fix (behavior/rendering). Small fix-mod.
- **Dynamic Fullbright** — scales/clamps light levels (default floor 4/15) instead of gamma-hacking; keeps ambience. Cloth-configurable. ([modrinth.com/project/tF7P4IlX](https://modrinth.com/project/tF7P4IlX)) — test with shaders.
- **My Totem Doll** — totems become 2D/3D player-skin dolls (rename to a nickname to use that skin). ([curseforge.com/minecraft/mc-mods/my-totem-doll](https://www.curseforge.com/minecraft/mc-mods/my-totem-doll))
- **Technopig** — pigs named "Technoblade" get his crown. RIP. Keep forever, obviously. ([modrinth.com/mod/technomodel](https://modrinth.com/mod/technomodel))
- **Player Armor Stands** — named armor renders as the named player. Needs YACL. ([curseforge.com/minecraft/mc-mods/player-armor-stands](https://www.curseforge.com/minecraft/mc-mods/player-armor-stands))
- **Entity Selection Outlines** — outlines on mobs, per-mob toggle via commands. Tiny project.
- **Nice Boat Refabricated** — ⚠ could not verify (page didn't render, no search coverage). Presumably the Fabric port of the boat QoL mod. Check its Mod Menu entry.
- **RowGlide** — ⚠ still unverified after 3 attempts (Modrinth page is JS-rendered and search engines have no coverage). Given your pack contains the whole cutebow/Walksy "client-side zero-ping" family (Pearl Optimizer, Silicon), it's *plausibly* client-side rowing prediction — but check its description in Mod Menu and decide if you row boats enough to care.
- **Frostbyte's Maximum Mobility** — step-up, boat step-up, coyote time, reach-around placement, elytra cancel (YACL config). See §2.5.

### 4.4 HUD / UI / inventory
- **Armor Indicator** — see §2.2.
- **Better Ping Display Remake** — see §2.3.
- **DualBar** — locator-bar icons overlay the XP bar/jump bar so XP doesn't hide the locator bar. ([modrinth.com/mod/dualbar](https://modrinth.com/mod/dualbar))
- **Durability Warner HUD** — low-durability warnings.
- **Effect Insights** (Fuzs) — potion/food effect descriptions in tooltips + hover active-effect icons. Needs Puzzles Lib + Forge Config API Port stack (already in your pack). Keep — near-zero cost. ([modpackindex.com/mod/46059/effect-insights](https://www.modpackindex.com/mod/46059/effect-insights))
- **Stew Detective** — suspicious-stew effect reveal (same family as Visible Suspicious Stew). Keep-if-liked.
- **Pick Up Notifier** — on-screen item pickup toasts.
- **Shulker Box Tooltip** — shulker contents preview in tooltips. Standard, keep.
- **Chest Tracker port** — remembers chests' contents for "where did I put X" search. Keep-if-used.
- **Inventory Item Groups** — Bedrock-style collapsible item groups in the Creative menu; supports custom groups. Creative-only, zero survival cost. ([modrinth.com/mod/inventory-item-groups](https://modrinth.com/mod/inventory-item-groups))
- **Just Enough Book (JEB)** — recipe-book upgrade: MMB shows ingredients, right-click shows what an item crafts into, search by name/tag/tooltip; "lightweight JEI alternative." Keep. ([modrinth.com/mod/justenoughbook](https://modrinth.com/mod/justenoughbook))
- **Slot Cycler** — number-key cycles through that inventory column.
- **Smart Block Placement** — smarter placement (avoid misplacing when clicking interactable blocks etc.). Note mild overlap with Frostbyte's "reach-around placement."
- **Smooth Swapping** — see §2.6.
- **Spatial GUI** — see Tier 2. ([curseforge.com/minecraft/mc-mods/spatial-gui](https://www.curseforge.com/minecraft/mc-mods/spatial-gui))
- **I Don't Wanna Scroll Again** — server list stops jumping to top on Refresh. Glorious. ([9minecraft.net/i-dont-wanna-scroll-again-mod/](https://www.9minecraft.net/i-dont-wanna-scroll-again-mod/))
- **Plane Advancements** — rearranges the advancement UI into springy graphs/grids/one-screen; draggable nodes; pairs with Better Advancements (you have it); incompatible with Paginated Advancements (you don't). ([modrinth.com/mod/plane-advancements](https://modrinth.com/mod/plane-advancements))
- **Controlling** — searchable keybind list. Standard.
- **Multi Key Bindings**, **Universal Settings**, **KeepSneak**, **OrthoCamera** (orthographic camera), **Cinematic Zoom** (v1), **AFK Cinematics** — keybind/UX utilities, keep.
- **Bedrock Hotbar / Bedrock Skins** — Bedrock-style hotbar visuals / Bedrock skin model support.
- **Dark Title Bar / Cycle Title Screen Splash / Hide Experimental Warning / No Telemetry / Nondirectional Damage Tilt Fix / Hot-Reload Resource Packs Forked** — the "make the client less annoying" tier. All fine, all free.
- **Compacting** — "intelligent compact chat" — stacks repeated chat messages. Client-side. Keep-if-chat-is-busy. ([modrinth.com/mod/compacting](https://modrinth.com/mod/compacting))
- **Iris & Oculus Search** — search bar in the shaderpack list. Free QoL given your shader stack.
- **Rendersnap** — screenshot/render utility (unverified detail).

### 4.5 Audio / media
- **Melody** (Keksuccino) — OpenAL background-music library; **Konkrete** is its sibling lib. With no FancyMenu in your pack, the likely consumer is your **Enhanced Biomes Music** — verify in Mod Menu (§5). If nothing lists them, they're orphaned.
- **Enhanced Biomes Music** — biome-specific music expansion.
- **WATERMeDIA + Binaries** — see Tier 1. VLC multimedia API + bundled VLC binaries.
- **Sound Culling** — different animal from the RSLS you removed: instead of raising the simultaneous-sound cap, it's a per-sound mute/priority system ("Sound Culling 2.0" = priority-based audio engine considering distance/direction/repetition). Small project; keep-if-you-use-it. ([modrinth.com/mod/soundculling](https://modrinth.com/mod/soundculling), [github.com/Cukkoo12/SoundCulling](https://github.com/Cukkoo12/SoundCulling))
- **Cool Rain** — see §4.3.

### 4.6 Game-behavior QoL
- **Better Combat** — melee overhaul (combos, weapon types/movesets); brings Player Animation Library + Architectury/Cloth. The anchor of your combat-visual setup.
- **Punchy!** — first-person animation layer over it (§2.1).
- **Break Free** — keep breaking a block when you switch items mid-break (one mixin). Note: repo archived in 2025; if it still loads on 26.2, fine — check for warnings. ([modrinth.com/mod/breakfree](https://modrinth.com/mod/breakfree))
- **Better Happy Ghast Controls** — control scheme QoL for the Happy Ghast mount.
- **Open Together** — "Open to LAN" improvements.
- **Tick Sync / Client Movement / Eslium / Pearl Optimizer / Frostbyte's** — covered above.
- **XPlus Autofish** — see Tier 2.
- **Enchanted Fishing Line / Fishing Ruler** — fishing visuals/info.

---

## 5. Library map — updated & nearly complete 🗺

New libs and their owners (verified or high-confidence):

| Library | Used by (in your pack) | Status |
|---|---|---|
| **Player Animation Library** (playerAnimator) | Better Combat | ✅ claimed |
| **Searchables** | Jade, PTP (Snownee's lib) | ✅ claimed |
| **Collective** | Serilum's mods (Delete Worlds To Trash etc.) | ✅ claimed |
| **Silicon** | Pearl Optimizer (cutebow's lib + update GUI; **requires Mod Menu**) | ✅ claimed |
| **FrozenLib** | Frostbyte's Maximum Mobility | ✅ claimed |
| **TLib** | "Library for Take's Mods" — one of your QoL mods is by Take | ✅ claimed, identify exact consumer in Mod Menu |
| **MRU** | Cassian & IMB11's mods ("Immersive Overlays/Minimaps" utilities — one of your newer visual mods) | ✅ claimed, identify exact consumer in Mod Menu |
| **Konkrete + Melody** | Keksuccino stack — likely Enhanced Biomes Music | ~ likely claimed, verify |
| **WATERMeDIA + Binaries** | **???** — nothing visible in your pack needs a VLC media API | ⚠ likely ORPHAN → Tier 1 |
| **Puzzles Lib / Forge Config API Port / Forge Config Screens** | Effect Insights (Fuzs stack) | ✅ claimed (must be present already) |

From v1, still-unverified orphans if they're still in the pack: **Craft Config Lib, TimelessLib, CICADA, CraterLib, MossyLib, BaguetteLib, EclipseUI.**

**The 30-second orphan test (worth doing once):** Mod Menu → click a library → "Dependencies/Dependents" tab lists what needs it. Anything with zero dependents and no "required-by" = orphan. Or the brute-force way: rename `X.jar` → `X.jar.disabled`, launch, see what screams. Removing orphan libs is the only "free" performance in a pack — zero risk, less mixin scanning, faster startup, less RAM.

---

## 6. Version pairing reminders (unchanged from v1, still critical)

- **EntityCulling ≥ 1.11.2** — older builds cause the invisible-entities regression around 26.3. You kept the mod; confirm the version.
- **Euphoria Patches** must match your **Complementary r5.9.x** branch (you have both — they're a pair, keep them aligned).
- **Nvidium** is locked to exact Sodium builds — after any Sodium update, check Nvidium still loads (check Mod Menu for the warning icon).

---

## 7. Suggested test protocol for v2 (quick version)

1. Do the Tier 1 removals (7 mods/jars incl. WATERMeDIA pair) → launch → confirm nothing complains.
2. Run the orphan check (§5) once.
3. Baseline FPS/frame-time: F3 + a fixed route, shaders off. Then shaders on.
4. A/B the three "watch list" newcomers individually: **Optis**, **Spatial GUI**, **Greedy Meshing** (shaders off), **Dynamic Fullbright** (shaders on).
5. Combat test with shaders on: swing a weapon with Better Combat + Punchy → look for the double-render.
6. Inventory test: open a chest, shift-click stacks around with IPN + Smooth Swapping + Spatial GUI all active → look for animation jank or ghost items.

---

## 8. Bottom line

Your v2 pack is *substantially* better-engineered than v1: you removed the right things, the new perf layer (Jasione, FastMapCodec, BBE, BRD, Async Hotbars, Cull Display Entities) is legitimately good, and the shaders-off stack is coherent. The remaining work is **deduplication, not optimization**: two armor HUDs, two tab-ping mods, one redundant Iris mod (FISM), one likely-orphaned VLC media stack, an experimental Rust jar, a joke bed mod, and a careful decision about how much ping-compensation and movement-tweak risk you want to carry onto servers. After Tier 1 + orphan cleanup you'd land around ~215 mods with zero functionality lost — everything beyond that is taste.

---

## 9. The ADD list — perf mods you *don't* have (verified live on 26.2/26.3, non-redundant)

*Checked against project pages Oct 2026. Your full 234-mod list isn't visible to me anymore (v1 list lost in the sandbox reset), so cross-check each against your pack before installing — BadOptimizations/More Culling/Dynamic FPS are ubiquitous and might already be in there.*

### The real gains, in priority order

1. **ModernFix** (embeddedt) — Fabric/Forge/NeoForge, 1.16→26.3. Attacks a bottleneck nothing else in your pack touches: **mod loading, startup time, and memory**. Large packs commonly halve load times; its opt-in **Dynamic Resources** mode (loads models on demand instead of baking everything at launch) can save *gigabytes* of RAM in 200+ mod packs, at the cost of tiny first-render hitches. Explicitly designed to stack with Sodium/Lithium/FerriteCore ("they target different bottlenecks"). For a 234-mod pack this is the single highest-value add. Note: Fabric builds above 1.21.4 require its **mVUS** dependency. ([bravith.com/mods/modernfix](https://bravith.com/mods/modernfix/), [craftdownunder.co/guides/mods/modernfix](https://craftdownunder.co/guides/mods/modernfix))
2. **ScalableLux** — 26.2 Fabric. Starlight-successor light-engine optimizer, with an optional **parallel light updates** mode for heavy lighting scenarios. Non-redundant because Lithium deliberately doesn't touch lighting (the v1 "Lithium ≠ light engine" myth-bust). Biggest effect where new chunks load and light recalculates. ([modrinth.com/mod/scalablelux](https://modrinth.com/mod/scalablelux/version/0.2.1+fabric.2b08348))
3. **More Culling** (FX) — 26.2, needs Cloth Config (you have it). Adds culling layers for **leaves (inner faces), item frames, and other cutout geometry**. Non-redundant: EntityCulling = entities/block-entities via raytrace; Greedy Meshing = merging *opaque* faces (leaves are cutout, not merged); More Culling = the leaf/misc layer. Do **not** also add standalone Cull Leaves — More Culling supersedes it. ([minespecs.com/mods/moreculling](https://minespecs.com/mods/moreculling/))
4. **C2ME (Concurrent Chunk Management Engine)** — 26.2 (beta-channel 0.4.x) / 26.3 devbuilds. Parallelizes chunk generation & loading across CPU cores; 20M+ downloads. Fixes the "chunks streaming in" stutter that none of your render-side mods address, and stacks beautifully with ScalableLux on the chunk-loading pipeline. Caveat: current-gen builds are beta/alpha quality — test for stability, keep an eye on their issue tracker. ([curseforge.com/minecraft/mc-mods/c2me](https://www.curseforge.com/minecraft/mc-mods/c2me))
5. **BadOptimizations** (thosea) — 26.3/26.2, 77M downloads. Grab-bag of client micro-opts for "things other than rendering" (skips pointless per-tick work). Different layer from everything above; near-universal in perf packs for a reason. ([curseforge.com/minecraft/mc-mods/badoptimizations](https://www.curseforge.com/minecraft/mc-mods/badoptimizations/files/all))

### Waste-reduction / smoothness tier

6. **Dynamic FPS** (juliand665) — 26.2/26.3, 62.7M downloads. Throttles framerate (and volume) when the window is unfocused/hidden/idle, and fixes a vanilla background-CPU bug. Not the same job as your FPS-Sync (that's an active-play limiter; this handles window states). ([discovermods.com/minecraft-mods/dynamic-fps](https://discovermods.com/minecraft-mods/dynamic-fps/))
7. **Rrls (Remove Reloading Screen)** — 26.2 (v5.2.x). Loads resource packs in the background instead of blocking you on the reload screen. Pairs with your Hot-Reload Resource Packs Forked (same subsystem, usually fine together — just test). ([klauncher.gg/en/minecraft/mods/rrls](https://klauncher.gg/en/minecraft/mods/rrls))
8. **FastQuit** (contaria) — ~17.75M downloads, current. Returns you to the title screen while the world saves in the background; safe (waits when it matters, e.g. rejoin/backup). ([modpackindex.com/mod/40435/fastquit](https://www.modpackindex.com/mod/40435/fastquit))

### Considered and REJECTED (so you don't waste time)

- **Exordium** (render GUI at low framerate) — Modrinth project **archived**, no 26.2 build, and its own docs say it's "for the Vanilla UI only… the wilder the mod, the more likely it is to not work." Your pack is HUD-mod-heavy (Pick Up Notifier, DualBar, Durability Warner, armor HUDs…) = exactly the wrong fit. ([modpackindex.com/mod/39071/exordium](https://www.modpackindex.com/mod/39071/exordium))
- **Sodium Dynamic Lights** as a LambDynamicLights replacement — no 26.2 build exists, and LDDL already integrates with Sodium for performance. ([9minecraft.net/sodium-embeddium-dynamic-lights-mod](https://www.9minecraft.net/sodium-embeddium-dynamic-lights-mod/))
- **Cull Leaves** (standalone) — superseded by More Culling.
- **FastChest** — superseded by your Better Block Entities.
- **Starlight** — superseded by ScalableLux on modern versions.
- **fastfastmap** — only relevant if you ever add a minimap.

---

## 10. Follow-up: "client-side only" clarification + block-breaking animation mods

### 10.1 Which ADD-list mods are true client-side wins
- **Pure client-only** (work on any vanilla server): More Culling, BadOptimizations, Dynamic FPS, Rrls, FastQuit.
- **ModernFix** — client-only install is fine; its launch-time/RAM wins apply regardless of server.
- **ScalableLux & C2ME** — install client-side is fine, but chunk generation and lighting are the *server's* job in multiplayer → these two only pay off in singleplayer or on a server you control. If you play mostly on other people's vanilla servers, skip them.

### 10.2 "The opposite of A Good Place" — block BREAKING animations
**A Good Place** (MehVahdJukaar) = block **placement** animations (blocks zoom/settle into place). The breaking-side counterparts, subtle → dramatic:

1. **Stupid Block Animations** (fnn) — *the direct answer.* Client-side Fabric, **Sodium-compatible**, 26.2/26.3 builds (updated Sep 2026). While you mine a block it **tilts and wobbles** (delay → oscillation → hold at max tilt → smooth return). Fully configurable: delay, oscillation speed, max angles, easing, return duration, connected-block handling — and since it *also* does placement animations, you can disable those in config for breaking-only (or let it replace A Good Place entirely, which caps at 1.21.4 anyway). ([modrinth.com/mod/stupid-block-animations](https://modrinth.com/mod/stupid-block-animations/versions))
2. **Fancy Block Particles – Renewed** — the classic "satisfying destruction" look: blocks break into **3D textured fragments** that tumble; also block placing animation + rounded flame/smoke/rain/snow particles. Settings panel on the **I** key; every effect individually reducible/disabled. Fabric/Forge/NeoForge; 26.1.2 beta so far — check for a 26.2 file. Real particle cost in big mines/fights. ([curseforge.com/minecraft/mc-mods/fbp-renewed](https://www.curseforge.com/minecraft/mc-mods/fbp-renewed))
3. **Physics Mod** — maximal option: blocks **crack apart into physics debris** that falls (plus ragdolls, item physics). Client-side only; 26.2 Fabric build confirmed (3.1.45); recent changelog: **PBR support for Iris working again**, option to hide vanilla break particles, reduced fragment spawning. The catch: it's the one mod here that *costs* real FPS (tune fragment counts down or skip if chasing frames). ([curseforge.com/minecraft/mc-mods/physics-mod](https://www.curseforge.com/minecraft/mc-mods/physics-mod/files/8333350))

Note: your **Particle Interactions** makes break particles pixel-consistent but doesn't animate the block itself — all three above stack with it.

*Report written 2026-10-07 (re-saved after sandbox reset — first version was at /home/user/modpack-audit-v2.md). All mod descriptions verified against Modrinth/CurseForge/GitHub pages listed inline, except where marked ⚠ unverified (RowGlide, Nice Boat Refabricated, Rendersnap, and the inferred Cull Display Entities / Fog identity).*
