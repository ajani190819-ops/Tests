package dev.arena.spatialhud;

import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;

import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.resources.Identifier;

/**
 * Wraps one vanilla bottom-strip element. Disabled mode is a direct vanilla
 * passthrough; enabled mode applies the one shared Spatial HUD pose.
 */
final class SpatialHudElement implements HudElement {
	/** Vanilla bottom-strip width in GUI pixels (hotbar plus the bar block). */
	private static final float STRIP_W = 182.0f;
	/** Reference centre for the vanilla bottom-strip group, in GUI pixels. */
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
		if (!SpatialHud.isGameplayHudActive()) {
			vanilla.extractRenderState(graphics, deltaTracker);
			return;
		}
		if (!cfg.showElement(id)) {
			return;
		}

		// AppleSkin and Detail Armor Bar Reconstructed add their own pixels from
		// inside these vanilla calls, rather than as independently registered HUD
		// elements. Their complete roots remain native in compatibility mode so
		// their icons cannot become detached from the hearts/food/armor they
		// decorate. They are still hidden until the player looks down.
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

	/**
	 * Computes the common plane pose. The physical placement controls follow
	 * Spatial GUI's terminology: width, distance, and vertical height are in
	 * blocks. FOV compensation is enabled by default, matching Spatial GUI's
	 * comfortable, consistent apparent scale at different FOV settings.
	 */
	/** Apply the panel pose for its backing plate (which has no single HUD root). */
	static void applySpatialPose(GuiGraphicsExtractor graphics, SpatialHudConfig cfg) {
		applySpatialPose(graphics, cfg, null);
	}

	/** Apply the shared pose plus a root-specific projective width approximation. */
	static void applySpatialPose(GuiGraphicsExtractor graphics, SpatialHudConfig cfg, Identifier elementId) {
		Minecraft mc = Minecraft.getInstance();
		int w = graphics.guiWidth();
		int h = graphics.guiHeight();

		double fov = Math.max(30.0, Math.min(150.0, mc.options.fov().get()));
		double focal = (h / 2.0) / Math.tan(Math.toRadians(fov) / 2.0);
		double distance = Math.max(0.10, cfg.distance);
		double width = Math.max(0.10, cfg.planeWidth);

		// Same FOV-aware scale curve used by Spatial GUI (baseline 70 by
		// default). It prevents a high FOV from making the HUD unreadably tiny.
		double fovCompensation = 1.0;
		if (cfg.autoScaleByFov) {
			double baseline = Math.max(30.0, Math.min(110.0, cfg.fovBaseline));
			fovCompensation = Math.pow(fov / baseline, 1.2);
		}
		float scale = (float) ((width * focal / distance) / STRIP_W * fovCompensation);
		scale = clamp(scale, 0.15f, 4.0f);

		// Source: centre of the unmodified vanilla strip.
		float srcX = w / 2.0f;
		float srcY = h - STRIP_Y_OFF;

		// Target: a compact lower-half panel when deliberately revealed. In the
		// normal look direction it slides completely below the screen, rather
		// than competing with the world, minimap, crosshair, or overlay mods.
		float drop = (float) (cfg.height * focal / distance);
		float tgtX = w / 2.0f;
		float revealedY = h / 2.0f + drop;
		float tgtY = revealedY;
		if (cfg.revealWhenLookingDown) {
			float start = clamp(cfg.revealStartPitch, 0f, 89f);
			float full = Math.max(start + 1f, clamp(cfg.revealFullPitch, 1f, 90f));
			float reveal = smoothstep(start, full, SpatialHud.pitch);
			float hiddenY = h + Math.max(20, cfg.hiddenBelowScreenPixels);
			tgtY = lerp(hiddenY, revealedY, reveal);
		}

		// The GUI pose API is affine rather than a world-space projection. A
		// vertical foreshortening curve therefore gives the safe, readable
		// horizontal-plane cue: nearly edge-on at the horizon and face-on only
		// after looking down toward the configured pitch.
		float verticalForeshortening = 1.0f;
		if (cfg.lookDownPlaneTilt) {
			float horizonHeight = clamp(cfg.planeHorizonHeightPercent / 100.0f, 0.05f, 1.0f);
			verticalForeshortening = lerp(horizonHeight, 1.0f, planeTiltAmount(cfg));
		}

		// A single affine pose cannot taper a texture from its top to its bottom.
		// Each vanilla HUD root does, however, have a known depth in the strip.
		// Scale that root by the actual perspective law 1 / (1 + depth * k),
		// where k comes from the configured near/far plate-width ratio. This
		// gives the hotbar, bars, XP, and tooltip visibly different apparent
		// widths without capturing them or applying a global GUI transform.
		float horizontalPerspective = horizontalPerspectiveScale(elementId, cfg);

		float strength = clamp((float) cfg.sway, 0.0f, 2.0f);
		float yawErr = SpatialHud.wrapDegrees(SpatialHud.yaw - SpatialHud.smoothYaw);
		float pitchErr = SpatialHud.pitch - SpatialHud.smoothPitch;
		float swayX = clamp((float) (-Math.toRadians(yawErr) * focal * 0.30 * strength), -60f, 60f);
		float swayY = clamp((float) (Math.toRadians(pitchErr) * focal * 0.30 * strength), -60f, 60f);
		float swayAngle = cfg.rotateWithSway
				? clamp(yawErr * 0.20f * strength, -5f, 5f) : 0f;

		var pose = graphics.pose();
		// Transform order matters. Rotating while the strip is centred at the
		// local origin keeps its centre locked to the target. v0.3 rotated about
		// a post-transform screen coordinate, which could make the strip jump.
		pose.translate(tgtX + swayX, tgtY + swayY);
		if (swayAngle != 0f) {
			pose.rotateAbout((float) Math.toRadians(swayAngle), 0f, 0f);
		}
		pose.scale(scale * horizontalPerspective, scale * verticalForeshortening);
		pose.translate(-srcX, -srcY);
	}

