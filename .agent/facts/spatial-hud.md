# Spatial HUD facts

- Status: active; some runtime behavior remains unverified.
- Last verified: 2026-10-07 build CI.
- Read when: changing Spatial HUD code, its updater, or F5W compatibility.

## Source and release

- Mod source: `spatial-hud-template-26.3/`.
- Rolling artifact: GitHub release `spatial-hud-latest`, asset
  `spatial-hud-1.0.0.jar`.
- GitHub workflow: `.github/workflows/build-spatial-hud.yml`.
- Source version and release asset name may remain `1.0.0`; use commit and
  asset timestamp when identifying a test build.

## Rendering design

- `VirtualHudPlane.java` is the shared physical-plane projection model.
- `ExperimentalHudCapture.java` captures only the selected lower HUD into a
  private target and renders it through a tessellated mesh.
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

With `virtualTiltWithLook` enabled, view pitch changes the plane's effective
pitch continuously. `virtualFaceOnLookDownPitch` selects the downward view
angle at which the plane becomes face-on. `virtualPitch` is a manual offset.

## Compatibility and fallback

- Experimental captured-mesh mode is default off.
- The normal mode is a safe affine fallback. It cannot bend pixels inside an
  icon or text glyph.
- First-class targets: AppleSkin and Detail Armor Bar Reconstructed.
- F5W also includes other same-region HUD mods, including Bedrock Hotbar,
  Immersive Hotbar, DualBar, Armor Indicator, Status Effect Bars, Mount
  Opacity, and Durability Warner HUD.
- The user reported an early capture build flickering, with invisible backing
  and no visible deformation. The supplied `latest.log` had no capture failure
  latch message. The post-GUI composite and look-driven plane changes were
  built after that observation and require real F5W testing.

## Relevant commits

- `25a8447` — shared virtual plane and selected capture architecture.
- `1a89847` — composite after normal GUI pass.
- `931b6de` — drive plane perspective from look pitch.

## Related files

- Compatibility test plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- User-facing setup: `spatial-hud-template-26.3/README.md`.
- F5W test runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Design constraint: `.agent/decisions/0001-selected-hud-only.md`.
