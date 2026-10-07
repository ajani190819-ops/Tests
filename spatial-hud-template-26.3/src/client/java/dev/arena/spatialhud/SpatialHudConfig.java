package dev.arena.spatialhud;

import me.shedaniel.autoconfig.AutoConfig;
import me.shedaniel.autoconfig.ConfigData;
import me.shedaniel.autoconfig.annotation.Config;
import me.shedaniel.autoconfig.annotation.ConfigEntry;
import me.shedaniel.autoconfig.serializer.GsonConfigSerializer;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
import net.minecraft.resources.Identifier;

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
	@ConfigEntry.Gui.Excluded
	// Starts at 0 so a v0.3 file, which has no version field, is detected.
	// registerAndLoad writes it as 14 after checking the values.
	public int configVersion = 0;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean enabled = true;

	// Earlier affine-only settings retained solely for old JSON files.
	@ConfigEntry.Gui.Excluded
	public boolean autoScaleByFov = true;

	@ConfigEntry.Gui.Excluded
	public int fovBaseline = 70;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean showPanel = true;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean onlyDuringGameplay = true;

	// Player-relative placement in block units. The default is in front of the
	// player at waist height rather than fixed in screen/camera space.
	@ConfigEntry.Category("placement")
	@ConfigEntry.Gui.Tooltip
	public double distance = 1.25;

	@ConfigEntry.Category("placement")
	@ConfigEntry.Gui.Tooltip
	public double planeWidth = 1.45;

	/**
	 * Legacy fields remain in old JSON files but are no longer exposed. The
	 * virtual plane is always present now; migration translates placement into
	 * the explicit X/Y/Z controls below instead of keeping a look-down reveal.
	 */
	@ConfigEntry.Gui.Excluded
	public double height = 0.85;

	@ConfigEntry.Gui.Excluded
	public boolean revealWhenLookingDown = false;

	@ConfigEntry.Gui.Excluded
	public int revealStartPitch = 18;

	@ConfigEntry.Gui.Excluded
	public int revealFullPitch = 48;

	@ConfigEntry.Gui.Excluded
	public int hiddenBelowScreenPixels = 105;

	/** Player-local placement: +X right, +Y up, and +Z forward. */
	@ConfigEntry.Category("virtualPlane")
	@ConfigEntry.Gui.Tooltip
	public double virtualOffsetX = 0.0;

	@ConfigEntry.Category("virtualPlane")
	@ConfigEntry.Gui.Tooltip
	public double virtualOffsetY = -0.72;

	/** Extra plane tilt relative to the configured face-on view angle. */
	@ConfigEntry.Category("virtualPlane")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -45, max = 45)
	public int virtualPitch = 0;

	/** Retained only so v1.2 config files still load; it is no longer read. */
	@ConfigEntry.Gui.Excluded
	public boolean virtualTiltWithLook = false;

	@ConfigEntry.Category("virtualPlane")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 5, max = 80)
	public int virtualFaceOnLookDownPitch = 30;

	/** Maximum camera-relative taper while looking at the horizon. */
	@ConfigEntry.Category("virtualPlane")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 15, max = 85)
	public int virtualHorizonPerspectivePitch = 80;

	@ConfigEntry.Category("virtualPlane")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = -80, max = 80)
	public int virtualYaw = 0;

	// Legacy alternatives remain readable from JSON but are not part of the
	// supported hologram model. The migration selects CAMERA_YAW.
	@ConfigEntry.Gui.Excluded
	public VirtualAnchorMode virtualAnchorMode = VirtualAnchorMode.CAMERA_YAW;

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
	@ConfigEntry.Gui.Excluded
	public boolean lookDownPlaneTilt = false;

	@ConfigEntry.Gui.Excluded
	public int planeFaceOnPitch = 72;

	@ConfigEntry.Gui.Excluded
	public int planeHorizonHeightPercent = 18;

	@ConfigEntry.Gui.Excluded
	public boolean taperBackingPlate = true;

	@ConfigEntry.Gui.Excluded
	public int planeHorizonFarEdgeWidthPercent = 42;

	@ConfigEntry.Gui.Excluded
	public boolean projectiveIconScaling = true;

	/**
	 * Experimental capture mode is intentionally opt-in. It captures only the
	 * selected gameplay bottom-HUD roots into a private texture, then warps that
	 * completed strip as a mesh. Normal GUI renderers are never redirected.
	 * Any capture/render error immediately latches the released affine mode.
	 */
	@ConfigEntry.Category("experimentalCapture")
	@ConfigEntry.Gui.Tooltip
	public boolean experimentalCaptureWarp = false;

	/** Reserved for a later cylindrical mesh mode; normal hologram mode is flat. */
	@ConfigEntry.Gui.Excluded
	public int experimentalCaptureCurvaturePercent = 0;

	// Legacy motion fields retained for saved configurations. The supported
	// Camera Yaw hologram uses the player's current view directly.
	@ConfigEntry.Gui.Excluded
	public double sway = 0.35;

	@ConfigEntry.Gui.Excluded
	public int swayResponseMs = 85;

	@ConfigEntry.Gui.Excluded
	public boolean rotateWithSway = true;

	/**
	 * Safe affine fallback only: AppleSkin and Detail Armor Bar Reconstructed
	 * inject decoration into vanilla status-bar methods. Keep those complete
	 * roots native there to avoid detached companion pixels. Experimental
	 * capture deliberately takes the complete injected group instead.
	 */
	@ConfigEntry.Category("compatibility")
	@ConfigEntry.Gui.Tooltip
	public boolean preserveCompanionStatusLayout = true;

	@ConfigEntry.Category("visibility")
	@ConfigEntry.Gui.Tooltip
	public boolean showHotbar = true;

	@ConfigEntry.Category("visibility")
	@ConfigEntry.Gui.Tooltip
	public boolean showBars = true;

	@ConfigEntry.Category("visibility")
	@ConfigEntry.Gui.Tooltip
	public boolean showXp = true;

	@ConfigEntry.Category("visibility")
	@ConfigEntry.Gui.Tooltip
	public boolean showMountBars = true;

	@ConfigEntry.Category("visibility")
	@ConfigEntry.Gui.Tooltip
	public boolean showHeldItemName = true;

	private static SpatialHudConfig instance;
	private static boolean registered;

	/** Register once early in client startup, then load the saved configuration. */
	public static SpatialHudConfig registerAndLoad() {
		if (!registered) {
			AutoConfig.register(SpatialHudConfig.class, GsonConfigSerializer::new);
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

	/** Migrates legacy JSON fields to the single supported hologram defaults. */
	private static void migrateV03Defaults(SpatialHudConfig cfg) {
		if (cfg.configVersion >= 14) {
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

		cfg.configVersion = 14;
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
