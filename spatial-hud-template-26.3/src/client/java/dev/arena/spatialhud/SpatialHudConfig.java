package dev.arena.spatialhud;

import me.shedaniel.autoconfig.AutoConfig;
import me.shedaniel.autoconfig.AutoConfigClient;
import me.shedaniel.autoconfig.ConfigData;
import me.shedaniel.autoconfig.annotation.Config;
import me.shedaniel.autoconfig.annotation.ConfigEntry;
import me.shedaniel.autoconfig.serializer.GsonConfigSerializer;
import me.shedaniel.clothconfig2.api.AbstractConfigListEntry;
import me.shedaniel.clothconfig2.api.ConfigEntryBuilder;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;

/**
 * Marks a transient field as a display-only card in the Method Guide tab.
 * The dedicated AutoConfig provider below renders static wrapped text instead
 * of an editable configuration widget, so guide prose never becomes saved
 * player configuration.
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.FIELD)
@interface MethodGuideText {
	int color() default -1;
}

/**
 * The in-game Cloth Config definition for Spatial HUD.
 *
 * <p>This intentionally uses the same configuration framework as Spatial GUI:
 * the settings appear in Mod Menu, can be opened from the Controls settings
 * keybind, and are stored in {@code config/spatialhud.json}. The old v0.3 JSON
 * fields keep their names, so existing settings continue to load.</p>
 */
@Config(name = "spatialhud")
public final class SpatialHudConfig implements ConfigData {
	/** Incremented when a safe default migration is needed. */
	@ConfigEntry.Category("guide")
	@ConfigEntry.Gui.Excluded
	// Starts at 0 so a v0.3 file, which has no version field, is detected.
	// registerAndLoad writes it as 18 after checking the values.
	public int configVersion = 0;

	/*
	 * Display-only cards. They are transient so Gson never saves them, and the
	 * MethodGuideText provider turns them into wrapped prose rather than inputs.
	 * Declaring them first also puts the plain-language guide first in Cloth
	 * Config's category bar.
	 */
	@ConfigEntry.Category("guide")
	@MethodGuideText
	public transient String guideOverview = "";

	@ConfigEntry.Category("guide")
	@MethodGuideText(color = 0xFF38C172)
	public transient String guideClassicAffine = "";

	@ConfigEntry.Category("guide")
	@MethodGuideText(color = 0xFF469AEF)
	public transient String guideCapturedMesh = "";

	@ConfigEntry.Category("guide")
	@MethodGuideText(color = 0xFFEF5350)
	public transient String guideWorldSpace = "";

	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Tooltip
	public boolean enabled = true;

	// Earlier affine-only settings retained solely for old JSON files.
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	public boolean autoScaleByFov = true;

	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	public int fovBaseline = 70;

	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Tooltip
	public boolean showPanel = true;

	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Tooltip
	public boolean onlyDuringGameplay = true;

	/**
	 * The one top-level rendering choice. The method controls the implementation,
	 * not which HUD parts are visible; those controls remain in HUD Contents.
	 */
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Tooltip
	// Perspective capture is the baseline: every selectable method now carries
	// the complete lower-HUD texture rather than silently defaulting to a flat
	// root-by-root presentation.
	public RenderMethod renderMethod = RenderMethod.CAPTURED_MESH;

	public enum RenderMethod {
		/** Captured texture with a balanced, always-on projective mesh warp. */
		CLASSIC_AFFINE,
		/** Captured texture with an intentionally strong projective mesh warp. */
		CAPTURED_MESH,
		/** Captured texture drawn on a real plane in the rendered world. */
		WORLD_SPACE_TEXTURE
	}

