package dev.arena.spatialhud.mixin;

import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.state.level.CameraRenderState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

/**
 * Lets the panel call vanilla's own bob functions. Mods that add to the bob (for
 * example a camera roll) are then included, so the panel can cancel exactly what
 * the world projection received.
 */
@Mixin(GameRenderer.class)
public interface GameRendererBobAccessor {
	@Invoker("bobHurt")
	void spatialhud$bobHurt(CameraRenderState cameraState, PoseStack poseStack);

	@Invoker("bobView")
	void spatialhud$bobView(CameraRenderState cameraState, PoseStack poseStack);
}
