package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.resources.Identifier;

/**
 * Moves one vanilla HUD element into Spatial HUD's isolated GUI render state.
 * Minecraft still extracts the original element itself; it is simply drawn to
 * a transparent texture which is then presented as a real 3D panel.
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
		if (!SpatialHud.isWorldModeActive()) {
			vanilla.extractRenderState(graphics, deltaTracker);
			return;
		}
		if (!cfg.showElement(id)) {
			return;
		}

		try {
			SpatialHudWorldRenderer.get().extract(vanilla, deltaTracker);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
			// Preserve a usable HUD if an unexpected modded element fails.
			vanilla.extractRenderState(graphics, deltaTracker);
		}
	}

	/** Kept here so all element visibility rules remain in one well-known place. */
	static boolean isBottomStripElement(Identifier id) {
		return id.equals(VanillaHudElements.HOTBAR)
				|| id.equals(VanillaHudElements.ARMOR_BAR)
				|| id.equals(VanillaHudElements.HEALTH_BAR)
				|| id.equals(VanillaHudElements.FOOD_BAR)
				|| id.equals(VanillaHudElements.AIR_BAR)
				|| id.equals(VanillaHudElements.MOUNT_HEALTH)
				|| id.equals(VanillaHudElements.INFO_BAR)
				|| id.equals(VanillaHudElements.EXPERIENCE_LEVEL)
				|| id.equals(VanillaHudElements.HELD_ITEM_TOOLTIP)
				|| id.equals(VanillaHudElements.SPECTATOR_MENU)
				|| id.equals(VanillaHudElements.SPECTATOR_TOOLTIP);
	}
}
