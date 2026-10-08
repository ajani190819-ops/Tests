package dev.arena.spatialhud;

import com.terraformersmc.modmenu.api.ConfigScreenFactory;
import com.terraformersmc.modmenu.api.ModMenuApi;
import me.shedaniel.autoconfig.AutoConfigClient;

/** Makes the Cloth Config screen available from Spatial HUD's Mod Menu entry. */
public final class SpatialHudModMenu implements ModMenuApi {
	@Override
	public ConfigScreenFactory<?> getModConfigScreenFactory() {
		return parent -> AutoConfigClient.getConfigScreen(SpatialHudConfig.class, parent).get();
	}
}
