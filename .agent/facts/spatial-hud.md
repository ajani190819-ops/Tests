# Spatial HUD facts

- Status: the experimental renderer was restored to its prior camera-yaw
  capture path; an F5W runtime result is pending.
- Last verified: 2026-10-08 CI build `37706894548` for recovery `5c245d5`;
  no rollback runtime result exists yet.
- Read when: changing Spatial HUD code, its updater, or F5W compatibility.

## Source and release

- Mod source: `spatial-hud-template-26.3/`.
- Rolling artifact: GitHub release `spatial-hud-latest`, asset
  `spatial-hud-1.0.0.jar`.
- GitHub workflow: `.github/workflows/build-spatial-hud.yml`.
- Source version and release asset name may remain `1.0.0`; use commit and
  asset timestamp when identifying a test build.

## Rendering design

- `VirtualHudPlane.java` is the shared physical-plane projection model. Its
  active camera-yaw pose follows camera yaw rather than body yaw, has a fixed
  horizon-space orientation, and is face-on at the configured 30° down look
  angle. Different camera pitch then produces the plane's real perspective.
- `ExperimentalHudCapture.java` captures only the selected lower HUD into a
  private target and renders it through a tessellated mesh. The current
  rollback intentionally restores the isolated `GuiRenderer(..., List.of())`
  implementation from `46874e5`; it does not share or mutate the main
  picture-in-picture renderer map. The map-sharing attempt is a suspect F5W
  regression and must not return without a separate, successful runtime test.
- `SpatialHudGameRendererMixin.java` composites the mesh after the normal
  `GuiRenderer` call.
- `SpatialHudGuiRendererMixin.java` redirects only the private captured
  renderer by object identity.
- The capture includes the backing plate and selected bottom-HUD pixels. It
  must not capture ordinary screens, chat, maps, menus, debug text, or an
  unrelated mod's GUI.
- The experimental mesh uses a 24 by 12 grid. Its projection comes from one
  virtual plane, so the backing and all captured pixels share the same corners
  and perspective.

## Perspective requirement

A head-on plane projects as a rectangle. When viewed at a grazing angle, its
far edge must project narrower than its near edge. This is projective geometry,
not independent affine scaling of icon groups.

The current recovery model is a fixed physical plane that is camera-yaw
anchored. At `virtualFaceOnLookDownPitch` (default 30°), it is face-on. Looking
higher makes the top/far edge narrower; looking lower changes perspective in
the opposite direction. `virtualPitch` is a manual physical-orientation offset.
`virtualHorizonPerspectivePitch` remains only as ignored old JSON data.

## Compatibility and fallback

- Experimental captured-mesh mode is default off.
- The normal mode is a safe affine fallback. It cannot bend pixels inside an
  icon or text glyph.
- First-class targets: AppleSkin and Detail Armor Bar Reconstructed.
- F5W also includes other same-region HUD mods, including Bedrock Hotbar,
  Immersive Hotbar, DualBar, Armor Indicator, Status Effect Bars, Mount
  Opacity, and Durability Warner HUD.
- The user reported that the newer experimental/PiP/refactor builds no longer
  work reliably, despite resolving a flicker. The recovery deliberately returns
  the selected-capture path to the camera-yaw implementation in `46874e5`,
  while removing all body-yaw rotation. Its F5W result is still required.
- The current capture path logs once whether `VanillaHudElements.HOTBAR` was
  extracted into the private texture. Use that message to distinguish a missing
  root from a projected/composite loss when the backing appears without slots.

## Relevant commits

- `25a8447` — shared virtual plane and selected capture architecture.
- `1a89847` — composite after normal GUI pass.
- `931b6de` — drive plane perspective from look pitch.
- `46874e5` — last user-observed working camera-yaw capture path; restored as
  the experimental-renderer baseline after the later refactor regressed F5W.

## Related files

- Compatibility test plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- User-facing setup: `spatial-hud-template-26.3/README.md`.
- F5W test runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Design constraint: `.agent/decisions/0001-selected-hud-only.md`.
