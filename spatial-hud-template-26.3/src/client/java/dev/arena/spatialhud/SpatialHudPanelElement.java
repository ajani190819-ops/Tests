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
		if (!SpatialHud.isEnabled() || Minecraft.getInstance().player == null) {
			return;
		}

		// Run even when the backing is hidden: this is registered immediately
		// before HOTBAR and makes the time-based pose stable for every element.
		SpatialHud.updateRenderSway();

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
			graphics.fill(x0 - 6, y0 - 4, x1 + 6, y1 + 4, 0x80101018);
			graphics.fill(x0 - 6, y0 - 4, x1 + 6, y0, 0x5038384A);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}
}
