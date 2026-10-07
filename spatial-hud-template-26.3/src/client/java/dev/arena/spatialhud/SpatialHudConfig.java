package dev.arena.spatialhud;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.resources.Identifier;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Config, persisted to config/spatialhud.json with Gson.
 */
public final class SpatialHudConfig {
	public boolean enabled = true;

	/** Distance of the plane from the eye, in blocks. */
	public double distance = 1.1;
	/** Width of the plane, in blocks (182 gui-pixel strip maps onto this). */
	public double planeWidth = 1.9;
	/** How far below the view center the plane floats, in blocks. */
	public double height = 0.35;
	/** Strength of the look-lag sway (0 = rigid, 1 = pronounced). */
	public double sway = 0.6;
	/** Reserved for a future tilt effect (26.x gui poses are 2D affine). */
	public double tiltDegrees = 14;

	public boolean showPanel = true;
	public boolean showHotbar = true;
	public boolean showBars = true;
	public boolean showXp = true;
	public boolean showMountBars = true;
	public boolean showHeldItemName = true;

	private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
	private static SpatialHudConfig instance;

	public static SpatialHudConfig get() {
		if (instance == null) {
			load();
		}
		return instance;
	}

	public static SpatialHudConfig load() {
		Path path = configPath();
		if (Files.exists(path)) {
			try {
				instance = GSON.fromJson(Files.readString(path), SpatialHudConfig.class);
			} catch (Exception e) {
				SpatialHud.LOGGER.warn("Could not read spatialhud.json, using defaults", e);
			}
		}
		if (instance == null) {
			instance = new SpatialHudConfig();
		}
		return instance;
	}

	public static void save() {
		if (instance == null) {
			return;
		}
		try {
			Files.createDirectories(configPath().getParent());
			Files.writeString(configPath(), GSON.toJson(instance));
		} catch (IOException e) {
			SpatialHud.LOGGER.warn("Could not save spatialhud.json", e);
		}
	}

	private static Path configPath() {
		return FabricLoader.getInstance().getConfigDir().resolve("spatialhud.json");
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
