package dev.arena.spatialhud.mixin;

import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Draws the already-isolated bottom-HUD capture immediately before the normal
 * GuiRenderer. No screen is cancelled, replaced, or re-extracted here.
 */
@Mixin(GameRenderer.class)
abstract class SpatialHudGameRendererMixin {
	@Inject(
			method = "render",
			at = @At(value = "INVOKE", target = "Lnet/minecraft/client/gui/render/GuiRenderer;render()V"))
	private void spatialhud$drawSelectedBottomHudCapture(CallbackInfo ci) {
		dev.arena.spatialhud.ExperimentalHudCapture.renderAndComposite();
	}
}
