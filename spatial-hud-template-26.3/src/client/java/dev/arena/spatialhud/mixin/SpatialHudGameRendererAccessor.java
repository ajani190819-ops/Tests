package dev.arena.spatialhud.mixin;

import net.minecraft.client.gui.render.GuiRenderer;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** Accesses only Minecraft's already-created GUI renderer for its renderer map. */
@Mixin(GameRenderer.class)
public interface SpatialHudGameRendererAccessor {
	@Accessor("guiRenderer")
	GuiRenderer spatialhud$getGuiRenderer();
}
