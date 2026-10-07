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
 * the settings appear in Mod Menu, can be opened from an unbound Controls
 * keybind, and are stored in {@code config/spatialhud.json}. The old v0.3 JSON
 * fields keep their names, so existing settings continue to load.</p>
 */
@Config(name = "spatialhud")
public final class SpatialHudConfig implements ConfigData {
	/** Incremented when a safe default migration is needed. */
	@ConfigEntry.Gui.Excluded
	// Starts at 0 so a v0.3 file, which has no version field, is detected.
	// registerAndLoad writes it as 8 after checking the values.
	public int configVersion = 0;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean enabled = true;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean autoScaleByFov = true;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 30, max = 110)
	public int fovBaseline = 70;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean showPanel = true;

	@ConfigEntry.Category("general")
	@ConfigEntry.Gui.Tooltip
	public boolean onlyDuringGameplay = true;

	// Placement — expressed in blocks to mirror Spatial GUI's first-person controls.
	@ConfigEntry.Category("placement")
	@ConfigEntry.Gui.Tooltip
	public double distance = 1.75;

	@ConfigEntry.Category("placement")
	@ConfigEntry.Gui.Tooltip
	public double planeWidth = 1.45;

	@ConfigEntry.Category("placement")
	@ConfigEntry.Gui.Tooltip
	public double height = 0.85;

	// Look-down reveal keeps the HUD out of the way until it is intentionally needed.
	@ConfigEntry.Category("lookDownReveal")
	@ConfigEntry.Gui.Tooltip
	public boolean revealWhenLookingDown = true;

	@ConfigEntry.Category("lookDownReveal")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 0, max = 89)
	public int revealStartPitch = 18;

	@ConfigEntry.Category("lookDownReveal")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 1, max = 90)
	public int revealFullPitch = 48;

	@ConfigEntry.Category("lookDownReveal")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 0, max = 240)
	public int hiddenBelowScreenPixels = 105;

	/**
	 * A safe 2.5D floor-plane illusion. The HUD API only exposes a 2D GUI pose,
	 * so this deliberately uses vertical foreshortening instead of a world/UI
	 * capture renderer. At the horizon the panel is thin; it fills out as the
	 * player looks toward its configured face-on pitch.
	 */
	@ConfigEntry.Category("planeTilt")
	@ConfigEntry.Gui.Tooltip
	public boolean lookDownPlaneTilt = true;

	@ConfigEntry.Category("planeTilt")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 20, max = 89)
	public int planeFaceOnPitch = 72;

	@ConfigEntry.Category("planeTilt")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 5, max = 100)
	public int planeHorizonHeightPercent = 18;

	/**
	 * A true projective warp needs a captured texture or a world renderer, both
	 * of which are deliberately outside this compatibility-first build. The
	 * backing plate can still taper safely with ordinary HUD rectangles, giving
	 * the panel a clear near/far edge without touching vanilla icon geometry.
	 */
	@ConfigEntry.Category("planeTilt")
	@ConfigEntry.Gui.Tooltip
	public boolean taperBackingPlate = true;

	@ConfigEntry.Category("planeTilt")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 20, max = 100)
	public int planeHorizonFarEdgeWidthPercent = 42;

	/**
	 * Applies the same near/far perspective ratio to each vanilla bottom-strip
	 * root. This is the closest safe approximation to icon warping available
	 * through Fabric's public affine HUD pose: individual roots stretch with
	 * their depth, but no framebuffer capture or global GUI hook is needed.
	 */
	@ConfigEntry.Category("planeTilt")
	@ConfigEntry.Gui.Tooltip
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

	/** Optional screen-space bow for the experimental mesh; zero is flat. */
	@ConfigEntry.Category("experimentalCapture")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 0, max = 100)
	public int experimentalCaptureCurvaturePercent = 0;

	// Motion. A time-based filter is used, so it remains smooth above 20 FPS.
	@ConfigEntry.Category("motion")
	@ConfigEntry.Gui.Tooltip
	public double sway = 0.35;

	@ConfigEntry.Category("motion")
	@ConfigEntry.Gui.Tooltip
	@ConfigEntry.BoundedDiscrete(min = 20, max = 500)
	public int swayResponseMs = 85;

	@ConfigEntry.Category("motion")
	@ConfigEntry.Gui.Tooltip
	public boolean rotateWithSway = true;

	/**
	 * AppleSkin and Detail Armor Bar Reconstructed inject decoration into the
	 * vanilla status-bar methods rather than registering standalone HUD elements.
	 * Leave those decorated roots in their native layout while revealed so the
	 * whole group stays visible and aligned. The roots are still omitted until
	 * the look-down reveal begins.
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

	/** Adds safe reveal, tilt, and companion-layout defaults to older config files. */
	private static void migrateV03Defaults(SpatialHudConfig cfg) {
		if (cfg.configVersion >= 8) {
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

		cfg.configVersion = 8;
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
