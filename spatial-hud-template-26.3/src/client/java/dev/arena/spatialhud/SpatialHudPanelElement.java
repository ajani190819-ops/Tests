package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Starts the private capture for the active mode and draws the purple panel
 * decorations inside that capture. It is also the single per-frame camera-pose
 * update point, before wrapped HUD elements read the shared pose, and it draws
 * the red failure indicator when the capture has latched a failure.
 *
 * <p>Methods 3 and 4 both use this private texture. Their purple border (and,
 * for Method 4, the four corner handles) is captured with the selected vanilla
 * roots, so the surface and the HUD move together.</p>
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

		if (ExperimentalHudCapture.hasCaptureFailed()) {
			drawFailureIndicator(graphics);
		}

		if (cfg.usesPolygonTest()) {
			// Method 4 textures the selected lower HUD into the four purple GUI
			// corners, so the private capture has to start here - before the
			// wrapped vanilla roots extract below. The purple interior, border and
			// corner handles belong to that same texture.
			// If the capture is unavailable or failed, nothing is drawn here:
			// SpatialHudElement shows the untouched vanilla HUD and the red marker
			// above reports the failure.
			ExperimentalHudCapture.beginFrame(graphics);
			if (ExperimentalHudCapture.isFrameActive()) {
				ExperimentalHudCapture.capturePanelDecorations(cfg);
			}
			return;
		}

		// Method 3: the panel is viewport-culled like the rest of the physical
		// plane. When it is out of view, nothing is drawn.
		if (!SpatialHud.isPhysicalPanelVisibleInGui(graphics.guiWidth(), graphics.guiHeight())) {
			return;
		}

		ExperimentalHudCapture.beginFrame(graphics);
		// If the capture is not running, nothing is drawn here: SpatialHudElement
		// then shows the untouched vanilla HUD.
		ExperimentalHudCapture.capturePanelDecorations(cfg);
	}

	/**
	 * A small red square just above the hotbar's top-left corner. It appears
	 * only after a capture failure, so a failed texture never looks like a
	 * working one.
	 */
	private static void drawFailureIndicator(GuiGraphicsExtractor graphics) {
		int x0 = graphics.guiWidth() / 2 - 91 - 9;
		int y0 = graphics.guiHeight() - 22 - 9;
		graphics.fill(x0, y0, x0 + 8, y0 + 8, 0xFF3A0000);
		graphics.fill(x0 + 1, y0 + 1, x0 + 7, y0 + 7, 0xFFE53935);
	}
}
