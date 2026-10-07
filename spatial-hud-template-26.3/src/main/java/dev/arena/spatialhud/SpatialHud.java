package dev.arena.spatialhud;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.option.KeyBinding;
import net.minecraft.client.util.InputUtil;
import net.minecraft.text.Text;
import org.lwjgl.glfw.GLFW;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class SpatialHud implements ClientModInitializer {

    public static final String MOD_ID = "spatialhud";
    public static final Logger LOGGER = LoggerFactory.getLogger("SpatialHud");

    private static KeyBinding toggleKey;

    @Override
    public void onInitializeClient() {
        SpatialHudConfig.load();

        toggleKey = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.spatialhud.toggle",
                InputUtil.Type.KEYSYM,
                GLFW.GLFW_KEY_H,
                "category.spatialhud"
        ));

        ClientTickEvents.END_CLIENT_TICK.register(client -> {
            while (toggleKey.wasPressed()) {
                SpatialHudConfig cfg = SpatialHudConfig.get();
                cfg.enabled = !cfg.enabled;
                SpatialHudConfig.save();
                if (client.player != null) {
                    client.player.sendMessage(
                            Text.literal("Spatial HUD: " + (cfg.enabled ? "ON" : "OFF")), true);
                }
            }
        });

        HudPlaneRenderer.register();
        LOGGER.info("Spatial HUD loaded. Press H in-game to toggle.");
    }

    /**
     * Decides whether the flat vanilla HUD bottom strip should be suppressed.
     * We only steal the HUD while actually playing: not in menus, not with GUIs
     * open (so the normal flat HUD shows behind inventories like vanilla),
     * not when the HUD is hidden with F1.
     */
    public static boolean shouldStealHud() {
        MinecraftClient client = MinecraftClient.getInstance();
        SpatialHudConfig cfg = SpatialHudConfig.get();
        return cfg.enabled
                && client.world != null
                && client.player != null
                && !client.options.hudHidden
                && client.currentScreen == null;
    }
}