	/**
	 * A broad, always-present panel-edge band makes the active renderer
	 * unmistakable in screenshots and while switching methods. Its pixels enter
	 * the same capture texture as the HUD in Methods 2 and 3, rather than being
	 * drawn as an unrelated overlay.
	 */
	int modeIndicatorColor() {
		return switch (renderMethod) {
			case CLASSIC_AFFINE -> 0xE038C172; // green: balanced forced mesh warp
			case CAPTURED_MESH -> 0xE0469AEF; // blue: strong forced mesh warp
			case WORLD_SPACE_TEXTURE -> 0xE0EF5350; // red: physical world texture
		};
	}

	// Player-relative placement in block units. The default is in front of the
	// player at waist height rather than fixed in screen/camera space.
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	public double distance = 1.25;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	public double planeWidth = 1.45;

	/**
	 * Legacy fields remain in old JSON files but are no longer exposed. The
	 * virtual plane is always present now; migration translates placement into
	 * the explicit X/Y/Z controls below instead of keeping a look-down reveal.
	 */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public double height = 0.85;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public boolean revealWhenLookingDown = false;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int revealStartPitch = 18;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int revealFullPitch = 48;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int hiddenBelowScreenPixels = 105;

	/** Player-local placement: +X right, +Y up, and +Z forward. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	public double virtualOffsetX = 0.0;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	public double virtualOffsetY = -0.72;

	/** Fine adjustment on the same left-to-right axis as the primary map tilt. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -45, max = 45)
	public int virtualPitch = 0;

	/**
	 * Retained exclusively to read old JSON. Visibility is now controlled by
	 * the physical panel's depth and viewport intersection, never by an angle.
	 */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int minimumLookDownPitch = 0;

