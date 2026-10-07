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
- Latest code changes:
  - `1a89847`: composites the private capture after Minecraft's normal GUI
    pass, addressing output being overwritten or rendered with an unstable
    GUI state.
  - `931b6de`: changes the virtual plane continuously with view pitch. At a
    grazing angle, the far edge narrows; at the configured face-on angle, the
    mesh is rectangular.
- CI builds passed for both commits. No real F5W result has yet confirmed the
  latest look-driven deformation.

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
- User wants physical perspective: head-on is rectangular; a grazing view is
  a trapezoid with the far edge horizontally narrower.
- Keep all work on `arena/c83497e6-tests`; commit and push each completed
  change.

## Next action

1. Install the latest rolling Spatial HUD JAR in F5W.
2. Enable the experimental captured-texture mode and leave look-driven tilt
   enabled.
3. Test a head-on view and a grazing/upward relative view.
4. Record whether the whole capture changes from a rectangle to a trapezoid,
   whether the far edge narrows, and whether flicker returns.
5. If it still renders as a rectangle or flickers, inspect the render pass
   rather than making the safe mode the default.

## Evidence

- Supplied F5W log: `latest.log`.
- Compatibility plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- Source root: `spatial-hud-template-26.3/`.
- Technical summary: `.agent/facts/spatial-hud.md`.
