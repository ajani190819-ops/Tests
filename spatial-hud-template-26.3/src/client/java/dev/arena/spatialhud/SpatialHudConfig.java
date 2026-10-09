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
	// registerAndLoad writes it as 20 after checking the values.
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
	@MethodGuideText(color = 0xFFC75CFF)
	public transient String guidePolygonTest = "";

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
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	public boolean showPanel = true;

	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Tooltip
	public boolean onlyDuringGameplay = true;

	/**
	 * Method 3 selector (3 = real world-space panel, 4 = purple panel). Hidden
	 * while Method 3 is shelved: the purple panel is always the active method, and
	 * this value is ignored. Kept so old files still load.
	 */
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.BoundedDiscrete(min = 3, max = 4)
	public int renderModePicker = 3;

	/**
	 * Legacy JSON/keybinding storage. The slider above is the visible control;
	 * this enum is kept so existing files and F8/F9 stay in step with it.
	 */
	@ConfigEntry.Category("setup")
	@ConfigEntry.Gui.Excluded
	public RenderMethod renderMethod = RenderMethod.WORLD_SPACE_TEXTURE;

	public enum RenderMethod {
		/** Method 3: the captured HUD on a real, flat, client-side world panel. */
		WORLD_SPACE_TEXTURE,
		/** Method 4: the purple horizontal panel, anchored to the player's feet. */
		POLYGON_TEST
	}

	/**
	 * Where Method 4 places the panel. Waist uses the distance and height below and
	 * the body heading setting. Face and the two custom presets are locked to the
	 * camera or the feet, as their own settings say. Each preset keeps the wiggle.
	 */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public PanelPreset horizontalPanelPreset = PanelPreset.WAIST;

	/** Purple Method 4 placement. Waist preset: a flat sheet anchored to your feet and body heading. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelDistance = 1.25;

	/** Height of the Waist panel above your feet. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelHeight = 0.9;

	/** Face preset: how far in front of the camera the panel sits, in blocks. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelFaceDistance = 0.6;

	/** Face preset: height of the panel relative to the camera, in blocks. Negative is below the camera. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelFaceHeight = -0.3;

	/** Custom preset 1: distance in front of its anchor, in blocks. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelCustomOneDistance = 1.0;

	/** Custom preset 1: height relative to its anchor, in blocks. For feet anchors this is above your feet. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelCustomOneHeight = 0.6;

	/** Custom preset 1: anchor the panel to the camera instead of your feet. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelCustomOneAttachToCamera = false;

	/** Custom preset 2: distance in front of its anchor, in blocks. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelCustomTwoDistance = 1.25;

	/** Custom preset 2: height relative to its anchor, in blocks. For feet anchors this is above your feet. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public double horizontalPanelCustomTwoHeight = 0.9;

	/** Custom preset 2: anchor the panel to the camera instead of your feet. */
	@ConfigEntry.Category("presets")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelCustomTwoAttachToCamera = false;

	/** Width of the purple panel, in blocks. Also sets the Method 4 panel's width. */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public double planeWidth = 1.45;


	/**
	 * Which heading Method 4 turns with. Player Body keeps the panel at body
	 * heading, so turning your head does not move it. Camera Yaw turns the panel
	 * with your horizontal view. Method 3 has its own setting above.
	 */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public HorizontalPanelAnchor horizontalPanelAnchor = HorizontalPanelAnchor.PLAYER_BODY;

	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 20, max = 90)
	public int horizontalPanelAngle = 90;

	/**
	 * Curves the purple panel around you, like a curved gaming monitor. 0 keeps it
	 * flat. The panel keeps its width along the curve, so higher values shorten its
	 * straight-line span. Applies to every preset.
	 */
	@ConfigEntry.Category("panel")
	@ConfigEntry.BoundedDiscrete(min = 0, max = 120)
	@ConfigEntry.Gui.Tooltip
	public int horizontalPanelCurveDegrees = 0;

	/**
	 * Over-the-shoulder cameras sit beside the player, so the body can hide the
	 * feet-anchored panel. On, the panel moves sideways by the camera's offset
	 * from your eye, so the camera has a clear view of it. Has no effect when the
	 * camera sits at your eye, and no effect on the camera-anchored presets.
	 */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelFollowShoulderCamera = true;

	/**
	 * Whether Method 4 draws its purple fill. Off leaves only the white border
	 * ring, so the panel's outline can be seen against the world without the fill.
	 */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelFill = true;

	/** Whether Method 4 draws the white border ring. Off leaves just the HUD picture (and the fill, if on). */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelBorder = true;

	/** Makes the purple fill run under the border ring, so no purple line shows at the ring's edge. */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelHideEdges = true;

	/**
	 * Whether blocks and entities between you and the Method 4 panel hide it.
	 * Off draws the panel over everything, and also keeps water and other
	 * translucent world surfaces showing through it correctly.
	 */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelOcclusion = true;

	/**
	 * In third person, the panel ignores blocks and mobs, including your own body,
	 * so it stays visible from behind you. Applies only while occlusion is on.
	 * Note: this is broader than the body alone. The depth buffer cannot exempt
	 * just the player's model.
	 */
	@ConfigEntry.Category("panel")
	@ConfigEntry.Gui.Tooltip
	public boolean horizontalPanelThirdPersonException = true;

	/**
	 * Lets the Method 4 panel lag behind your movement and catch up, instead of
	 * snapping. Only the parts ticked below move with a lag.
	 */
	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public boolean panelWiggle = true;

	/**
	 * How long the heading and height take to catch up, in seconds. About 63% of
	 * the way in one catch-up time.
	 */
	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public double panelWiggleSeconds = 0.2;

	/** How long the position takes to catch up, in seconds. Set separately, so position can lag more or less. */
	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public double panelWigglePositionSeconds = 0.1;

	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public boolean panelWiggleHeading = true;

	/**
	 * How much of the heading lag is shown, as a percentage. 100 shows the full
	 * lag; 0 keeps the panel's turn locked to your body. The lag is scaled, not
	 * re-timed, so it still never overshoots.
	 */
	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 0, max = 100)
	public int panelWiggleHeadingStrength = 100;

	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public boolean panelWigglePosition = false;

	/**
	 * How much of the position (and height, when ticked) lag is shown, as a
	 * percentage. 50 halves how far the panel trails behind you.
	 */
	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 0, max = 100)
	public int panelWigglePositionStrength = 50;

	/**
	 * The most the position lag may pull the panel away from its true place, in
	 * blocks. Larger lag is clamped to this distance, so sprinting cannot pull
	 * the panel far away.
	 */
	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public double panelWigglePositionMaxBlocks = 0.25;

	@ConfigEntry.Category("wiggle")
	@ConfigEntry.Gui.Tooltip
	public boolean panelWiggleHeight = false;

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

	/** Hotbar Slot Cycling's side display (the cycle slots beside the hotbar). Method 4 widens its picture to hold it. */
	@ConfigEntry.Category("contents")
	@ConfigEntry.Gui.Tooltip
	public boolean showSlotCycling = true;

	/**
	 * Moves the HUD picture inside the purple panel, in percent of the panel's
	 * height. Negative and positive values move it in opposite directions. The
	 * default is -4. -3 was the position confirmed in game.
	 */
	@ConfigEntry.Category("contents")
	@ConfigEntry.BoundedDiscrete(min = -10, max = 10)
	@ConfigEntry.Gui.Tooltip
	public int horizontalPanelPictureOffset = -4;

	/**
	 * Half-width, in GUI pixels from the centre of the hotbar, of the band the
	 * purple panel samples while Slot Cycling is shown. The cycle slots sit about
	 * 91 to 167 pixels out, so 172 holds them all. Lower it to narrow the panel,
	 * which cuts off the outermost cycle slots.
	 */
	@ConfigEntry.Category("contents")
	@ConfigEntry.BoundedDiscrete(min = 112, max = 200)
	@ConfigEntry.Gui.Tooltip
	public int slotCyclingHalfWidth = 172;

	// Player-relative placement in block units. The default is in front of the
	// player at waist height rather than fixed in screen/camera space.
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	public double distance = 1.25;

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
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	public double virtualOffsetX = 0.0;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	public double virtualOffsetY = -0.72;

	/** Fine adjustment on the same left-to-right axis as the primary map tilt. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
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
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 5, max = 89)
	public int virtualFaceOnLookDownPitch = 85;

	/** Retained for v1.4 JSON compatibility; fixed-plane perspective ignores it. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	public int virtualHorizonPerspectivePitch = 80;

	/** Turn around the player-local/world-up axis. Positive values turn right. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -80, max = 80)
	public int virtualYaw = 0;

	/**
	 * Roll around the panel normal. Positive values raise the panel's right
	 * edge; keep zero for a normal readable desk surface.
	 */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -45, max = 45)
	public int virtualRoll = 0;

	/** These controls affect only Render Method: World-Space Texture. */
	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	public WorldSpaceAnchor worldSpaceAnchor = WorldSpaceAnchor.CAMERA_YAW;

	@ConfigEntry.Category("positioning")
	@ConfigEntry.Gui.Excluded
	@ConfigEntry.Gui.Tooltip
	public boolean worldSpaceOccludeBehindWorld = true;

	public enum WorldSpaceAnchor {
		/** Keep the plane in front as the camera turns horizontally. */
		CAMERA_YAW,
		/** Keep the plane at a heading fixed to the player body. */
		PLAYER_BODY
	}

	/** Where Method 4 places its panel. Only the purple panel reads this. */
	public enum PanelPreset {
		/** Anchored to your feet, using the distance and height sliders. */
		WAIST,
		/** Locked to the camera's view: square to you, a set distance ahead and height above the view centre. */
		FACE,
		/** The first custom preset. */
		CUSTOM_ONE,
		/** The second custom preset. */
		CUSTOM_TWO
	}

	/** Which heading Method 4 follows. Only the purple panel reads this. */
	public enum HorizontalPanelAnchor {
		/** Turns with the camera's horizontal view. */
		CAMERA_YAW,
		/** Keeps the panel at body heading, so turning your head does not move it. */
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
	 * now the only visible selector. A capture failure shows the vanilla HUD.
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

	// Legacy corner controls from the GUI-layer warp that Method 4 used before
	// v22. They stay in the JSON so older files still load, but nothing reads them.
	@ConfigEntry.Gui.Excluded
	public boolean polygonFollowCameraPitch = true;

	@ConfigEntry.Gui.Excluded
	public int polygonTopLeftXPercent = 30;

	@ConfigEntry.Gui.Excluded
	public int polygonTopLeftYPercent = 35;

	@ConfigEntry.Gui.Excluded
	public int polygonTopRightXPercent = 70;

	@ConfigEntry.Gui.Excluded
	public int polygonTopRightYPercent = 35;

	@ConfigEntry.Gui.Excluded
	public int polygonBottomRightXPercent = 80;

	@ConfigEntry.Gui.Excluded
	public int polygonBottomRightYPercent = 70;

	@ConfigEntry.Gui.Excluded
	public int polygonBottomLeftXPercent = 20;

	@ConfigEntry.Gui.Excluded
	public int polygonBottomLeftYPercent = 70;

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

	/** Maps the visible 3–4 slider to the one active renderer. */
	RenderMethod selectedRenderMethod() {
		// The purple panel is the only active method while Method 3 is shelved.
		return RenderMethod.POLYGON_TEST;
	}

	/** Synchronizes direct key selection with the visible slider and old JSON field. */
	void selectRenderMethod(RenderMethod method) {
		renderMethod = method;
		renderModePicker = switch (method) {
			case WORLD_SPACE_TEXTURE -> 3;
			case POLYGON_TEST -> 4;
		};
	}

	/**
	 * Both panel methods present the selected lower HUD through the one private
	 * captured texture, drawn on a world-space quad. Capture is therefore always
	 * required while Spatial HUD is enabled.
	 */
	boolean capturesTexture() {
		return true;
	}

	/**
	 * Method 4: the purple horizontal panel (saved as POLYGON_TEST, a name kept
	 * so existing files still load).
	 */
	boolean usesPurplePanel() {
		return selectedRenderMethod() == RenderMethod.POLYGON_TEST;
	}

		/** Migrates legacy JSON fields to the current named rendering methods. */
	private static void migrateV03Defaults(SpatialHudConfig cfg) {
		if (cfg.configVersion >= 22) {
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
			// Any value written here is translated again by the v21 step at
			// the end of this migration.
			cfg.renderMethod = RenderMethod.WORLD_SPACE_TEXTURE;
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
		}

		if (cfg.configVersion < 18) {
			// v1.8 makes real texture warping the non-optional baseline. Older
			// files began on the legacy affine path, which is why a method choice
			// could appear to do nothing in a heavily modded HUD stack. Start at
			// the unmistakably stronger blue mesh; users can still pick green or
			// red explicitly afterwards.
			cfg.renderMethod = RenderMethod.WORLD_SPACE_TEXTURE;
			cfg.experimentalCaptureWarp = true;
		}

		if (cfg.configVersion < 19) {
			// v1.9 replaces the enum-dropdown dependency with a visible 1–4 slider.
			// Translate the already-saved enum once so the old chosen mode remains
			// selected while the purple Method 4 panel becomes available.
			// An enum name removed in v21 loads as null; the v21 step fixes it.
			if (cfg.renderMethod != null) {
				cfg.selectRenderMethod(cfg.renderMethod);
			}
		}

		if (cfg.configVersion < 20) {
			// Purple Method 4 now has one visible test surface, whose configured
			// corners respond continuously to look pitch by default.
			cfg.polygonFollowCameraPitch = true;
		}

		if (cfg.configVersion < 21) {
			// v2.0 keeps only Method 3 (real world-space panel) and Method 4
			// (purple GUI approximation). Old slider values 1 and 2 were the
			// archived affine and mesh methods, so they move to Method 3.
			cfg.renderModePicker = Math.max(3, Math.min(4, cfg.renderModePicker));
			cfg.selectRenderMethod(cfg.selectedRenderMethod());
		}

		// v22: Method 4 became a purple horizontal world panel. Its settings are
		// new fields, so older files receive the defaults automatically.
		cfg.configVersion = 22;
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
		if (id.equals(SpatialHud.SLOT_CYCLING_ELEMENT)) {
			return showSlotCycling;
		}
		return showBars; // armor / health / food / air
	}
}
