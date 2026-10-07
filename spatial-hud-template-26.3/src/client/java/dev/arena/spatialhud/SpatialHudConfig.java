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
	// registerAndLoad writes it as 2 after checking the values.
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

	/**
	 * v0.3's first-release defaults made the strip roughly twice as wide as
	 * vanilla and raised it toward the centre of the screen. Only replace that
	 * exact untouched combination; deliberately customized values are left alone.
	 */
	private static void migrateV03Defaults(SpatialHudConfig cfg) {
		if (cfg.configVersion >= 2) {
			return;
		}

		boolean untouchedV03Placement = nearly(cfg.distance, 1.1)
				&& nearly(cfg.planeWidth, 1.9)
				&& nearly(cfg.height, 0.35)
				&& nearly(cfg.sway, 0.6);
		if (untouchedV03Placement) {
			cfg.distance = 1.75;
			cfg.planeWidth = 1.45;
			cfg.height = 0.85;
			cfg.sway = 0.35;
		}
		cfg.configVersion = 2;
		save();
	}

	private static boolean nearly(double value, double expected) {
		return Math.abs(value - expected) < 0.00001;
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
