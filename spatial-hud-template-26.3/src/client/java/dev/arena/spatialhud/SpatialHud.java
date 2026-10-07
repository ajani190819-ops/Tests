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
import net.fabricmc.fabric.api.client.rendering.v1.level.LevelRenderEvents;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.resources.Identifier;

import java.util.List;

/**
 * Spatial HUD — a compact, smooth Spatial-GUI-style panel for the vanilla
 * bottom HUD strip (hotbar, bars, XP and held-item name).
 *
 * <p>The render path intentionally uses only Fabric's official HUD API. Each
 * vanilla element is re-extracted under one shared affine pose; disabling the
 * mod immediately delegates every element back to vanilla.</p>
 */
public class SpatialHud implements ClientModInitializer {
	public static final Logger LOGGER = LoggerFactory.getLogger("spatialhud");

	private static KeyMapping toggleKey;
	private static KeyMapping openConfigKey;
	private static boolean enabled;

	// Render-frame sway state. The panel updates it once before the strip is
	// extracted, so every wrapped vanilla element has precisely the same pose.
	static float smoothYaw, smoothPitch;
	static float yaw, pitch;
	private static boolean snapped;
	private static long lastSwayNanos;

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
		SpatialHudConfig cfg = SpatialHudConfig.registerAndLoad();
		enabled = cfg.enabled;

		toggleKey = KeyMappingHelper.registerKeyMapping(new KeyMapping(
				"key.spatialhud.toggle",
				InputConstants.Type.KEYBOARD,
				SDLScancode.SDL_SCANCODE_H,
				KeyMapping.Category.MISC));

		// Just like Spatial GUI, this is unbound by default to avoid claiming a
		// key in a large modpack. It can be assigned under Controls, while the
		// same screen is always available from Mod Menu.
		openConfigKey = KeyMappingHelper.registerKeyMapping(new KeyMapping(
				"key.spatialhud.open_config",
				InputConstants.UNKNOWN.getType(),
				InputConstants.UNKNOWN.getValue(),
				KeyMapping.Category.MISC));

		for (Identifier id : STRIP_ELEMENTS) {
			HudElementRegistry.replaceElement(id, vanilla -> new SpatialHudElement(id, vanilla));
		}

		// The panel is extracted before HOTBAR. It also marks the start of our
		// render-frame update, eliminating the old 20 Hz (tick-only) sway.
		HudElementRegistry.attachElementBefore(
				VanillaHudElements.HOTBAR,
				Identifier.fromNamespaceAndPath("spatialhud", "panel"),
				new SpatialHudPanelElement());

		// Capture the real world projection just before the GUI phase. The
		// renderer restores this projection to draw the HUD texture in 3D.
		LevelRenderEvents.END_MAIN.register(context -> SpatialHudWorldRenderer.get().capturePerspective());

		ClientTickEvents.END_CLIENT_TICK.register(client -> {
			while (toggleKey.consumeClick()) {
				enabled = !enabled;
				SpatialHudConfig.get().enabled = enabled;
				SpatialHudConfig.save();
				if (!enabled) {
					resetSway();
				}
			}

			while (openConfigKey.consumeClick()) {
				client.setScreenAndShow(
						me.shedaniel.autoconfig.AutoConfigClient
								.getConfigScreen(SpatialHudConfig.class, null).get());
			}

			if (client.player == null) {
				resetSway();
			}
		});

		LOGGER.info("Spatial HUD initialized. Press H to toggle; configure it from Mod Menu or an assigned Controls key.");
	}

	public static boolean isEnabled() {
		return enabled;
	}

	/**
	 * Called by the panel once per HUD extraction frame. Spatial GUI uses the
	 * same time-based exponential filtering for its first-person parallax: it
	 * stays fluid at any FPS rather than stepping once per client tick.
	 */
	static void updateRenderSway() {
		Minecraft mc = Minecraft.getInstance();
		if (!enabled || mc.player == null) {
			resetSway();
			return;
		}

		yaw = mc.player.getYRot();
		pitch = mc.player.getXRot();
		long now = System.nanoTime();
		if (!snapped || lastSwayNanos == 0L) {
			smoothYaw = yaw;
			smoothPitch = pitch;
			snapped = true;
			lastSwayNanos = now;
			return;
		}

		float dt = Math.min((now - lastSwayNanos) / 1_000_000_000.0f, 0.1f);
		lastSwayNanos = now;
		float tau = Math.max(20, SpatialHudConfig.get().swayResponseMs) / 1000.0f;
		float alpha = 1.0f - (float) Math.exp(-dt / tau);
		smoothYaw += wrapDegrees(yaw - smoothYaw) * alpha;
		smoothPitch += (pitch - smoothPitch) * alpha;
	}

	private static void resetSway() {
		snapped = false;
		lastSwayNanos = 0L;
	}

	/** Self-protection: never keep the HUD broken over our own math. */
	static void safeDisable(Throwable t) {
		enabled = false;
		resetSway();
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
