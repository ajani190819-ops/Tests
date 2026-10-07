package dev.arena.spatialhud.mixin;

import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Composites the isolated bottom HUD only after Minecraft has completed its
 * normal GuiRenderer pass. This gives the projective mesh the same settled GUI
 * projection/target state as the rest of the HUD and prevents renderer stacks
 * such as Iris from subsequently replacing the private composite. No screen is
 * cancelled, replaced, or re-extracted here.
 */
@Mixin(GameRenderer.class)
abstract class SpatialHudGameRendererMixin {
	@Inject(
			method = "render",
			at = @At(
					value = "INVOKE",
					target = "Lnet/minecraft/client/gui/render/GuiRenderer;render()V",
					shift = At.Shift.AFTER))
	private void spatialhud$drawSelectedBottomHudCapture(CallbackInfo ci) {
		dev.arena.spatialhud.ExperimentalHudCapture.renderAndComposite();
	}
}
