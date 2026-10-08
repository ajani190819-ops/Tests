package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.resources.Identifier;

/**
 * Wraps one vanilla bottom-strip element. Disabled mode is a direct vanilla
 * passthrough; enabled mode applies the one shared Spatial HUD pose.
 */
final class SpatialHudElement implements HudElement {
	private final Identifier id;
	private final HudElement vanilla;

	SpatialHudElement(Identifier id, HudElement vanilla) {
		this.id = id;
		this.vanilla = vanilla;
	}

	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		SpatialHudConfig cfg = SpatialHudConfig.get();
		if (!SpatialHud.isGameplayHudActive()) {
			vanilla.extractRenderState(graphics, deltaTracker);
			return;
		}
		// Enabled Spatial HUD owns these selected roots. When the finite physical
		// map surface is outside the viewport, suppress them rather than falling
		// back to a flat vanilla HUD. There is no artificial look-down threshold:
		// any visible piece of the surface keeps its selected roots live.
		if (!SpatialHud.isPhysicalPanelVisibleInGui(graphics.guiWidth(), graphics.guiHeight())) {
			return;
		}
		if (!cfg.showElement(id)) {
			return;
		}

		// AppleSkin and Detail Armor Bar Reconstructed inject their pixels while
		// the vanilla roots extract. Capture therefore runs first so a completed
		// companion group enters one projective texture mesh. Safe affine mode
		// keeps the conservative native-layout fallback below.
		if (ExperimentalHudCapture.isFrameActive()
				&& ExperimentalHudCapture.capture(id, (isolated, tracker) -> vanilla.extractRenderState(isolated, tracker), deltaTracker)) {
			return;
		}

		if (SpatialHud.shouldPreserveNativeStatusLayout(id, cfg)) {
			if (SpatialHud.isStatusLayoutRevealed(cfg)) {
				vanilla.extractRenderState(graphics, deltaTracker);
			}
			return;
		}

		graphics.pose().pushMatrix();
		try {
			applySpatialPose(graphics, cfg, id);
			vanilla.extractRenderState(graphics, deltaTracker);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}

	/** Apply the safe affine tangent for the backing plate (which has no root). */
	static void applySpatialPose(GuiGraphicsExtractor graphics, SpatialHudConfig cfg) {
		applySpatialPose(graphics, cfg, null);
	}

	/**
	 * Safe performance fallback. Fabric exposes only an affine GUI pose, so it
	 * samples the centre tangent of the same {@link VirtualHudPlane} that the
	 * experimental mesh uses. It follows the camera-yaw waist-height map pose
	 * and physical viewport culling, but cannot bend individual icon pixels.
	 */
	static void applySpatialPose(GuiGraphicsExtractor graphics, SpatialHudConfig cfg, Identifier elementId) {
		VirtualHudPlane plane = VirtualHudPlane.forGui(cfg, graphics.guiWidth(), graphics.guiHeight());
		float sourceX = plane.sourceCenterX();
		float sourceY = plane.sourceCenterY();
		VirtualHudPlane.Point centre = plane.project(sourceX, sourceY);
		VirtualHudPlane.Point xTangent = plane.project(sourceX + 1.0f, sourceY);
		VirtualHudPlane.Point yTangent = plane.project(sourceX, sourceY + 1.0f);

		float scaleX = (float) Math.hypot(xTangent.x() - centre.x(), xTangent.y() - centre.y());
		float scaleY = (float) Math.hypot(yTangent.x() - centre.x(), yTangent.y() - centre.y());
		float angle = (float) Math.atan2(xTangent.y() - centre.y(), xTangent.x() - centre.x());

		// Keep the public-HUD path conservative. The capture renderer applies
		// real perspective at every texture vertex; this path deliberately has
		// one stable local scale per root so a modded GUI cannot poison it.
		var pose = graphics.pose();
		pose.translate(centre.x(), centre.y());
		if (angle != 0.0f) {
			pose.rotateAbout(angle, 0.0f, 0.0f);
		}
		pose.scale(Math.max(0.02f, scaleX), Math.max(0.02f, scaleY));
		pose.translate(-sourceX, -sourceY);
	}

}
