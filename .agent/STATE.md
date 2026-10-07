# Current state

- Last updated: 2026-10-07
- Session branch: `arena/c83497e6-tests`
- Read when: starting any task in this repository.

## Active work

### Spatial HUD captured-mesh renderer

- Status: compiled by CI; runtime approval is pending.
- Goal: render one selected lower-HUD capture through a shared projective mesh.
  The backing, hotbar, status bars, icons, text, and compatible injected HUD
  pixels must deform together.
- The experimental captured-mesh mode is opt-in and remains off by default.
  The safe mode is affine only and cannot provide pixel-level deformation.
- Latest built code changes:
  - `1a89847`: composites the private capture after Minecraft's normal GUI
    pass, addressing output being overwritten or rendered with an unstable
    GUI state.
  - `931b6de`: added a first look-pitch deformation attempt.
- `bb99cda`: replaced overlapping legacy pose paths with one camera-yaw,
  pitch-driven projection: rectangular at 30° down, tapered above or below
  that angle, and guarded only against total off-screen loss.
- The private `GuiRenderer` now shares the main renderer's existing
  picture-in-picture map after private construction. 26.3 GUI item rendering
  uses that path; this avoids an empty map dropping hotbar item content while
  avoiding duplicate constructor-time registration by third-party mixins.
- H opens settings; a separate unbound Toggle Spatial HUD action appears in
  Minecraft Controls. No real F5W result has confirmed this refactor yet.
- GitHub Actions run `37705190664` compiled and published `bb99cda` with JDK
  25. A local Gradle build was unavailable because this sandbox has no Java
  runtime; static JSON/reference/whitespace contracts also passed locally.

### Direct Modrinth updater

- Status: implemented; not the active task.
- Target defaults to `%APPDATA%\ModrinthApp\profiles\F5W\mods` and is
  remembered. Folder opening is optional and never forced to Windows Explorer.
- The eventual branch picker must offer `main` plus five recent successfully
  built branches, each with its own compiled JAR.

## Non-negotiable constraints

- Do not restore global GUI cancellation, whole-screen offscreen capture, or
  a general GUI mixin. Only the selected lower HUD may be captured.
- Do not claim F5W, Iris, AppleSkin, or Detail Armor compatibility without a
  real test result.
- AppleSkin and Detail Armor Bar Reconstructed pixels belong in the same
  selected capture when their vanilla status root is captured.
- User wants a waist-height hologram that follows camera yaw, does not turn
  with body yaw, and rotates its mesh with look pitch. At 30° down it is
  rectangular; looking higher makes the far top edge horizontally narrower.
- Keep all work on `arena/c83497e6-tests`; commit and push each completed
  change.

## Next action

1. Install the rolling JAR updated at 2026-10-07 23:58 UTC in F5W.
2. In F5W, bind Toggle Spatial HUD from Minecraft Controls if desired; H must
   open settings without toggling.
3. Enable experimental capture and test the full F5W bottom HUD at 30° down
   and at a higher/grazing view.
4. Confirm hotbar slots/items, status bars, AppleSkin, Detail Armor, XP, and
   held-item text enter one captured trapezoid without flicker or loss. Check
   the log for the hotbar-root capture message and any capture/projection
   failure latch.

## Evidence

- Supplied F5W log: `latest.log`.
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
