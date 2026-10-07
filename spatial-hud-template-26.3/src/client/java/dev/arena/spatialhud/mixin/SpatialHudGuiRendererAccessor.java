package dev.arena.spatialhud.mixin;

import net.minecraft.client.gui.render.GuiRenderer;
import net.minecraft.client.gui.render.pip.PictureInPictureRenderer;
import net.minecraft.client.renderer.state.gui.pip.PictureInPictureRenderState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Mutable;
import org.spongepowered.asm.mixin.gen.Accessor;

import java.util.Map;

/**
 * Shares Minecraft's existing PiP renderer map with the selected-HUD private
 * renderer. This is needed for GUI item rendering; it does not alter the main
 * renderer's map or route any ordinary GUI into Spatial HUD.
 */
@Mixin(GuiRenderer.class)
public interface SpatialHudGuiRendererAccessor {
	@Accessor("pictureInPictureRenderers")
	Map<Class<? extends PictureInPictureRenderState>, PictureInPictureRenderer<?>> spatialhud$getPictureInPictureRenderers();

	@Mutable
	@Accessor("pictureInPictureRenderers")
	void spatialhud$setPictureInPictureRenderers(
			Map<Class<? extends PictureInPictureRenderState>, PictureInPictureRenderer<?>> renderers);
}
