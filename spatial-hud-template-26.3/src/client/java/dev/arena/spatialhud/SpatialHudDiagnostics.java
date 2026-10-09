package dev.arena.spatialhud;

import net.fabricmc.fabric.impl.client.rendering.hud.HudElementRegistryImpl;
import net.fabricmc.fabric.impl.client.rendering.hud.HudLayer;
import net.minecraft.resources.Identifier;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * One-time diagnostic. Logs the HUD roots and layers that Fabric's HUD registry
 * holds, so the elements other mods add (such as the locator arrows and a
 * minimap) can be identified from the log. It only reads the registry. It runs
 * once, and any failure is logged and then ignored, so it can never stop the game.
 */
final class SpatialHudDiagnostics {
	private static boolean done;

	private SpatialHudDiagnostics() {
	}

	/** Logs the registry once per session. Safe to call every frame. */
	static void dumpOnce() {
		if (done) {
			return;
		}
		done = true;
		try {
			Map<Identifier, HudElementRegistryImpl.RootLayer> roots = HudElementRegistryImpl.ROOT_ELEMENTS;
			SpatialHud.LOGGER.info("Spatial HUD diagnostic: {} HUD roots in the registry.", roots.size());
			for (Map.Entry<Identifier, HudElementRegistryImpl.RootLayer> entry : roots.entrySet()) {
				List<String> layerIds = new ArrayList<>();
				for (HudLayer layer : entry.getValue().layers()) {
					layerIds.add(layer.id() + (layer.isRemoved() ? " (removed)" : ""));
				}
				SpatialHud.LOGGER.info("Spatial HUD diagnostic: root {} has layers {}", entry.getKey(), layerIds);
			}
		} catch (Throwable t) {
			SpatialHud.LOGGER.warn("Spatial HUD diagnostic could not read the HUD registry. Continuing without it.", t);
		}
	}
}
