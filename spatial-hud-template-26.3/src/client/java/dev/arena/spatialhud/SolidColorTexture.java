package dev.arena.spatialhud;

import com.mojang.blaze3d.pipeline.TextureTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.renderpearl.api.GpuFormat;
import com.mojang.renderpearl.api.textures.GpuTextureView;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.texture.AbstractTexture;
import net.minecraft.resources.Identifier;
import org.joml.Vector4f;

/**
 * A 1x1 white texture under its own identifier. The see-through text render
 * type samples a texture, so a solid colour is drawn as a white texel tinted by
 * the vertex colour. That gives an unfilled-depth solid fill for the open mode.
 *
 * <p>Created once, on the render thread, before the first frame that uses it.
 * {@link #ensure()} throws if the GPU cannot make it; the caller then falls back.</p>
 */
final class SolidColorTexture extends AbstractTexture {
	static final Identifier ID = Identifier.fromNamespaceAndPath("spatialhud", "solid_white");

	private static TextureTarget target;

	private SolidColorTexture() {
	}

	/**
	 * Creates and registers the texture the first time it is called. Later calls
	 * return at once. Must run on the render thread, outside a render pass.
	 */
	static void ensure() {
		if (target != null) {
			return;
		}
		TextureTarget created = new TextureTarget("Spatial HUD solid white", 1, 1, GpuFormat.RGBA8_UNORM, null);
		var colour = created.getColorTexture();
		if (colour == null) {
			throw new IllegalStateException("the white texture has no colour attachment");
		}
		RenderSystem.getDevice().createCommandEncoder().clearColorTexture(colour, new Vector4f(1.0f, 1.0f, 1.0f, 1.0f));
		target = created;
		Minecraft.getInstance().getTextureManager().register(ID, new SolidColorTexture());
	}

	@Override
	public GpuTextureView getTextureView() {
		return target.getColorTextureView();
	}
}
