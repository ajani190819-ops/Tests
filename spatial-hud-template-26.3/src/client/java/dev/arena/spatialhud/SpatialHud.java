package dev.arena.spatialhud;

import com.mojang.blaze3d.platform.InputConstants;
import org.lwjgl.sdl.SDLScancode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientLifecycleEvents;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.loader.api.FabricLoader;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElementRegistry;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.resources.Identifier;

import java.util.List;

/**
 * Spatial HUD — a compact, smooth Spatial-GUI-style panel for the vanilla
 * bottom HUD strip (hotbar, bars, XP and held-item name).
 *
 * <p>The render path intentionally uses Fabric's HUD API for its selected
 * vanilla roots. The captured texture is drawn on the Method 3 world panel or
 * the Method 4 purple quad. Disabling the mod immediately delegates every element back
 * to vanilla.</p>
 */
public class SpatialHud implements ClientModInitializer {
	public static final Logger LOGGER = LoggerFactory.getLogger("spatialhud");

	private static KeyMapping openConfigKey;
	private static KeyMapping toggleHudKey;
	private static KeyMapping selectMethodThreeKey;
	private static KeyMapping selectMethodFourKey;

	// The panel samples camera pitch once before every selected HUD extraction,
	// so the backing and all captured roots share one mesh pose for that frame.
	static float pitch;

	/**
	 * Hotbar Slot Cycling's cycle-slot display. It is attached after the hotbar
	 * by that mod's own client setup, so it is wrapped once the client starts.
	 */
	static final Identifier SLOT_CYCLING_ELEMENT =
			Identifier.fromNamespaceAndPath("hotbarslotcycling", "cycling_slots");

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
		WorldSpaceHudRenderer.initialize();

		// H is deliberately reserved for rapid HUD tuning. The HUD enable switch
		// remains available in its configuration screen; this key only opens that
		// screen and never changes render state by itself.
		openConfigKey = KeyMappingHelper.registerKeyMapping(new KeyMapping(
				"key.spatialhud.open_config",
				InputConstants.Type.KEYBOARD,
				SDLScancode.SDL_SCANCODE_H,
				KeyMapping.Category.MISC));

		// Deliberately unbound. The Controls menu exposes a separate quick
		// enable/disable action without taking another key in a large modpack.
		toggleHudKey = KeyMappingHelper.registerKeyMapping(new KeyMapping(
				"key.spatialhud.toggle",
				InputConstants.UNKNOWN.getType(),
				InputConstants.UNKNOWN.getValue(),
				KeyMapping.Category.MISC));

		// Render mode is deliberately selected through these direct Controls-menu
		// bindings rather than a persistent config dropdown. F8/F9 avoid the
		// hotbar number keys while providing an immediate Method 3 or 4 selection;
		// users can rebind any conflict in the normal Minecraft Controls screen.
		selectMethodThreeKey = registerMethodKey("key.spatialhud.select_method_3", SDLScancode.SDL_SCANCODE_F8);
		selectMethodFourKey = registerMethodKey("key.spatialhud.select_method_4", SDLScancode.SDL_SCANCODE_F9);

		for (Identifier id : STRIP_ELEMENTS) {
			HudElementRegistry.replaceElement(id, vanilla -> new SpatialHudElement(id, vanilla));
		}

		ClientLifecycleEvents.CLIENT_STARTED.register(client -> wrapSlotCycling());

		// The panel is extracted before HOTBAR. It also marks the start of our
		// render-frame update, eliminating the old 20 Hz (tick-only) sway.
		HudElementRegistry.attachElementBefore(
				VanillaHudElements.HOTBAR,
				Identifier.fromNamespaceAndPath("spatialhud", "panel"),
				new SpatialHudPanelElement());

		ClientTickEvents.END_CLIENT_TICK.register(client -> {
			while (openConfigKey.consumeClick()) {
				client.setScreenAndShow(
						me.shedaniel.autoconfig.AutoConfigClient
								.getConfigScreen(SpatialHudConfig.class, null).get());
			}

			while (toggleHudKey.consumeClick()) {
				SpatialHudConfig toggleConfig = SpatialHudConfig.get();
				toggleConfig.enabled = !toggleConfig.enabled;
				SpatialHudConfig.save();
				if (!toggleConfig.enabled) {
					resetSway();
				}
				LOGGER.info("Spatial HUD toggled {}.", toggleConfig.enabled ? "on" : "off");
			}

			while (selectMethodThreeKey.consumeClick()) {
				selectRenderMethod(SpatialHudConfig.RenderMethod.WORLD_SPACE_TEXTURE, 3,
						"real 3D world-space panel");
			}
			while (selectMethodFourKey.consumeClick()) {
				selectRenderMethod(SpatialHudConfig.RenderMethod.POLYGON_TEST, 4,
						"purple horizontal panel");
			}

			if (client.player == null) {
				resetSway();
			}
		});

