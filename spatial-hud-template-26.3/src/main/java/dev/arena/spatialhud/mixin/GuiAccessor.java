package dev.arena.spatialhud.mixin;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.Gui;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.player.PlayerRideableJumping;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

/**
 * Exposes vanilla Gui's private HUD render methods so we can re-render them
 * onto our world-space plane.
 *
 * Signatures follow modern Mojang mappings (1.21.x era):
 *  - renderHotbar(GuiGraphics, DeltaTracker)
 *  - renderPlayerHealth(GuiGraphics)
 *  - renderExperienceBar(GuiGraphics, int)
 *  - renderVehicleHealth(GuiGraphics)
 *  - renderJumpMeter(PlayerRideableJumping, GuiGraphics, int)
 *  - renderSelectedItemName(GuiGraphics)
 *
 * !! VERIFY POINT 1 !! If the game crashes AT LAUNCH with
 * "Critical injection failure" / "method not found", the 26.3 signature of
 * one of these changed — the crash names the exact method. Tell me and I'll
 * adjust.
 */
@Mixin(Gui.class)
public interface GuiAccessor {

    @Invoker("renderHotbar")
    void invokeRenderHotbar(GuiGraphics guiGraphics, DeltaTracker deltaTracker);

    @Invoker("renderPlayerHealth")
    void invokeRenderPlayerHealth(GuiGraphics guiGraphics);

    @Invoker("renderExperienceBar")
    void invokeRenderExperienceBar(GuiGraphics guiGraphics, int x);

    @Invoker("renderVehicleHealth")
    void invokeRenderVehicleHealth(GuiGraphics guiGraphics);

    @Invoker("renderJumpMeter")
    void invokeRenderJumpMeter(PlayerRideableJumping rideable, GuiGraphics guiGraphics, int x);

    @Invoker("renderSelectedItemName")
    void invokeRenderSelectedItemName(GuiGraphics guiGraphics);
}
