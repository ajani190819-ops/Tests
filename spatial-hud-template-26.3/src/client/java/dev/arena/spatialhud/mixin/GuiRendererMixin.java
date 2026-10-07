package dev.arena.spatialhud.mixin;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import dev.arena.spatialhud.SpatialHudWorldRenderer;
import net.minecraft.client.gui.render.GuiRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyArg;

/** Sends only our isolated GUI renderer to the Spatial HUD texture target. */
@Mixin(GuiRenderer.class)
public abstract class GuiRendererMixin {
	@ModifyArg(
			method = "draw",
			at = @At(value = "INVOKE", target =
					"Lnet/minecraft/client/gui/render/GuiRenderer;executeDrawRange(Ljava/util/function/Supplier;Lcom/mojang/blaze3d/pipeline/RenderTarget;Lcom/mojang/renderpearl/api/buffers/GpuBufferSlice;II)V"),
			index = 1,
			require = 0)
	private RenderTarget spatialhud$redirectIsolatedHud(RenderTarget original) {
		SpatialHudWorldRenderer renderer = SpatialHudWorldRenderer.get();
		return renderer.isIsolatedRenderer(this) ? renderer.getTargetOr(original) : original;
	}
}