		LOGGER.info("Spatial HUD initialized. Press H for the read-only guide; direct keys: F8 = Method 3, F9 = Method 4.");
	}

	/**
	 * Wraps the slot cycler's element so its cycle slots are captured with the
	 * hotbar. Skipped when that mod is not installed. If the replacement fails,
	 * its slots stay on the vanilla HUD, outside the panel.
	 */
	private static void wrapSlotCycling() {
		if (!FabricLoader.getInstance().isModLoaded("hotbarslotcycling")) {
			return;
		}
		try {
			HudElementRegistry.replaceElement(SLOT_CYCLING_ELEMENT,
					vanilla -> new SpatialHudElement(SLOT_CYCLING_ELEMENT, vanilla));
			LOGGER.info("Spatial HUD registered Hotbar Slot Cycling's cycle slots for the panel capture.");
		} catch (Throwable t) {
			LOGGER.error("Spatial HUD could not register Hotbar Slot Cycling's cycle slots; they stay on the vanilla HUD.", t);
		}
	}

	private static KeyMapping registerMethodKey(String translationKey, int defaultScancode) {
		return KeyMappingHelper.registerKeyMapping(new KeyMapping(
				translationKey,
				InputConstants.Type.KEYBOARD,
				defaultScancode,
				KeyMapping.Category.MISC));
	}

	private static void selectRenderMethod(SpatialHudConfig.RenderMethod method, int number,
			String description) {
		SpatialHudConfig cfg = SpatialHudConfig.get();
		if (cfg.selectedRenderMethod() != method) {
			cfg.selectRenderMethod(method);
			SpatialHudConfig.save();
		}
		// Logging gives modpack troubleshooting an exact trace of direct key use.
		LOGGER.info("Spatial HUD Method {} selected: {}.", number, description);
	}

	/** The config instance is the sole live source of truth for the Enable toggle. */
	public static boolean isEnabled() {
		return SpatialHudConfig.get().enabled;
	}

	/**
	 * Never transform HUD elements while another screen owns the GUI. This keeps
	 * containers, Mod Menu, chat, inventories, config screens, and modded UI
	 * renderers completely outside Spatial HUD's scope.
	 */
	static boolean isGameplayHudActive() {
		Minecraft mc = Minecraft.getInstance();
		SpatialHudConfig cfg = SpatialHudConfig.get();
		return cfg.enabled && mc.player != null && mc.level != null
				&& (!cfg.onlyDuringGameplay || mc.gui.screen() == null);
	}

	/**
	 * Keeps selected vanilla HUD roots hidden only when their finite physical
	 * map-like panel is actually outside the viewport. There is deliberately no
	 * fixed look-pitch cutoff: if any part of the panel is in the player's field
	 * of view, it stays live; if it is outside, no selected-HUD rendering occurs.
	 */
	static boolean isPhysicalPanelVisibleInGui(int guiWidth, int guiHeight) {
		return VirtualHudPlane.forGui(SpatialHudConfig.get(), guiWidth, guiHeight).intersectsViewport();
	}

	/**
	 * Both panel methods present the captured texture, so they share one
	 * stricter boundary than a plain HUD transform: they are gameplay-only even
	 * if a user turns off the normal Gameplay Only preference. This keeps the
	 * private capture separate from every screen. Only Method 3 is culled by the
	 * physical-plane viewport test. Method 4 is placed by its own feet anchor.
	 */
	static boolean isTextureCaptureActive() {
		Minecraft mc = Minecraft.getInstance();
		SpatialHudConfig cfg = SpatialHudConfig.get();
		return isGameplayHudActive()
					&& cfg.capturesTexture()
					&& mc.gui.screen() == null
					&& (cfg.usesPurplePanel() || isPhysicalPanelVisibleInGui(
							mc.getWindow().getGuiScaledWidth(), mc.getWindow().getGuiScaledHeight()));
	}

	/** Both panel methods draw the captured texture on a world-space quad. */
	static boolean isWorldSpaceTextureActive() {
		return isTextureCaptureActive();
	}

	/** Samples the current camera pitch once before the selected HUD roots extract. */
	static void updateViewPose() {
		Minecraft mc = Minecraft.getInstance();
		if (!isEnabled() || mc.player == null) {
			pitch = 0.0f;
			return;
		}
		pitch = mc.player.getXRot();
	}

	private static void resetSway() {
		pitch = 0.0f;
	}

	/** Self-protection: never keep the HUD broken over our own math. */
	static void safeDisable(Throwable t) {
		resetSway();
		SpatialHudConfig.get().enabled = false;
		SpatialHudConfig.save();
		LOGGER.error("Spatial HUD hit an error and disabled itself (vanilla HUD is restored):", t);
	}
}
