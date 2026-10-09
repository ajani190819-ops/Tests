package dev.arena.spatialhud.mixin;

import com.mojang.blaze3d.pipeline.RenderTarget;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.state.OptionsRenderState;
import net.minecraft.client.renderer.state.level.CameraRenderState;
import net.minecraft.client.renderer.state.level.PlayerRenderState;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
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
	@Shadow
	@Final
	private RenderTarget mainRenderTarget;

	@Inject(
			method = "render",
			at = @At(
					value = "INVOKE",
					target = "Lnet/minecraft/client/gui/render/GuiRenderer;render()V",
					shift = At.Shift.AFTER))
	private void spatialhud$drawSelectedBottomHudCapture(CallbackInfo ci) {
		dev.arena.spatialhud.ExperimentalHudCapture.renderAndComposite();
		// The world-space quad was drawn during the level pass; ending its
		// buffer frame has to wait until that pass has closed, which is here.
		dev.arena.spatialhud.WorldSpaceHudRenderer.endFrame();
	}

	/**
	 * The first-person hand is drawn in render3dHud, after the world, with its own
	 * depth. The purple panel is drawn here, after the hand, when it is not
	 * occluded. Otherwise the hand would cover it.
	 */
	@Inject(method = "render3dHud", at = @At("RETURN"))
	private void spatialhud$drawPanelOverHand(CameraRenderState cameraState, PlayerRenderState playerState,
			OptionsRenderState optionsState, boolean consistentDepthRequired, CallbackInfo ci) {
		dev.arena.spatialhud.WorldSpaceHudRenderer.drawOverHand(this.mainRenderTarget, cameraState.pos);
	}

	@Inject(method = "close", at = @At("RETURN"))
	private void spatialhud$closeWorldTextureBuffer(CallbackInfo ci) {
		dev.arena.spatialhud.WorldSpaceHudRenderer.close();
	}
}
