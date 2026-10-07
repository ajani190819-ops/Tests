package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.resources.Identifier;

/**
 * Wraps one vanilla HUD element. When the mod is enabled, the vanilla
 * element is re-extracted under a 2D affine pose that places it on a
 * screen-parallel plane at {@code cfg.distance} blocks from the eye — the
 * scale is real perspective math (focal length from the current FOV), so
 * "distance" behaves like actual distance in the world. When disabled, the
 * wrapper is transparent and vanilla renders exactly as before.
 */
final class SpatialHudElement implements HudElement {
	/** Vanilla bottom-strip width in gui pixels (hotbar + bars block). */
	private static final float STRIP_W = 182.0f;
	/** Vertical center of the vanilla bottom strip, in gui pixels. */
	private static final float STRIP_Y_OFF = 33.0f;

	private final Identifier id;
	private final HudElement vanilla;

	SpatialHudElement(Identifier id, HudElement vanilla) {
		this.id = id;
		this.vanilla = vanilla;
	}

	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		SpatialHudConfig cfg = SpatialHudConfig.get();

		if (!SpatialHud.isEnabled() || Minecraft.getInstance().player == null) {
			vanilla.extractRenderState(graphics, deltaTracker);
			return;
		}

		if (!cfg.showElement(id)) {
			return; // spatial mode with this element toggled off: draw nothing
		}

		graphics.pose().pushMatrix();
		try {
			applySpatialPose(graphics, cfg);
			vanilla.extractRenderState(graphics, deltaTracker);
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		} finally {
			graphics.pose().popMatrix();
		}
	}

	/**
	 * Computes and applies the shared "plane" pose. All strip elements and
	 * the panel use the same math every frame so they stay glued together.
	 *
	 * The math: a plane parallel to the screen at distance D, of width W
	 * blocks, appears on screen W * focal / D gui-pixels wide, where
	 * focal = (guiHeight / 2) / tan(fovY / 2) is the perspective focal
	 * length in gui units. That mapping is exactly affine (scale +
	 * translate), which is why it fits the 2D pose API.
	 */
	static void applySpatialPose(GuiGraphicsExtractor graphics, SpatialHudConfig cfg) {
		Minecraft mc = Minecraft.getInstance();
		int w = graphics.guiWidth();
		int h = graphics.guiHeight();

		double fov = mc.options.fov().get();
		double focal = (h / 2.0) / Math.tan(Math.toRadians(fov) / 2.0);

		// Apparent width of the plane on screen (gui pixels).
		float scale = (float) ((cfg.planeWidth * focal / cfg.distance) / STRIP_W);

		// Source: where vanilla draws the strip.
		float srcX = w / 2.0f;
		float srcY = h - STRIP_Y_OFF;

		// Target: center of the view, lowered by cfg.height blocks (in perspective).
		float drop = (float) (cfg.height * focal / cfg.distance);
		float tgtX = w / 2.0f;
		float tgtY = h / 2.0f + drop;

		// Sway: the smoothed look lags the real look; the lag is the sway.
		float yawErr = SpatialHud.wrapDegrees(SpatialHud.yaw - SpatialHud.smoothYaw);
		float pitchErr = SpatialHud.pitch - SpatialHud.smoothPitch;
		float swayX = clamp((float) (-Math.toRadians(yawErr) * focal * 0.30 * cfg.sway), -60f, 60f);
		float swayY = clamp((float) (Math.toRadians(pitchErr) * focal * 0.30 * cfg.sway), -60f, 60f);
		float swayAngle = clamp(yawErr * 0.20f * (float) cfg.sway, -8f, 8f);

		var pose = graphics.pose();
		pose.translate(tgtX + swayX, tgtY + swayY);
		pose.scale(scale, scale);
		pose.translate(-srcX, -srcY);
		if (swayAngle != 0f) {
			pose.rotateAbout((float) Math.toRadians(swayAngle), tgtX + swayX, tgtY + swayY);
		}
	}

	private static float clamp(float v, float min, float max) {
		return v < min ? min : (v > max ? max : v);
	}
}