	/** Retained only so v1.2 config files still load; it is no longer read. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public boolean virtualTiltWithLook = false;

	/**
	 * Main map-surface tilt around the panel's left-to-right axis. A near-90°
	 * value makes a flat Minecraft-map-like surface readable from above, while
	 * still allowing a deliberate angled-paper view when reduced.
	 */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 5, max = 89)
	public int virtualFaceOnLookDownPitch = 85;

	/** Retained for v1.4 JSON compatibility; fixed-plane perspective ignores it. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int virtualHorizonPerspectivePitch = 80;

	/** Turn around the player-local/world-up axis. Positive values turn right. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -80, max = 80)
	public int virtualYaw = 0;

	/**
	 * Roll around the panel normal. Positive values raise the panel's right
	 * edge; keep zero for a normal readable desk surface.
	 */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -45, max = 45)
	public int virtualRoll = 0;

	/** These controls affect only Render Method: World-Space Texture. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	public WorldSpaceAnchor worldSpaceAnchor = WorldSpaceAnchor.CAMERA_YAW;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Tooltip
	public boolean worldSpaceOccludeBehindWorld = true;

	public enum WorldSpaceAnchor {
		/** Keep the plane in front as the camera turns horizontally. */
		CAMERA_YAW,
		/** Keep the plane at a heading fixed to the player body. */
		PLAYER_BODY
	}

	// Legacy alternatives remain readable from JSON but are not part of the
	// supported hologram model. The migration selects CAMERA_YAW.
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public VirtualAnchorMode virtualAnchorMode = VirtualAnchorMode.CAMERA_YAW;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public double virtualWorldParallaxStrength = 0.35;

	public enum VirtualAnchorMode {
		CAMERA_YAW,
		PLAYER_BODY,
		VIEW_LOCKED,
		WORLD_LIKE
	}

	/**
	 * Legacy affine-plane fields kept only to read existing JSON files. The
	 * virtual plane and captured mesh supersede them; hiding them prevents two
	 * competing placement models in Mod Menu.
	 */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public boolean lookDownPlaneTilt = false;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int planeFaceOnPitch = 72;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int planeHorizonHeightPercent = 18;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public boolean taperBackingPlate = true;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int planeHorizonFarEdgeWidthPercent = 42;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public boolean projectiveIconScaling = true;

	/**
	 * Legacy switch retained to migrate pre-v1.5 JSON files. Render Method is
	 * now the only visible selector and any failure returns to Classic Affine.
	 */
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	public boolean experimentalCaptureWarp = false;

	/** Reserved for a later cylindrical mesh mode; normal hologram mode is flat. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int experimentalCaptureCurvaturePercent = 0;

	// Legacy motion fields retained for saved configurations. The supported
	// Camera Yaw hologram uses the player's current view directly.
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public double sway = 0.35;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int swayResponseMs = 85;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public boolean rotateWithSway = true;

	/**
	 * Compatibility fallback for a companion status-bar mod. Off by default so
	 * health, armor, food, and air travel with the spatial panel in every method.
	 * Turn it on only if a companion mod renders detached duplicate decorations.
	 */
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Tooltip
	public boolean preserveCompanionStatusLayout = false;

	@ConfigEntry.Category("contents")
	@ConfigEntry.Gui.Tooltip
	public boolean showHotbar = true;

	@ConfigEntry.Category("contents")
	@ConfigEntry.Gui.Tooltip
	public boolean showBars = true;

	@ConfigEntry.Category("contents")
	@ConfigEntry.Gui.Tooltip
	public boolean showXp = true;

	@ConfigEntry.Category("contents")
	@ConfigEntry.Gui.Tooltip
	public boolean showMountBars = true;

	@ConfigEntry.Category("contents")
	@ConfigEntry.Gui.Tooltip
	public boolean showHeldItemName = true;

	// AutoConfig reflects fields when constructing Cloth Config. These are runtime
	// singletons, never settings; excluding them prevents a config-screen error.
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	private static SpatialHudConfig instance;
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	private static boolean registered;

	/**
	 * Converts the four transient guide anchors into wrapped, read-only Cloth
	 * Config text entries. This avoids fake booleans or editable strings in a
	 * tab whose only job is explaining the renderer choices.
	 */
	private static void registerMethodGuideTextProvider() {
		AutoConfigClient.getGuiRegistry(SpatialHudConfig.class).registerAnnotationProvider(
				(i18n, field, config, defaults, registry) -> {
					MethodGuideText card = field.getAnnotation(MethodGuideText.class);
					ConfigEntryBuilder entries = ConfigEntryBuilder.create();
					return List.<AbstractConfigListEntry>of(
							entries.startTextDescription(Component.translatable(i18n + ".title"))
									.setColor(card.color())
									.build(),
							entries.startTextDescription(Component.translatable(i18n + ".body"))
									.build());
				},
				MethodGuideText.class);
	}

	/** Register once early in client startup, then load the saved configuration. */
	public static SpatialHudConfig registerAndLoad() {
		if (!registered) {
			AutoConfig.register(SpatialHudConfig.class, GsonConfigSerializer::new);
			registerMethodGuideTextProvider();
			registered = true;
			instance = AutoConfig.getConfigHolder(SpatialHudConfig.class).getConfig();
			migrateV03Defaults(instance);
		}
		return instance;
	}

	public static SpatialHudConfig get() {
		return registered ? instance : registerAndLoad();
	}

	public static void save() {
		if (registered) {
			AutoConfig.getConfigHolder(SpatialHudConfig.class).save();
		}
	}

	/**
	 * Perspective capture is deliberately mandatory for every method. Method 1
	 * and Method 2 use two clearly different mesh-warp strengths; Method 3
	 * presents the same completed source texture on a real world plane.
	 */
	boolean capturesTexture() {
		return true;
	}

	/**
	 * Method 1 is still the more balanced green presentation, but it is no
	 * longer a flat affine fallback. Method 2 deliberately exaggerates the
	 * finite-plane projection so a mode change is obvious at a glance.
	 */
	float meshWarpStrength() {
		return renderMethod == RenderMethod.CLASSIC_AFFINE ? 1.25f : 2.10f;
	}

	/** Whether the captured texture is presented by the real world renderer. */
	boolean usesWorldSpaceTexture() {
		return renderMethod == RenderMethod.WORLD_SPACE_TEXTURE;
	}

	/** Migrates legacy JSON fields to the current named rendering methods. */
	private static void migrateV03Defaults(SpatialHudConfig cfg) {
		if (cfg.configVersion >= 18) {
			return;
		}

		if (cfg.configVersion < 3) {
			// v0.4 placement remains intact; only the new deliberate-reveal
			// defaults are added. This keeps a player's placement tuning intact.
			cfg.revealWhenLookingDown = true;
			cfg.revealStartPitch = 18;
			cfg.revealFullPitch = 48;
			cfg.hiddenBelowScreenPixels = 105;
			cfg.onlyDuringGameplay = true;
		}

		if (cfg.configVersion < 4) {
			// This is deliberately narrow and applies only when a corresponding
			// companion mod is actually loaded.
			cfg.preserveCompanionStatusLayout = true;
		}

		if (cfg.configVersion < 5) {
			cfg.lookDownPlaneTilt = true;
			cfg.planeFaceOnPitch = 65;
			cfg.planeHorizonHeightPercent = 18;
		}

		if (cfg.configVersion < 6) {
			cfg.taperBackingPlate = true;
			cfg.planeHorizonFarEdgeWidthPercent = 48;
		}

		if (cfg.configVersion < 7) {
			// Only retune the old shipped defaults. A player who already changed
			// either value keeps their deliberate placement preference.
			if (cfg.planeFaceOnPitch == 65 && cfg.planeHorizonFarEdgeWidthPercent == 48) {
				cfg.planeFaceOnPitch = 72;
				cfg.planeHorizonFarEdgeWidthPercent = 42;
			}
			cfg.projectiveIconScaling = true;
		}

		if (cfg.configVersion < 8) {
			// Never silently opt an existing installation into renderer hooks. The
			// isolated capture path remains a deliberate experimental choice.
			cfg.experimentalCaptureWarp = false;
			cfg.experimentalCaptureCurvaturePercent = 0;
		}

		if (cfg.configVersion < 9) {
			// v1.1 retires the look-down-only presentation in favour of an
			// always-visible virtual plane. Preserve a player's old vertical
			// placement in the new +Y-up coordinate system as closely as possible.
			cfg.revealWhenLookingDown = false;
			cfg.virtualOffsetX = 0.0;
			cfg.virtualOffsetY = -Math.max(0.05, Math.min(2.0, cfg.height * 0.5));
			cfg.virtualPitch = 48;
			cfg.virtualYaw = 0;
			cfg.virtualAnchorMode = VirtualAnchorMode.VIEW_LOCKED;
			cfg.virtualWorldParallaxStrength = 0.35;
		}

		if (cfg.configVersion < 10) {
			// The first virtual-plane release used a static pitch, which made the
			// mesh look like a moved 2D card. This is superseded by the player-body
			// model below, but keep the fields valid while migrating older JSON.
			cfg.virtualPitch = 0;
			cfg.virtualTiltWithLook = false;
			cfg.virtualFaceOnLookDownPitch = 30;
		}

		if (cfg.configVersion < 11) {
			// v1.3 gives the hologram a real player-local pose. Preserve a player's
			// deliberate X/Y/Z tuning where possible, but move untouched v1.2
			// defaults to a practical waist-height location in front of the body.
			boolean priorDefaults = Math.abs(cfg.distance - 1.75) < 0.02
					&& Math.abs(cfg.virtualOffsetX) < 0.02
					&& Math.abs(cfg.virtualOffsetY + 0.42) < 0.02;
			if (priorDefaults) {
				cfg.distance = 1.25;
				cfg.virtualOffsetX = 0.0;
				cfg.virtualOffsetY = -0.72;
			}
			cfg.virtualPitch = 0;
			cfg.virtualTiltWithLook = false;
			cfg.virtualFaceOnLookDownPitch = 30;
			cfg.virtualAnchorMode = VirtualAnchorMode.PLAYER_BODY;
		}

		if (cfg.configVersion < 12) {
			// The HUD should remain directly in front while the player looks left
			// or right, without inheriting body-turn lag.
			cfg.virtualAnchorMode = VirtualAnchorMode.CAMERA_YAW;
		}

		if (cfg.configVersion < 13) {
			// Camera-yaw anchoring keeps the location stable horizontally, while
			// this value restores the strong mesh taper as look pitch changes.
			cfg.virtualHorizonPerspectivePitch = 80;
		}

		if (cfg.configVersion < 14) {
			// v1.4 supports one camera-yaw hologram model. Old anchor selections
			// are retained only as ignored JSON values, preventing competing pose
			// rules from silently selecting a different renderer path.
			cfg.virtualAnchorMode = VirtualAnchorMode.CAMERA_YAW;
		}

		if (cfg.configVersion < 15) {
			// Preserve an explicit earlier capture choice. New installations stay on
			// the original stable affine path until the player picks a method.
			cfg.renderMethod = cfg.experimentalCaptureWarp
					? RenderMethod.CAPTURED_MESH
					: RenderMethod.CLASSIC_AFFINE;
			cfg.worldSpaceAnchor = WorldSpaceAnchor.CAMERA_YAW;
			cfg.worldSpaceOccludeBehindWorld = true;
		}

		if (cfg.configVersion < 16) {
			// v1.6 restored the fixed physical surface. Keep this old migration so
			// files that skip directly from an earlier version remain well formed.
			boolean formerDefaultTilt = cfg.virtualFaceOnLookDownPitch == 30
					&& cfg.virtualPitch == 0 && cfg.virtualYaw == 0;
			if (formerDefaultTilt) {
				cfg.virtualFaceOnLookDownPitch = 45;
			}
			cfg.virtualRoll = 0;
		}

		if (cfg.configVersion < 17) {
			// v1.7 replaces the hard look-down gate with actual viewport/depth
			// culling, makes the default a flat map-like surface, and carries all
			// status roots with Method 1 unless a user explicitly needs the legacy
			// companion-mod compatibility fallback.
			boolean priorShippedDefault = cfg.virtualFaceOnLookDownPitch == 45
					&& cfg.virtualPitch == 0 && cfg.virtualYaw == 0 && cfg.virtualRoll == 0;
			if (priorShippedDefault) {
				cfg.virtualFaceOnLookDownPitch = 85;
			}
			cfg.minimumLookDownPitch = 0;
			cfg.preserveCompanionStatusLayout = false;
		}

		if (cfg.configVersion < 18) {
			// v1.8 makes real texture warping the non-optional baseline. Older
			// files began on the legacy affine path, which is why a method choice
			// could appear to do nothing in a heavily modded HUD stack. Start at
			// the unmistakably stronger blue mesh; users can still pick green or
			// red explicitly afterwards.
			cfg.renderMethod = RenderMethod.CAPTURED_MESH;
			cfg.experimentalCaptureWarp = true;
		}

		cfg.configVersion = 18;
		save();
	}

	/** Per-element visibility inside spatial mode. */
	boolean showElement(Identifier id) {
		if (id.equals(VanillaHudElements.HOTBAR)
				|| id.equals(VanillaHudElements.SPECTATOR_MENU)
				|| id.equals(VanillaHudElements.SPECTATOR_TOOLTIP)) {
			return showHotbar;
		}
		if (id.equals(VanillaHudElements.MOUNT_HEALTH)) {
			return showMountBars;
		}
		if (id.equals(VanillaHudElements.INFO_BAR) || id.equals(VanillaHudElements.EXPERIENCE_LEVEL)) {
			return showXp;
		}
		if (id.equals(VanillaHudElements.HELD_ITEM_TOOLTIP)) {
			return showHeldItemName;
		}
		return showBars; // armor / health / food / air
	}
}
