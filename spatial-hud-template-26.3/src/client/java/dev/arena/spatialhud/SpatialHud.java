package dev.arena.spatialhud;

import com.mojang.blaze3d.platform.InputConstants;
import org.lwjgl.sdl.SDLScancode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import net.fabricmc.api.ClientModInitializer;
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
 * vanilla roots. Safe mode re-extracts them under one affine tangent; the
 * opt-in capture mode composites their completed texture through one
 * projective mesh. Disabling the mod immediately delegates every element back
 * to vanilla.</p>
 */
public class SpatialHud implements ClientModInitializer {
	public static final Logger LOGGER = LoggerFactory.getLogger("spatialhud");

	private static KeyMapping openConfigKey;
	private static KeyMapping toggleHudKey;
	private static KeyMapping selectMethodOneKey;
	private static KeyMapping selectMethodTwoKey;
	private static KeyMapping selectMethodThreeKey;
	private static boolean enabled;

	// These two mods add their visual details by injecting inside vanilla's
	// status-bar extraction methods. Their exact 26.3 Fabric releases are
	// AppleSkin 3.0.10 and Detail Armor Bar Reconstructed 5.3.2.
	private static boolean appleSkinLoaded;
	private static boolean detailArmorBarLoaded;

	// The panel samples camera pitch once before every selected HUD extraction,
	// so the backing and all captured roots share one mesh pose for that frame.
	static float pitch;

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
		appleSkinLoaded = FabricLoader.getInstance().isModLoaded("appleskin");
		detailArmorBarLoaded = FabricLoader.getInstance().isModLoaded("detailabreconst");
		WorldSpaceHudRenderer.initialize();

		if (cfg.preserveCompanionStatusLayout && (appleSkinLoaded || detailArmorBarLoaded)) {
			String companions = appleSkinLoaded && detailArmorBarLoaded
					? "AppleSkin and Detail Armor Bar Reconstructed"
					: (appleSkinLoaded ? "AppleSkin" : "Detail Armor Bar Reconstructed");
			LOGGER.info("Spatial HUD compatibility layout enabled for {}; affected status bars remain in their native layout while revealed.", companions);
		}

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
		// bindings rather than a persistent config dropdown. F6/F7/F8 avoid the
		// hotbar number keys while providing an immediate 1/2/3 selection; users
		// can rebind any conflict in the normal Minecraft Controls screen.
		selectMethodOneKey = registerMethodKey("key.spatialhud.select_method_1", SDLScancode.SDL_SCANCODE_F6);
		selectMethodTwoKey = registerMethodKey("key.spatialhud.select_method_2", SDLScancode.SDL_SCANCODE_F7);
		selectMethodThreeKey = registerMethodKey("key.spatialhud.select_method_3", SDLScancode.SDL_SCANCODE_F8);

		for (Identifier id : STRIP_ELEMENTS) {
			HudElementRegistry.replaceElement(id, vanilla -> new SpatialHudElement(id, vanilla));
		}

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
				enabled = !enabled;
				SpatialHudConfig.get().enabled = enabled;
				SpatialHudConfig.save();
				if (!enabled) {
					resetSway();
				}
			}

			while (selectMethodOneKey.consumeClick()) {
				selectRenderMethod(SpatialHudConfig.RenderMethod.CLASSIC_AFFINE, 1,
						"green balanced trapezoid warp");
			}
			while (selectMethodTwoKey.consumeClick()) {
				selectRenderMethod(SpatialHudConfig.RenderMethod.CAPTURED_MESH, 2,
						"blue strong trapezoid warp");
			}
			while (selectMethodThreeKey.consumeClick()) {
				selectRenderMethod(SpatialHudConfig.RenderMethod.WORLD_SPACE_TEXTURE, 3,
						"red real world map");
			}

			if (client.player == null) {
				resetSway();
			}
		});

		LOGGER.info("Spatial HUD forced-warp build initialized. Press H for the read-only guide; bind direct Method 1/2/3 keys in Controls.");
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
		// The full-width green/blue/red band is the deliberately on-panel visual
		// confirmation; logging also gives modpack troubleshooting an exact trace.
		LOGGER.info("Spatial HUD Method {} selected: {}.", number, description);
	}

	public static boolean isEnabled() {
		return enabled;
	}

	/**
	 * Never transform HUD elements while another screen owns the GUI. This keeps
	 * containers, Mod Menu, chat, inventories, config screens, and modded UI
	 * renderers completely outside Spatial HUD's scope.
	 */
	static boolean isGameplayHudActive() {
		Minecraft mc = Minecraft.getInstance();
		return enabled && mc.player != null && mc.level != null
				&& (!SpatialHudConfig.get().onlyDuringGameplay || mc.gui.screen() == null);
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
	 * Both texture methods have a stricter boundary than Classic Affine: they
	 * are gameplay-only even if a user turns off the normal Gameplay Only
	 * preference. This keeps the private capture separate from every screen.
	 */
	static boolean isTextureCaptureActive() {
		Minecraft mc = Minecraft.getInstance();
		return isGameplayHudActive()
					&& SpatialHudConfig.get().capturesTexture()
					&& mc.gui.screen() == null
					&& isPhysicalPanelVisibleInGui(
							mc.getWindow().getGuiScaledWidth(), mc.getWindow().getGuiScaledHeight());
	}

	/** True only for the third renderer: a captured texture on a world-space quad. */
	static boolean isWorldSpaceTextureActive() {
		return isTextureCaptureActive() && SpatialHudConfig.get().usesWorldSpaceTexture();
	}

	/**
	 * AppleSkin 3.0.10 injects its saturation/food/health decorations inside
	 * {@code Hud.extractFood}/{@code extractHearts}; Detail Armor Bar
	 * Reconstructed 5.3.2 injects inside {@code Hud.extractArmor}. Keeping those
	 * roots native while they are revealed makes their complete, already-laid-out
	 * groups draw together. It avoids assuming a registration order for either
	 * mod and does not hook any unrelated HUD/GUI layer.
	 */
	static boolean shouldPreserveNativeStatusLayout(Identifier id, SpatialHudConfig cfg) {
		if (!cfg.preserveCompanionStatusLayout) {
			return false;
		}
		if (!appleSkinLoaded && !detailArmorBarLoaded) {
			return false;
		}
		// Preserve the whole adjacent group. Keeping only one root native would
		// allow a custom armor-row height or air bar to split the group again.
		return id.equals(VanillaHudElements.ARMOR_BAR)
				|| id.equals(VanillaHudElements.HEALTH_BAR)
				|| id.equals(VanillaHudElements.FOOD_BAR)
				|| id.equals(VanillaHudElements.AIR_BAR);
	}

	/**
	 * The virtual-plane refactor is always visible. This method remains only so
	 * a pre-v1.1 JSON field cannot reintroduce a hidden companion status group.
	 */
	static boolean isStatusLayoutRevealed(SpatialHudConfig cfg) {
		return true;
	}

	/** Samples the current camera pitch once before the selected HUD roots extract. */
	static void updateViewPose() {
		Minecraft mc = Minecraft.getInstance();
		if (!enabled || mc.player == null) {
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
		enabled = false;
		resetSway();
		SpatialHudConfig.get().enabled = false;
		SpatialHudConfig.save();
		LOGGER.error("Spatial HUD hit an error and disabled itself (vanilla HUD is restored):", t);
	}
}
