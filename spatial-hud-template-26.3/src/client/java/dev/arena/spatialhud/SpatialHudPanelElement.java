package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * A quiet translucent backing panel drawn beneath the spatial strip. It is
 * also the single per-frame update point for sway, before wrapped HUD
 * elements read the shared pose.
 */
final class SpatialHudPanelElement implements HudElement {
	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		if (!SpatialHud.isGameplayHudActive()) {
			return;
		}

		// Run even when the backing is hidden: this is registered immediately
		// before HOTBAR and makes the time-based pose stable for every element.
		SpatialHud.updateRenderSway();

		// This prepares an isolated state only when the explicitly opt-in
		// experiment is active. It never touches the main GUI extractor or a
		// screen renderer; selected wrapped roots decide individually whether to
		// feed it below.
		ExperimentalHudCapture.beginFrame(graphics);

		SpatialHudConfig cfg = SpatialHudConfig.get();
		// In captured-mesh mode the backing is part of the same source texture
		// as hotbar/status pixels. It must not also draw here, or a second affine
		// plate would visibly diverge from the true projective icon surface.
		if (ExperimentalHudCapture.captureBacking(cfg)) {
			return;
		}
		if (!cfg.showPanel) {
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
			graphics.fill(x0 - 6, y0 - 4, x1 + 6, y1 + 4, 0x80101018);
			graphics.fill(x0 - 6, y0 - 4, x1 + 6, y0, 0x5038384A);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}

}
