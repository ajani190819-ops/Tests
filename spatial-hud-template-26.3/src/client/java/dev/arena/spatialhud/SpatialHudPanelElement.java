package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Starts the private lower-HUD capture for the active mode and draws the
 * optional translucent backing panel and full-width method-colour band beneath
 * the spatial strip. It is also the single per-frame camera-pose update point,
 * before wrapped HUD elements read the shared pose.
 *
 * <p>Method 4 uses this same private texture: its purple interior, border, and
 * four corner handles are captured together with the selected vanilla roots,
 * and the composite maps that whole source rectangle onto the configured
 * corners.</p>
 */
final class SpatialHudPanelElement implements HudElement {
	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		if (!SpatialHud.isGameplayHudActive()) {
			return;
		}

		SpatialHudConfig cfg = SpatialHudConfig.get();

		// Run even when the backing is hidden: this is registered immediately
		// before HOTBAR and gives every selected root one camera pitch value.
		SpatialHud.updateViewPose();

		if (cfg.usesPolygonTest()) {
			// Method 4 textures the selected lower HUD into the four purple
			// GUI corners, so the private capture has to start here - before the
			// wrapped vanilla roots extract below. The purple interior, border
			// and corner handles belong to that same texture, which is why
			// nothing else is drawn while the capture is running: one surface,
			// already following the live pitch response of the target quad.
			ExperimentalHudCapture.beginFrame(graphics);
			if (ExperimentalHudCapture.isFrameActive() && ExperimentalHudCapture.capturePanelDecorations(cfg)) {
				return;
			}
			// Capture unavailable or failed for this frame. Keep the plain
			// outline and handles visible so the quad can still be seen and
			// tuned by hand while the vanilla HUD stays readable.
			PolygonTestRenderer.drawGuide(graphics, cfg);
			return;
		}

		// The physical-map methods keep finite-surface viewport culling.
		if (!SpatialHud.isPhysicalPanelVisibleInGui(graphics.guiWidth(), graphics.guiHeight())) {
			return;
		}

		// This prepares an isolated state only when the explicitly opt-in
		// experiment is active. It never touches the main GUI extractor or a
		// screen renderer; selected wrapped roots decide individually whether to
		// feed it below.
		ExperimentalHudCapture.beginFrame(graphics);

		// In the texture modes, both the optional backing and wide coloured mode
		// band are part of the same source texture as hotbar/status pixels. They
		// must not also draw here, or an affine duplicate would diverge from the
		// true projective/world surface.
		if (ExperimentalHudCapture.capturePanelDecorations(cfg)) {
			return;
		}

		int w = graphics.guiWidth();
		int h = graphics.guiHeight();

		// The safe fallback uses the same source envelope and local tangent as
		// the virtual plane. Its rectangle is intentionally affine; the capture
		// path above is the only mode that can bend its pixels exactly.
		int x0 = w / 2 - (int) VirtualHudPlane.SOURCE_HALF_WIDTH;
		int x1 = w / 2 + (int) VirtualHudPlane.SOURCE_HALF_WIDTH;
		int y0 = Math.max(0, h - (int) VirtualHudPlane.SOURCE_TOP_FROM_BOTTOM);
		int y1 = h;

		graphics.pose().pushMatrix();
		try {
			SpatialHudElement.applySpatialPose(graphics, cfg);
			if (cfg.showPanel) {
				graphics.fill(x0 - 6, y0 - 8, x1 + 6, y1 + 4, 0x80101018);
				graphics.fill(x0 - 6, y0 - 8, x1 + 6, y0, 0x5038384A);
			}
			// A deliberately full-width, 8px mode band stays inside the physical
			// source envelope, so the texture methods sample the exact same marker.
			// Green is Method 1's affine fallback, blue is Method 2's projective
			// mesh, and red is Method 3's world-space texture.
			graphics.fill(x0, y0, x1, y0 + 8, cfg.modeIndicatorColor());
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}

}
