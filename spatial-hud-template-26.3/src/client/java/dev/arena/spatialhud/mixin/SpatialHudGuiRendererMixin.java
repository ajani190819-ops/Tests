package dev.arena.spatialhud.mixin;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import net.minecraft.client.gui.render.GuiRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyArg;

/**
 * Routes only Spatial HUD's private capture renderer to its private texture.
 * The normal Minecraft GuiRenderer never passes this identity check, so its
 * target—and all ordinary screens and mod GUIs—are untouched.
 */
@Mixin(GuiRenderer.class)
abstract class SpatialHudGuiRendererMixin {
	@ModifyArg(
			method = "draw",
			at = @At(
					value = "INVOKE",
					target = "Lnet/minecraft/client/gui/render/GuiRenderer;executeDrawRange(Ljava/util/function/Supplier;Lcom/mojang/blaze3d/pipeline/RenderTarget;Lcom/mojang/renderpearl/api/buffers/GpuBufferSlice;II)V"),
			index = 1)
	private RenderTarget spatialhud$routePrivateCaptureTarget(RenderTarget original) {
		return dev.arena.spatialhud.ExperimentalHudCapture.isCapturedRenderer(this)
				? dev.arena.spatialhud.ExperimentalHudCapture.captureTargetOr(original)
				: original;
	}
}
