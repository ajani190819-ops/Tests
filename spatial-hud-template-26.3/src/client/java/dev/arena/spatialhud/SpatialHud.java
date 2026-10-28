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
import net.fabricmc.fabric.impl.client.rendering.hud.HudElementRegistryImpl;
import net.fabricmc.fabric.impl.client.rendering.hud.HudLayer;

import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.resources.Identifier;

import java.util.List;
import java.util.Map;

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
	/** One-shot guard for the late wrapper reinforce pass. */
	private static boolean reinforcedWrappers;

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
			// One-shot, on the first tick after every mod has finished
			// registering HUD elements: put our capture wrappers back on the
			// outside. See reinforceStripWrappers().
			if (!reinforcedWrappers) {
				reinforcedWrappers = true;
				reinforceStripWrappers();
			}

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

			if (client.player == null) {
				resetSway();
			}
		});

		LOGGER.info("Spatial HUD initialized. Press H for the settings and guide.");
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

	/**
	 * Re-applies the capture wrappers once, on the first client tick after all
	 * mods have registered their HUD elements. A companion mod that replaces a
	 * vanilla root after our first wrap ends up outside it and then draws its
	 * copy on the vanilla HUD (the status bars "snapping" back to screen
	 * positions) while only the inner, wrapped vanilla root is captured.
	 * Replacing again puts SpatialHudElement back on the outside: a pure
	 * companion replacement draws its bars into the private texture instead of
	 * the screen, and a companion that delegates to the vanilla root is
	 * captured with it. Each pass nests one more wrapper; the nested capture
	 * extracts every root once into the same private texture.
	 */
	private static void reinforceStripWrappers() {
		for (Identifier id : STRIP_ELEMENTS) {
			try {
				HudElementRegistry.replaceElement(id, vanilla -> new SpatialHudElement(id, vanilla));
			} catch (Throwable t) {
				LOGGER.warn("Spatial HUD could not reinforce its capture wrapper for {}.", id, t);
			}
		}
		wrapCompanionLayers();
		wrapSlotCycling();
	}

	/**
	 * Wraps companion-mod layers attached to the strip roots. A layer is a
	 * sibling of the root's vanilla element, so wrapping only the root leaves
	 * the companion drawing on the vanilla HUD at screen positions while the
	 * panel shows its own captured copy - the status bars flickering between
	 * the two. Replacing each layer's element puts its pixels into the private
	 * capture instead. Our own layers (the panel element and the root wrappers
	 * themselves) are skipped; a double wrap would be harmless but wasteful.
	 */
	private static void wrapCompanionLayers() {
		try {
			Map<Identifier, HudElementRegistryImpl.RootLayer> roots = HudElementRegistryImpl.ROOT_ELEMENTS;
			for (Identifier rootId : STRIP_ELEMENTS) {
				HudElementRegistryImpl.RootLayer root = roots.get(rootId);
				if (root == null) {
					continue;
				}
				for (HudLayer layer : List.copyOf(root.layers())) {
					if (layer.isRemoved()) {
						continue;
					}
					Identifier layerId = layer.id();
					if (layerId.equals(rootId) || layerId.equals(SLOT_CYCLING_ELEMENT)
							|| layerId.getNamespace().equals("spatialhud")) {
						continue;
					}
					try {
						HudElementRegistry.replaceElement(layerId,
								vanilla -> new SpatialHudElement(rootId, vanilla));
						LOGGER.info("Spatial HUD captured companion HUD layer {} into the panel texture.", layerId);
					} catch (Throwable t) {
						LOGGER.warn("Spatial HUD could not wrap companion HUD layer {}.", layerId, t);
					}
				}
			}
		} catch (Throwable t) {
			LOGGER.warn("Spatial HUD could not scan the HUD registry for companion layers.", t);
		}
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
	 * Both panel methods present the captured texture. The Gameplay Only
	 * preference is the single gate: with it on, nothing draws over a screen;
	 * with it off, the capture also runs over screens. Only Method 3 is culled by
	 * the physical-plane viewport test. Method 4 is placed by its own feet anchor.
	 */
	static boolean isTextureCaptureActive() {
		Minecraft mc = Minecraft.getInstance();
		SpatialHudConfig cfg = SpatialHudConfig.get();
		return isGameplayHudActive()
					&& cfg.capturesTexture()
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
