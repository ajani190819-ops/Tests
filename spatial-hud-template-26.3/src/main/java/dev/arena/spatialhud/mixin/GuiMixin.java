package dev.arena.spatialhud.mixin;

import dev.arena.spatialhud.SpatialHud;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.Gui;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.player.PlayerRideableJumping;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Suppresses the flat on-screen rendering of the bottom HUD strip while the
 * spatial plane is active. Crosshair, chat, scoreboards, potion icons, debug
 * screen etc. keep rendering normally.
 */
@Mixin(Gui.class)
public abstract class GuiMixin {

    @Inject(method = "renderHotbar", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressHotbar(GuiGraphics guiGraphics, DeltaTracker deltaTracker, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderPlayerHealth", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressPlayerHealth(GuiGraphics guiGraphics, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderExperienceBar", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressExperienceBar(GuiGraphics guiGraphics, int x, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderVehicleHealth", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressVehicleHealth(GuiGraphics guiGraphics, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderJumpMeter", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressJumpMeter(PlayerRideableJumping rideable, GuiGraphics guiGraphics, int x, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }

    @Inject(method = "renderSelectedItemName", at = @At("HEAD"), cancellable = true)
    private void spatialhud$suppressSelectedItemName(GuiGraphics guiGraphics, CallbackInfo ci) {
        if (SpatialHud.shouldStealHud()) ci.cancel();
    }
}
