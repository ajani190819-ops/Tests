package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/** Draws a restrained glass-like backing into the captured HUD texture. */
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

		try {
			GuiGraphicsExtractor capture = SpatialHudWorldRenderer.get().graphics();
			if (capture == null) {
				return;
			}

			int w = capture.guiWidth();
			int h = capture.guiHeight();
			int x0 = w / 2 - 96;
			int x1 = w / 2 + 96;
			int y0 = h - 71;
			int y1 = h - 6;

			// The main surface is intentionally subtle; the game HUD remains the
			// focus while a narrow top highlight gives the panel a finished edge.
			capture.fill(x0, y0, x1, y1, 0x94101420);
			capture.fill(x0, y0, x1, y0 + 2, 0x887A8CA8);
			capture.fill(x0, y1 - 1, x1, y1, 0x40101018);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		}
	}
}
