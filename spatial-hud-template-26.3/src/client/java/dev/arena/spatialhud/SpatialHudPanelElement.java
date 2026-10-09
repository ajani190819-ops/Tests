package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Starts the private capture for the active panel method and draws the purple
 * identity into it. It is also the single per-frame camera-pose update point,
 * before wrapped HUD elements read the shared pose, and it draws the failure
 * indicator when the capture has latched a failure. The indicator is red for
 * Method 3 and purple for Method 4, so the failing method shows at a glance.
 *
 * <p>Both panel methods use this private texture. The purple border is
 * captured with the selected vanilla roots, so the panel and the HUD move
 * together.</p>
 */
final class SpatialHudPanelElement implements HudElement {
	// Failure indicator colours: red for Method 3, purple for Method 4.
	private static final int RED_INDICATOR_OUTLINE = 0xFF3A0000;
	private static final int RED_INDICATOR_FILL = 0xFFE53935;
	private static final int PURPLE_INDICATOR_OUTLINE = 0xFF2A0A3D;
	private static final int PURPLE_INDICATOR_FILL = 0xFFC75CFF;

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

		// Method 3 is viewport-culled like the rest of its physical plane. When
		// it is out of view, nothing is drawn. Method 4 is never culled here.
		if (!cfg.usesPurplePanel()
				&& !SpatialHud.isPhysicalPanelVisibleInGui(graphics.guiWidth(), graphics.guiHeight())) {
			return;
		}

		ExperimentalHudCapture.beginFrame(graphics);
		// If the capture is not running, nothing is drawn here: SpatialHudElement
		// then shows the untouched vanilla HUD.
		ExperimentalHudCapture.capturePanelDecorations(cfg);
	}

	/**
	 * A small square just above the hotbar's top-left corner. It appears only
	 * after a capture failure, so a failed texture never looks like a working
	 * one. Red marks a Method 3 failure; purple marks a Method 4 failure.
	 */
	private static void drawFailureIndicator(GuiGraphicsExtractor graphics) {
		SpatialHudConfig.RenderMethod failed = ExperimentalHudCapture.failedMethod();
		boolean purple = failed == SpatialHudConfig.RenderMethod.POLYGON_TEST;
		int outline = purple ? PURPLE_INDICATOR_OUTLINE : RED_INDICATOR_OUTLINE;
		int fill = purple ? PURPLE_INDICATOR_FILL : RED_INDICATOR_FILL;
		int x0 = graphics.guiWidth() / 2 - 91 - 9;
		int y0 = graphics.guiHeight() - 22 - 9;
		graphics.fill(x0, y0, x0 + 8, y0 + 8, outline);
		graphics.fill(x0 + 1, y0 + 1, x0 + 7, y0 + 7, fill);
	}
}
