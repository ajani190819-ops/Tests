# Spatial HUD F5W: source-aware verification and retained scope

**HUD-01 remains OPEN.** The saved request is Classic Affine, Captured Mesh and
World-Space Texture three-method acceptance. The current source cannot execute
that checklist: it only activates the **Purple Horizontal Panel**. This runbook
corrects the handoff; it does not restore methods or redefine the owner's request.

## Stop the version/method mismatch first

Current source 1.0.5 / Minecraft 26.3 / Java 25 has enum identities
`WORLD_SPACE_TEXTURE` and `POLYGON_TEST`, but `selectedRenderMethod()` always
returns `POLYGON_TEST`; the picker is excluded, Method 3 is shelved and the old
Classic Affine/Captured Mesh choices are absent. Current panel smoke results
must be recorded separately from the **unfulfilled original three-method test**.
If a tested JAR shows those old choices, identify that different build; do not
call its results current-source acceptance. No hidden JSON edits to force old
methods. Scope reconciliation with the owner is required before restoring or
replacing the retained three-method requirement.

## Identify a real built artifact

1. Close Minecraft. Back up the profile config/JAR privately before changes.
2. Use a **successfully built** branch-scoped
   `spatial-hud-build-<branch-with-slashes-replaced>` release for the active
   branch, recording build commit, asset timestamp, version and JAR SHA-256.
   Do not assume the shared `spatial-hud-latest` fallback matches that branch.
   If no correct asset exists, stop and record the build/delivery gap.
3. Replace only the old Spatial HUD JAR; avoid duplicate versions in `mods`.
   Preserve the normal F5W renderer/HUD stack. Do not disable Iris or companions
   just to obtain a green result; label any later isolation run separately.
4. Record Minecraft/Fabric/Cloth Config/Iris and companion versions. Retain
   same-run `latest.log`; startup should reach `Spatial HUD initialized. Press H
   for the settings and guide.` A shader reload/capture error is a failure to
   investigate, not an automatically passing fallback.

## Current Purple Horizontal Panel smoke matrix (not three-method acceptance)

Record the original values before changing a test setting. Test a known safe
world and return any deliberately altered setting afterward.

| Check | Evidence to record |
| --- | --- |
| Settings | H opens Guide, Setup, Presets, Panel, Wiggle and HUD Contents without toggling enabled state. Enabled/config values survive normal restart. |
| Presets | Waist is feet/body anchored; Face stays square to the camera; Custom 1/2 respect their recorded anchor/distance/height/width. Only the active preset's width should change. |
| Placement/motion | Look/turn/walk, first/third person and any shoulder camera. Observe curvature, angle, heading and wiggle response. No fixed 30-degree cutoff is promised by current source. |
| Occlusion | First person: blocks hide the captured image **and** its fill/border with Occlusion on, including while Body Exclusion is on. Third person: record behavior with the body exception on/off. Through-world behavior with Occlusion off must be explicit. |
| Selected-HUD scope | Hotbar/status/XP/held-item pixels and selected companions stay together, without duplicate vanilla strips, missing slots, flicker, stale captures or unrelated GUI capture. |
| Companion HUDs | AppleSkin, Detail Armor Bar Reconstructed and actually installed same-region HUD/slot-cycling mods, with the normal Iris stack retained. Record what was really present. |
| GUI boundaries | With default Gameplay Only on, ordinary screens/chat/inventory/maps/config remain normal. Test Gameplay Only off only if intentionally evaluating it, recording that difference; unrelated screen content is never a capture target. |
| Lifecycle/performance | Resize/fullscreen/GUI scale, world leave/rejoin, normal resource reload and restart. Record capture errors, bounded wrapper-reapply warnings, sustained flicker, frame-time change or leaks. |
| Failure indication | If a capture fails, preserve logs/screenshots of vanilla/error indication. Do not mark it as a passing archived affine fallback or repeatedly restart to hide it. |

## Report and retained decision

Share a private reviewed summary containing build/JAR SHA, F5W/OS/renderer and
companion versions, relevant saved settings, action, expected/observed behavior,
screenshots/clip and same-run log. Logs can contain private names/server paths;
review before sharing, never commit raw owner logs to public Git/Drive.

- Current-panel smoke: **not run / pass / fail / blocked**, with evidence.
- Original Classic Affine/Captured Mesh/World-Space Texture acceptance:
  **OPEN — incompatible with current source; owner scope reconciliation needed**.
- Do not restore old methods or claim the original request complete based on
  source contracts, successful Gradle compilation or a purple-panel smoke pass.
