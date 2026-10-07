package dev.arena.spatialhud;

import com.mojang.blaze3d.platform.InputConstants;
import org.lwjgl.sdl.SDLScancode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElementRegistry;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;

import net.minecraft.client.KeyMapping;
import net.minecraft.resources.Identifier;

import java.util.List;

/**
 * Spatial HUD — the bottom HUD strip (hotbar, hearts, hunger, armor, air,
 * XP, mount bars, held item name) rendered as a panel floating in front of
 * you instead of glued to the screen edge.
 *
 * MC 26.x rebuilt the GUI pipeline around render-state extraction with 2D
 * affine transforms, so v0.3 renders the strip as a screen-parallel plane
 * at a true perspective distance (scale computed from the actual FOV and
 * distance in blocks), with look-lag "sway". It uses only the official
 * Fabric HUD API — HudElementRegistry.replaceElement wraps each vanilla
 * element; disabling the mod makes every wrapper delegate back to vanilla.
 */
public class SpatialHud implements ClientModInitializer {
	public static final Logger LOGGER = LoggerFactory.getLogger("spatialhud");

	private static KeyMapping toggleKey;
	private static boolean enabled;

	// Look-lag sway: smoothed camera angles, updated once per tick.
	static float smoothYaw, smoothPitch;
	static float yaw, pitch;
	private static boolean snapped = false;

	/** The vanilla elements that make up the bottom HUD strip. */
	private static final List<Identifier> STRIP_ELEMENTS = List.of(
			VanillaHudElements.HOTBAR,
			VanillaHudElements.ARMOR_BAR,
			VanillaHudElements.HEALTH_BAR,
			VanillaHudElements.FOOD_BAR,
			VanillaHudElements.AIR_BAR,
			VanillaHudElements.MOUNT_HEALTH,
			VanillaHudElements.INFO_BAR,
			VanillaHudElements.EXPERIENCE_LEVEL,
			VanillaHudElements.HELD_ITEM_TOOLTIP,
			VanillaHudElements.SPECTATOR_MENU,
			VanillaHudElements.SPECTATOR_TOOLTIP);

	@Override
	public void onInitializeClient() {
		SpatialHudConfig cfg = SpatialHudConfig.load();
		enabled = cfg.enabled;

		toggleKey = KeyMappingHelper.registerKeyMapping(new KeyMapping(
				"key.spatialhud.toggle",
				InputConstants.Type.KEYBOARD,
				SDLScancode.SDL_SCANCODE_H,
				KeyMapping.Category.MISC));

		// Wrap every bottom-strip element: spatial pose when enabled,
		// perfect vanilla passthrough when disabled.
		for (Identifier id : STRIP_ELEMENTS) {
			HudElementRegistry.replaceElement(id, vanilla -> new SpatialHudElement(id, vanilla));
		}

		// Translucent backing panel, drawn just under the strip elements.
		HudElementRegistry.attachElementBefore(
				VanillaHudElements.HOTBAR,
				Identifier.fromNamespaceAndPath("spatialhud", "panel"),
				new SpatialHudPanelElement());

		ClientTickEvents.END_CLIENT_TICK.register(client -> {
			while (toggleKey.consumeClick()) {
				enabled = !enabled;
				SpatialHudConfig.get().enabled = enabled;
				SpatialHudConfig.save();
				if (!enabled) {
					snapped = false;
				}
			}

			if (client.player != null) {
				yaw = client.player.getYRot();
				pitch = client.player.getXRot();
				if (!snapped) {
					// Snap once so the plane doesn't fly in from angle 0.
					smoothYaw = yaw;
					smoothPitch = pitch;
					snapped = true;
				} else {
					smoothYaw += wrapDegrees(yaw - smoothYaw) * 0.35f;
					smoothPitch += (pitch - smoothPitch) * 0.35f;
				}
			}
		});

		LOGGER.info("Spatial HUD initialized. Press H to toggle; config at config/spatialhud.json");
	}

	public static boolean isEnabled() {
		return enabled;
	}

	/** Self-protection: never keep the HUD broken over our own math. */
	static void safeDisable(Throwable t) {
		enabled = false;
		SpatialHudConfig.get().enabled = false;
		SpatialHudConfig.save();
		LOGGER.error("Spatial HUD hit an error and disabled itself (vanilla HUD is restored):", t);
	}

	static float wrapDegrees(float d) {
		d %= 360.0f;
		if (d >= 180.0f) d -= 360.0f;
		if (d < -180.0f) d += 360.0f;
		return d;
	}
}
