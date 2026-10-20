package dev.arena.spatialhud.mixin;

import dev.arena.spatialhud.SpatialHud;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.hud.InGameHud;
import net.minecraft.client.render.RenderTickCounter;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Suppresses the flat on-screen rendering of the bottom HUD strip while the
 * spatial plane is active. Everything else (crosshair, chat, scoreboards,
 * potion icons, debug screen) keeps rendering normally.
 *
 * NOTE: if a signature below mismatches on your mappings, the game will
 * crash at launch with a mixin apply error naming the exact method —
 * that's VERIFY POINT 1 from the README, tell me and I'll fix it.
 */
@Mixin(InGameHud.class)
public abstract class InGameHudMixin {

    @Inject(method = "renderHotbar", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressHotbar(DrawContext context, RenderTickCounter tickCounter, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderStatusBars", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressStatusBars(DrawContext context, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderExperienceBar", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressExperienceBar(DrawContext context, int x, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderMountHealth", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressMountHealth(DrawContext context, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderMountJumpBar", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressMountJumpBar(DrawContext context, int x, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderHeldItemTooltip", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressHeldItemTooltip(DrawContext context, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }
}
