package dev.arena.spatialhud.mixin;

import dev.arena.spatialhud.SpatialHudWorldRenderer;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Bridges Minecraft's extraction and GUI draw phases to the 3D HUD capture. */
@Mixin(GameRenderer.class)
public abstract class GameRendererMixin {
	@Inject(method = "extract", at = @At("HEAD"), require = 0)
	private void spatialhud$beginHudCapture(CallbackInfo ci) {
		SpatialHudWorldRenderer.get().beginFrame();
	}

	@Inject(method = "render", at = @At(value = "INVOKE", target =
			"Lnet/minecraft/client/gui/render/GuiRenderer;render()V"), require = 0)
	private void spatialhud$renderWorldHudBeforeGui(CallbackInfo ci) {
		SpatialHudWorldRenderer.get().renderBeforeMainGui();
	}
}
