package dev.arena.spatialhud;

import com.mojang.renderpearl.api.textures.GpuTextureView;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.texture.AbstractTexture;
import net.minecraft.resources.Identifier;

/**
 * Exposes Spatial HUD's captured bottom-HUD texture under an identifier, so a
 * standard textured render type can sample it. The texture reads the capture
 * target's current view on every use, so it always shows the latest frame.
 *
 * <p>Roadmap stage 3. Registered on first use, once the texture manager exists.</p>
 */
final class CapturedHudTexture extends AbstractTexture {
	static final Identifier ID = Identifier.fromNamespaceAndPath("spatialhud", "captured_hud");

	private static boolean registered;

	private CapturedHudTexture() {
	}

	/** Registers the texture with the texture manager. Safe to call more than once. */
	static void register() {
		if (registered) {
			return;
		}
		registered = true;
		Minecraft.getInstance().getTextureManager().register(ID, new CapturedHudTexture());
	}

	@Override
	public GpuTextureView getTextureView() {
		return ExperimentalHudCapture.worldTextureView();
	}
}
