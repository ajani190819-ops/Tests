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
		if (!cfg.showPanel) {
			return;
		}

		int w = graphics.guiWidth();
		int h = graphics.guiHeight();

		// Covers the whole vanilla bottom-strip stack: held name, bars, XP and
		// hotbar. Bounds are specified before the shared spatial transform.
		int x0 = w / 2 - 91;
		int x1 = w / 2 + 91;
		int y0 = h - 70;
		int y1 = h - 7;

		graphics.pose().pushMatrix();
		try {
			SpatialHudElement.applySpatialPose(graphics, cfg);
			if (cfg.taperBackingPlate) {
				drawTaperedBackingPlate(graphics, cfg, x0, x1, y0, y1);
			} else {
				graphics.fill(x0 - 6, y0 - 4, x1 + 6, y1 + 4, 0x80101018);
				graphics.fill(x0 - 6, y0 - 4, x1 + 6, y0, 0x5038384A);
			}
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}

	/**
	 * Draw the plate as one-pixel horizontal bands. A filled quad is not exposed
	 * by the safe GUI API, but bands produce a genuine trapezoid silhouette
	 * without a custom renderer, capture texture, or hook outside this HUD root.
	 */
	private static void drawTaperedBackingPlate(
			GuiGraphicsExtractor graphics, SpatialHudConfig cfg, int x0, int x1, int y0, int y1) {
		int centreX = (x0 + x1) / 2;
		int top = y0 - 4;
		int bottom = y1 + 4;
		float fullHalfWidth = (x1 - x0) / 2.0f + 6.0f;
		float horizonFarEdge = clamp(cfg.planeHorizonFarEdgeWidthPercent / 100.0f, 0.20f, 1.0f);
		float farEdgeWidth = lerp(horizonFarEdge, 1.0f, SpatialHudElement.planeTiltAmount(cfg));
		int height = Math.max(1, bottom - top);

		for (int y = top; y < bottom; y++) {
			float progress = (y - top) / (float) height;
			float halfWidth = fullHalfWidth * lerp(farEdgeWidth, 1.0f, progress);
			int left = Math.round(centreX - halfWidth);
			int right = Math.round(centreX + halfWidth);
			int color = y < top + 4 ? 0x5038384A : 0x80101018;
			graphics.fill(left, y, right, y + 1, color);
		}
	}

	private static float lerp(float from, float to, float amount) {
		return from + (to - from) * amount;
	}

	private static float clamp(float value, float min, float max) {
		return value < min ? min : (value > max ? max : value);
	}
}
