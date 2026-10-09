package dev.arena.spatialhud.mixin;

import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.hud.InGameHud;
import net.minecraft.client.render.RenderTickCounter;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

/**
 * Exposes vanilla's private HUD render methods so we can re-render them onto
 * our world-space plane.
 *
 * !! VERIFY POINT 1 !! — these signatures are written against modern yarn
 * names (1.21.x era). If any @Invoker fails to resolve on 26.2 mappings,
 * open InGameHud in your IDE and adjust: the *method names* are stable
 * (renderHotbar, renderStatusBars, renderExperienceBar, renderMountHealth,
 * renderMountJumpBar, renderHeldItemTooltip) but a parameter may have been
 * added/removed since.
 */
@Mixin(InGameHud.class)
public interface InGameHudAccessor {

    @Invoker("renderHotbar")
    void invokeRenderHotbar(DrawContext context, RenderTickCounter tickCounter);

    @Invoker("renderStatusBars")
    void invokeRenderStatusBars(DrawContext context);

    @Invoker("renderExperienceBar")
    void invokeRenderExperienceBar(DrawContext context, int x);

    @Invoker("renderMountHealth")
    void invokeRenderMountHealth(DrawContext context);

    @Invoker("renderMountJumpBar")
    void invokeRenderMountJumpBar(DrawContext context, int x);

    @Invoker("renderHeldItemTooltip")
    void invokeRenderHeldItemTooltip(DrawContext context);
}
