# Spatial HUD facts

- Status: three named renderer methods compile; all F5W runtime results remain
  pending.
- Last verified: 2026-10-08 CI build `37713731571` for configuration/method
  marker update `b1c9600`; no Method 3 runtime result exists yet.
- Read when: changing Spatial HUD code, its updater, or F5W compatibility.

## Source and release

- Mod source: `spatial-hud-template-26.3/`.
- Rolling artifact: GitHub release `spatial-hud-latest`, asset
  `spatial-hud-1.0.0.jar`.
- GitHub workflow: `.github/workflows/build-spatial-hud.yml`.
- Source version and release asset name may remain `1.0.0`; use commit and
  asset timestamp when identifying a test build.

## Rendering design

- `SpatialHudConfig.RenderMethod` is the one visible renderer selector:
  `CLASSIC_AFFINE` (original safe path), `CAPTURED_MESH` (private captured
  texture on a GUI-space projective mesh), and `WORLD_SPACE_TEXTURE`
  (**World-Space Texture**, the same selected texture drawn on a real level
  quad). Existing JSON with
  `experimentalCaptureWarp: true` migrates to `CAPTURED_MESH`.
- The Cloth Config GUI intentionally has three sections: **Setup & Render
  Method**, **Panel Positioning & Orientation**, and **HUD Contents**. All
  geometric controls, including Method 3-only anchor and terrain occlusion,
  share the positioning section; their tooltips state when a control is
  method-specific. The panel's short upper-left marker is green for Method 1,
  blue for Method 2, and red for Method 3. Texture methods capture the marker
  into their same selected-HUD source texture.
- `VirtualHudPlane.java` is the shared Method 1/2 physical waist-height
  lectern projection. It follows camera yaw rather than body yaw but never
  follows camera pitch to remain on-screen. Its primary 45° tilt is around the
  panel left-to-right axis, placing the far/top edge farther away; at the 30°
  minimum look-down gate, the default far edge is about 86% the width of the
  near edge. It refuses any panel with a behind-camera corner or no viewport
  intersection rather than clamping it to a huge card.
- `ExperimentalHudCapture.java` captures only the selected lower HUD into a
  private target. Method 2 composites it through a tessellated mesh; Method 3
  leaves it for `WorldSpaceHudRenderer` on the following level frame. The
  capture path exits before allocation/upload when the 30° look-down gate or
  physical viewport test fails. The rollback retains the isolated
  `GuiRenderer(..., List.of())` implementation from `46874e5`; it does not
  share or mutate the main PiP renderer map.
- `WorldSpaceHudRenderer.java` extracts a physical Method 3 quad during level
  extraction and draws the previous completed private texture after translucent
  terrain. `worldSpaceAnchor` selects Camera Yaw or Player Body; depth texture
  attachment is selected by `worldSpaceOccludeBehindWorld`. It uses **only
  vanilla pipelines**: `ENTITY_TRANSLUCENT` for depth-occluded rendering and
  `GUI_TEXTURED` for through-world rendering. Its GPU buffer is lazy.
- The attached F5W log found the real startup failure in `d2308fb`:
  `spatialhud:world_texture_through_world` cloned an entity snippet that
  requested undefined `Sampler1`, causing required shader-program reload
  failure. `036ca35` removes that custom pipeline entirely.
- `SpatialHudConfig.instance` and `registered` are explicitly excluded from
  Cloth Config. The supplied F5W log also showed Cloth Config trying to expose
  the private `instance` singleton as a setting; that GUI-provider error is
  fixed in `d6aae3a`.
- `SpatialHudGameRendererMixin.java` completes the private capture after the
  normal `GuiRenderer` call and closes Method 3's GPU buffer on shutdown.
- `SpatialHudGuiRendererMixin.java` redirects only the private captured
  renderer by object identity.
- The capture includes the backing plate and selected bottom-HUD pixels. It
  must not capture ordinary screens, chat, maps, menus, debug text, or an
  unrelated mod's GUI.
- The experimental mesh uses a 24 by 12 grid. Its projection comes from one
  virtual plane, so the backing and all captured pixels share the same corners
  and perspective.

## Perspective requirement

A physical angled-paper panel has a nearer lower edge and a farther upper edge;
when projected, the far edge must become narrower. This is projective geometry,
not independent affine scaling of icon groups. The selected default uses a 45°
lectern tilt while the player begins rendering at 30° downward pitch, making
that taper obvious instead of starting face-on and rectangular.

The configuration explicitly defines all orientation axes: primary/fine tilt
rotate around the panel's left-to-right axis; turn rotates around the up axis;
roll rotates around the panel normal. `minimumLookDownPitch` defaults to 30°
and is a hard no-render gate. Method 1 shares that physical placement but can
only use one affine centre tangent through Fabric's public HUD API. Method 2
maps the finished capture through the true tapered mesh; Method 3 emits the
same physical basis as a world quad. `virtualHorizonPerspectivePitch` remains
only as ignored old JSON data.

## Compatibility and fallback

- New installations default to Classic Affine. It cannot bend pixels inside an
  icon or text glyph.
- Method 2 and Method 3 are isolated texture experiments. Their shared capture
  failure latches them back to Classic Affine and logs the stage/error.
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
- `d2308fb` — clear three-method configuration and initial world-space texture
  renderer; CI-complete but no gameplay runtime result yet.

## Related files

- Compatibility test plan: `spatial-hud-template-26.3/COMPATIBILITY.md`.
- User-facing setup: `spatial-hud-template-26.3/README.md`.
- F5W test runbook: `.agent/runbooks/test-spatial-hud-f5w.md`.
- Design constraint: `.agent/decisions/0001-selected-hud-only.md`.
