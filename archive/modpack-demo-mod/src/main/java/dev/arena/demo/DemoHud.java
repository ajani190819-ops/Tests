package dev.arena.demo;

import net.fabricmc.fabric.api.client.rendering.v1.HudRenderCallback;

/**
 * Draws a small FPS/frame-time readout in the corner while playing.
 *
 * This is the standard "draw on the HUD" hook from Fabric API. Note the
 * exact parameter types of the callback have changed across MC versions
 * (MatrixStack -> DrawContext, plus a tick counter) — this file is written
 * against modern yarn names; if your mappings differ, this is the line
 * your IDE will flag. That one-liner is the only "version drift" risk in
 * this whole demo.
 */
public class DemoHud {

    public static void register() {
        HudRenderCallback.EVENT.register((context, tickCounter) -> {
            if (!DemoMod.hudVisible) return;

            var client = net.minecraft.client.MinecraftClient.getInstance();
            if (client.player == null || client.getDebugHud().shouldShowDebugHud()) return;

            // getCurrentFps() exists on MinecraftClient in modern versions.
            int fps = client.getCurrentFps();

            String text = fps + " fps";
            int x = 4;
            int y = 4;

            // Black backing box + white text — the classic "mod HUD" look.
            context.fill(x - 2, y - 2,
                    x + client.textRenderer.getWidth(text) + 2, y + 11,
                    0x90000000);
            context.drawTextWithShadow(client.textRenderer,
                    net.minecraft.text.Text.literal(text), x, y, 0xFFFFFF);
        });
    }
}
