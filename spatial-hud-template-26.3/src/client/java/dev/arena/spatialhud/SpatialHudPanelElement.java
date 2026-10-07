package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Translucent backing panel drawn behind the spatialized strip, using the
 * same pose math so it stays locked to the strip.
 */
final class SpatialHudPanelElement implements HudElement {
	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		if (!SpatialHud.isEnabled() || Minecraft.getInstance().player == null) {
			return;
		}

		SpatialHudConfig cfg = SpatialHudConfig.get();
		if (!cfg.showPanel) {
			return;
		}

		int w = graphics.guiWidth();
		int h = graphics.guiHeight();

		// Bounds around the vanilla bottom strip (hotbar + bars block).
		int x0 = w / 2 - 91;
		int x1 = w / 2 + 91;
		int y0 = h - 44;
		int y1 = h - 22;

		graphics.pose().pushMatrix();
		try {
			SpatialHudElement.applySpatialPose(graphics, cfg);
			graphics.fill(x0 - 5, y0 - 4, x1 + 5, y1 + 4, 0x90101018);
			graphics.fill(x0 - 5, y0 - 4, x1 + 5, y0, 0x60303040);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}
}
