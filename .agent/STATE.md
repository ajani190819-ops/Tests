# Current state

- Last updated: 2026-10-07
- Session branch: `arena/c83497e6-tests`
- Read when: starting any task in this repository.

## Active work

### Spatial HUD captured-mesh renderer

- Status: built; runtime approval is pending.
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
- Latest pending build: fixed camera-yaw hologram pose. Default face-on angle
  is 30° below the horizon; it follows camera yaw but not camera pitch, so the
  plane remains horizon-parallel while pitch changes perspective. H opens
  settings instead of toggling the HUD.
- Experimental diagnostics now record whether the vanilla hotbar root reaches
  the private capture. No real F5W result has yet confirmed this revision.

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
- User wants a waist-height hologram that follows camera yaw, stays parallel
  to the horizon, and does not turn with body yaw. At 30° down it is
  rectangular; looking higher makes the far top edge horizontally narrower.
- Keep all work on `arena/c83497e6-tests`; commit and push each completed
  change.

## Next action

1. Install the next rolling Spatial HUD JAR in F5W.
2. Press H and confirm it opens settings without toggling the HUD.
3. Enable the experimental captured-texture mode and select `Camera Yaw
   (Hologram)`.
4. Test the 30° face-on view and a higher/grazing view. Record whether the
   whole capture changes from a rectangle to a trapezoid with a narrow far top
   edge while remaining directly in front after a yaw turn.
5. Record whether the hotbar disappears while the backing remains. Search the
   same-run log for the one-time hotbar-capture diagnostic before changing the
   capture or safe fallback.

## Evidence

- Supplied F5W log: `latest.log`.
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
