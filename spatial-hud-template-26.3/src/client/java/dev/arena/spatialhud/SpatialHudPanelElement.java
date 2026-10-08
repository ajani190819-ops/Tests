package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * An optional translucent backing panel and a compact method-colour stripe
 * beneath the spatial strip. It is also the single per-frame camera-pose
 * update point, before wrapped HUD elements read the shared pose.
 */
final class SpatialHudPanelElement implements HudElement {
	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		if (!SpatialHud.isGameplayHudActive()) {
			return;
		}

		// Run even when the backing is hidden: this is registered immediately
		// before HOTBAR and gives every selected root one camera pitch value.
		SpatialHud.updateViewPose();

		// This prepares an isolated state only when the explicitly opt-in
		// experiment is active. It never touches the main GUI extractor or a
		// screen renderer; selected wrapped roots decide individually whether to
		// feed it below.
		ExperimentalHudCapture.beginFrame(graphics);

		SpatialHudConfig cfg = SpatialHudConfig.get();
		// In the texture modes, both the optional backing and short coloured mode
		// stripe are part of the same source texture as hotbar/status pixels. They
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
				graphics.fill(x0 - 6, y0 - 4, x1 + 6, y1 + 4, 0x80101018);
				graphics.fill(x0 - 6, y0 - 4, x1 + 6, y0, 0x5038384A);
			}
			// A compact top-edge stripe stays with the panel pose. Green identifies
			// stable Method 1, blue Method 2's captured mesh, and red Method 3's
			// physical world texture.
			graphics.fill(x0 - 6, y0 - 4, Math.min(x1 + 6, x0 + 20), y0,
					cfg.modeIndicatorColor());
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}

}
