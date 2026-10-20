package dev.arena.demo;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
import net.minecraft.client.option.KeyBinding;
import net.minecraft.client.util.InputUtil;
import net.minecraft.text.Text;
import org.lwjgl.glfw.GLFW;

/**
 * Entry point for the client. Every client-side mod starts like this:
 * implement ClientModInitializer, register it under "entrypoints"."client"
 * in fabric.mod.json, and you're in the game.
 */
public class DemoMod implements ClientModInitializer {

    public static boolean hudVisible = true;
    private static KeyBinding toggleKey;

    @Override
    public void onInitializeClient() {
        // Keybinds must be registered through the helper, never 'new'ed into the registry directly.
        toggleKey = KeyBindingHelper.registerKeyBinding(new KeyBinding(
                "key.demo-fps-hud.toggle",
                InputUtil.Type.KEYSYM,
                GLFW.GLFW_KEY_J,          // default key: J
                "category.demo-fps-hud"
        ));

        // Client tick event = the client-side equivalent of a game loop hook.
        ClientTickEvents.END_CLIENT_TICK.register(client -> {
            while (toggleKey.wasPressed()) {
                hudVisible = !hudVisible;
                if (client.player != null) {
                    client.player.sendMessage(
                            Text.literal("FPS HUD: " + (hudVisible ? "ON" : "OFF")), true);
                }
            }
        });

        // The actual HUD drawing lives in DemoHud (registered there).
        DemoHud.register();
    }
}
