package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.resources.Identifier;

/**
 * Wraps one vanilla bottom-strip element. When Spatial HUD is off, or its
 * private capture is not running, the untouched vanilla root is drawn. When
 * the capture is running, the root is fed into the private texture instead.
 */
final class SpatialHudElement implements HudElement {
	private final Identifier id;
	private final HudElement vanilla;

	SpatialHudElement(Identifier id, HudElement vanilla) {
		this.id = id;
		this.vanilla = vanilla;
	}

	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		SpatialHudConfig cfg = SpatialHudConfig.get();
		if (!SpatialHud.isGameplayHudActive()) {
			vanilla.extractRenderState(graphics, deltaTracker);
			return;
		}
		// Method 4 bypasses physical-map viewport culling because its target quad
		// is placed by four GUI-space handles. Method 3 keeps viewport culling: a
		// panel that is out of view draws no HUD at all, by design.
		if (!cfg.usesPolygonTest()
				&& !SpatialHud.isPhysicalPanelVisibleInGui(graphics.guiWidth(), graphics.guiHeight())) {
			return;
		}
		if (!cfg.showElement(id)) {
			return;
		}

		// The selected root goes into the private capture; the texture is then
		// drawn on the world panel (Method 3) or the purple quad (Method 4).
		if (ExperimentalHudCapture.isFrameActive()
				&& ExperimentalHudCapture.capture(id, (isolated, tracker) -> vanilla.extractRenderState(isolated, tracker), deltaTracker)) {
			return;
		}

		// The capture is not running: it failed and latched for this session, or
		// this frame never started one. Draw the untouched vanilla root so the
		// player keeps a HUD. SpatialHudPanelElement shows the red indicator.
		vanilla.extractRenderState(graphics, deltaTracker);
	}
}
