# Spatial HUD facts

- Status: active; some runtime behavior remains unverified.
- Last verified: 2026-10-07 CI build `37705190664` for `bb99cda`; runtime
  behavior remains unverified.
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
  The current pending revision makes its default pose camera-yaw-local:
  waist-height and in front of the player, following camera yaw rather than
  body yaw, with a mesh that rotates in response to look pitch.
- `ExperimentalHudCapture.java` captures only the selected lower HUD into a
  private target and renders it through a tessellated mesh. Its private
  `GuiRenderer` must share Minecraft's existing picture-in-picture renderer
  map: 26.3 GUI item rendering uses that path, and an empty map can omit
  hotbar item content while leaving simple backing/bar pixels visible.
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

The accepted model is camera-yaw anchored with pitch-driven mesh rotation.
At `virtualFaceOnLookDownPitch` (default 30°), it is face-on. Looking higher
makes the top/far edge narrower; looking lower changes perspective in the
opposite direction. `virtualHorizonPerspectivePitch` controls the maximum
horizon taper (default 80°); `virtualPitch` is a manual offset. If all sampled
mesh points leave the GUI vertically, the projection is translated only enough
to expose an edge; the trapezoid geometry is not changed.

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
  latch message. The post-GUI composite and later hologram-pose changes require
  real F5W testing.
- The current capture path logs once whether `VanillaHudElements.HOTBAR` was
  extracted into the private texture. Use that message to distinguish a missing
  root from a projected/composite loss when the backing appears without slots.

## Relevant commits

- `25a8447` — shared virtual plane and selected capture architecture.
- `1a89847` — composite after normal GUI pass.
- `931b6de` — drive plane perspective from look pitch.

## Related files

- Compatibility test plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- User-facing setup: `spatial-hud-template-26.3/README.md`.
- F5W test runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Design constraint: `.agent/decisions/0001-selected-hud-only.md`.