	/**
	 * Shared by the panel backing and the affine HUD pose. Zero is the horizon;
	 * one is the player looking directly at the configured face-on angle.
	 */
	static float planeTiltAmount(SpatialHudConfig cfg) {
		if (!cfg.lookDownPlaneTilt) {
			return 1.0f;
		}
		float faceOn = clamp(cfg.planeFaceOnPitch, 20f, 89f);
		float lookDown = clamp(SpatialHud.pitch, 0f, faceOn);
		return smoothstep(0f, 1f, lookDown / faceOn);
	}

	/**
	 * Approximates a horizontal plane's perspective scale at a known HUD-root
	 * depth. The ratio is exact for a flat plane's left/right projection:
	 * nearScale / farScale = zNear / zFar. We cannot warp individual pixels
	 * with the public affine GUI pose, but each root remains internally crisp.
	 */
	static float horizontalPerspectiveScale(Identifier elementId, SpatialHudConfig cfg) {
		if (!cfg.projectiveIconScaling || elementId == null || !cfg.lookDownPlaneTilt) {
			return 1.0f;
		}

		float horizonFarEdge = clamp(cfg.planeHorizonFarEdgeWidthPercent / 100.0f, 0.20f, 1.0f);
		float farEdgeWidth = lerp(horizonFarEdge, 1.0f, planeTiltAmount(cfg));
		float depth = stripDepth(elementId);
		return 1.0f / (1.0f + depth * (1.0f / farEdgeWidth - 1.0f));
	}

	/**
	 * 0 = near/hotbar edge, 1 = far/held-item edge. These match vanilla's
	 * bottom-HUD layout and intentionally keep every root centred while its
	 * apparent width changes with depth.
	 */
	private static float stripDepth(Identifier id) {
		if (id.equals(VanillaHudElements.HOTBAR) || id.equals(VanillaHudElements.SPECTATOR_MENU)) {
			return 0.20f;
		}
		if (id.equals(VanillaHudElements.INFO_BAR) || id.equals(VanillaHudElements.EXPERIENCE_LEVEL)) {
			return 0.38f;
		}
		if (id.equals(VanillaHudElements.HEALTH_BAR) || id.equals(VanillaHudElements.FOOD_BAR)) {
			return 0.58f;
		}
		if (id.equals(VanillaHudElements.ARMOR_BAR) || id.equals(VanillaHudElements.AIR_BAR)
				|| id.equals(VanillaHudElements.MOUNT_HEALTH)) {
			return 0.74f;
		}
		if (id.equals(VanillaHudElements.HELD_ITEM_TOOLTIP) || id.equals(VanillaHudElements.SPECTATOR_TOOLTIP)) {
			return 0.90f;
		}
		return 0.50f;
	}

	private static float smoothstep(float edge0, float edge1, float value) {
		float t = clamp((value - edge0) / (edge1 - edge0), 0f, 1f);
		return t * t * (3f - 2f * t);
	}

	private static float lerp(float from, float to, float amount) {
		return from + (to - from) * amount;
	}

	private static float clamp(float v, float min, float max) {
		return v < min ? min : (v > max ? max : v);
	}
}
