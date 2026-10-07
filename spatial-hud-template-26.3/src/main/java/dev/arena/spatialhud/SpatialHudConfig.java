package dev.arena.spatialhud;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import net.fabricmc.loader.api.FabricLoader;

import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Dead-simple JSON config at config/spatialhud.json.
 * Hand-edit while the game is closed; values are live-relevant on next launch.
 * (v2 can grow a Cloth Config screen — the fields map 1:1.)
 */
public class SpatialHudConfig {

    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static SpatialHudConfig INSTANCE = new SpatialHudConfig();

    /** Master switch (also toggled in-game with H). */
    public boolean enabled = true;

    /** Distance of the plane from the camera, in blocks. */
    public double distance = 1.1;

    /** World-space width of the full GUI plane, in blocks. */
    public double planeWidth = 1.9;

    /** Height of the plane's BOTTOM edge below eye level, in blocks. */
    public double height = 0.35;

    /** Backward lean of the panel, in degrees (0 = upright, 15 = lectern-ish). */
    public double tiltDegrees = 14.0;

    /** How much the panel lags behind fast camera turns. 0 = glued, 1 = full sway. */
    public double sway = 0.6;

    /** Draw a translucent dark backing panel behind the elements. */
    public boolean showPanel = true;

    /** Element toggles. */
    public boolean showHotbar = true;
    public boolean showBars = true;       // hearts, hunger, armor, air bubbles
    public boolean showXp = true;         // XP bar + level number
    public boolean showMountBars = true;  // ghast/boat/horse jump & health bars
    public boolean showHeldItemName = true;

    public static SpatialHudConfig get() {
        return INSTANCE;
    }

    public static Path path() {
        return FabricLoader.getInstance().getConfigDir().resolve("spatialhud.json");
    }

    public static void load() {
        try {
            if (Files.exists(path())) {
                INSTANCE = GSON.fromJson(Files.readString(path()), SpatialHudConfig.class);
            } else {
                save();
            }
        } catch (Exception e) {
            SpatialHud.LOGGER.warn("Could not load config, using defaults", e);
            INSTANCE = new SpatialHudConfig();
        }
    }

    public static void save() {
        try {
            Files.createDirectories(path().getParent());
            Files.writeString(path(), GSON.toJson(INSTANCE));
        } catch (Exception e) {
            SpatialHud.LOGGER.warn("Could not save config", e);
        }
    }
}
