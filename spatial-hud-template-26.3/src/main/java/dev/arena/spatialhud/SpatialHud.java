package dev.arena.spatialhud;

import com.mojang.blaze3d.platform.InputConstants;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.network.chat.Component;
import org.lwjgl.glfw.GLFW;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Spatial HUD — mojmap edition.
 * (MC 26.x is the Mojang-mappings era; all MC class names here are official
 * Mojang names: Minecraft, KeyMapping, GuiGraphics, Gui, DeltaTracker, ...)
 */
public class SpatialHud implements ClientModInitializer {

    public static final String MOD_ID = "spatialhud";
    public static final Logger LOGGER = LoggerFactory.getLogger("SpatialHud");

    private static KeyMapping toggleKey;

    @Override
    public void onInitializeClient() {
        SpatialHudConfig.load();

        toggleKey = KeyBindingHelper.registerKeyBinding(new KeyMapping(
                "key.spatialhud.toggle",
                InputConstants.Type.KEYSYM,
                GLFW.GLFW_KEY_H,
                "category.spatialhud"
        ));

        ClientTickEvents.END_CLIENT_TICK.register(minecraft -> {
            while (toggleKey.consumeClick()) {
                SpatialHudConfig cfg = SpatialHudConfig.get();
                cfg.enabled = !cfg.enabled;
                SpatialHudConfig.save();
                if (minecraft.player != null) {
                    minecraft.player.displayClientMessage(
                            Component.literal("Spatial HUD: " + (cfg.enabled ? "ON" : "OFF")), true);
                }
            }
        });

        HudPlaneRenderer.register();
        LOGGER.info("Spatial HUD loaded. Press H in-game to toggle.");
    }

    /**
     * Whether the flat vanilla HUD bottom strip should be suppressed.
     * Only while actually playing: not in menus, not with screens open,
     * not when the HUD is hidden with F1.
     */
    public static boolean shouldStealHud() {
        Minecraft minecraft = Minecraft.getInstance();
        SpatialHudConfig cfg = SpatialHudConfig.get();
        return cfg.enabled
                && minecraft.level != null
                && minecraft.player != null
                && !minecraft.options.hideGui
                && minecraft.screen == null;
    }
}
