# Current state

- Last updated: 2026-10-07
- Session branch: `arena/c83497e6-tests`
- Read when: starting any task in this repository.

## Active work

### Spatial HUD captured-mesh renderer

- Status: recovery rollback compiled and published; F5W runtime approval is pending.
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
- `bb99cda` passed CI but its newer projective/PiP path regressed the actual
  F5W renderer. Do not use it as a runtime baseline.
- `5c245d5` restores `ExperimentalHudCapture.java` and its two mixin
  registrations exactly from user-observed working camera-yaw revision
  `46874e5`; it removes the PiP-map accessors introduced by `bb99cda`.
- Its `VirtualHudPlane` retains camera-yaw anchoring but removes every body-yaw
  and anchor-mode branch. It uses the earlier fixed physical plane: face-on at
  30° down and projectively trapezoidal at grazing angles.
- H opens settings; a separate unbound Toggle Spatial HUD action remains in
  Minecraft Controls. No real F5W result has confirmed the recovery yet.
- GitHub Actions run `37706894548` compiled and published `5c245d5` with JDK
  25; the rolling JAR was updated at 2026-10-08 00:17 UTC. A local Gradle build
  is unavailable because this sandbox has no Java runtime; static
  JSON/reference/whitespace contracts passed locally.

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

1. Install the rolling JAR updated at 2026-10-08 00:17 UTC in F5W. H must
   open settings without toggling; Camera Yaw is the only active plane pose.
2. Enable experimental capture and test the full F5W bottom HUD at 30° down
   and at a higher/grazing view.
3. Confirm the backing and hotbar form one trapezoid without flicker or body
   yaw rotation. Then record hotbar slots/items, status bars, AppleSkin, Detail
   Armor, XP, and held-item text separately; check the log for a hotbar-root
   capture message and any capture failure latch.

## Evidence

- Supplied F5W log: `latest.log`.
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
