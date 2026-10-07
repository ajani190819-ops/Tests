package dev.arena.spatialhud;

import net.minecraft.client.Minecraft;

/**
 * The one projection model for both the captured lower-HUD texture and the
 * safe affine fallback. The plane is camera-yaw anchored at a player-relative
 * waist-height pose. Looking up or down changes its mesh pitch; it is not a
 * collection of separately transformed HUD roots.
 */
final class VirtualHudPlane {
	static final float SOURCE_HALF_WIDTH = 112.0f;
	static final float SOURCE_TOP_FROM_BOTTOM = 128.0f;
	static final float SOURCE_BOTTOM_BELOW_SCREEN = 4.0f;

	record Point(float x, float y) {
	}

	private final SpatialHudConfig cfg;
	private final int guiWidth;
	private final int guiHeight;
	private final float focalLength;
	private final float sourceLeft;
	private final float sourceRight;
	private final float sourceTop;
	private final float sourceBottom;
	private final float sourceCenterX;
	private final float sourceCenterY;
	private final float sourceWidth;
	private final float sourceHeight;
	private final float visibilityOffsetY;

	private VirtualHudPlane(SpatialHudConfig cfg, int guiWidth, int guiHeight, float focalLength) {
		this.cfg = cfg;
		this.guiWidth = guiWidth;
		this.guiHeight = guiHeight;
		this.focalLength = focalLength;
		this.sourceCenterX = guiWidth * 0.5f;
		this.sourceCenterY = guiHeight - 33.0f;
		this.sourceLeft = sourceCenterX - SOURCE_HALF_WIDTH;
		this.sourceRight = sourceCenterX + SOURCE_HALF_WIDTH;
		this.sourceTop = Math.max(0.0f, guiHeight - SOURCE_TOP_FROM_BOTTOM);
		this.sourceBottom = guiHeight + SOURCE_BOTTOM_BELOW_SCREEN;
		this.sourceWidth = sourceRight - sourceLeft;
		this.sourceHeight = sourceBottom - sourceTop;
		this.visibilityOffsetY = fullyOffscreenVerticalCorrection();
	}

	static VirtualHudPlane forGui(SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		Minecraft mc = Minecraft.getInstance();
		double fov = clamp(mc.options.fov().get(), 30.0, 150.0);
		float focal = (float) ((guiHeight * 0.5) / Math.tan(Math.toRadians(fov) * 0.5));
		return new VirtualHudPlane(cfg, guiWidth, guiHeight, focal);
	}

	int guiWidth() {
		return guiWidth;
	}

	int guiHeight() {
		return guiHeight;
	}

	float sourceLeft() {
		return sourceLeft;
	}

	float sourceRight() {
		return sourceRight;
	}

	float sourceTop() {
		return sourceTop;
	}

	float sourceBottom() {
		return sourceBottom;
	}

	float sourceCenterX() {
		return sourceCenterX;
	}

	float sourceCenterY() {
		return sourceCenterY;
	}

	/**
	 * Projects one finished source pixel. The experimental path calls this for
	 * every mesh vertex, so hotbar slots, icon pixels, bars, and glyphs share
	 * exactly the same trapezoid instead of being affine root groups.
	 */
	Point project(float sourceX, float sourceY) {
		Point raw = projectRaw(sourceX, sourceY);
		return new Point(raw.x(), raw.y() + visibilityOffsetY);
	}

	private Point projectRaw(float sourceX, float sourceY) {
		float u = (sourceX - sourceLeft) / sourceWidth;
		float v = (sourceY - sourceTop) / sourceHeight;
		float planeWidth = clamp((float) cfg.planeWidth, 0.10f, 6.0f);
		float planeHeight = planeWidth * sourceHeight / sourceWidth;

		float localX = (u - 0.5f) * planeWidth;
		float localY = (0.5f - v) * planeHeight;
		float localZ = curvedDepth(u, planeWidth);

		// Camera yaw is the anchor, so a left/right look keeps the hologram in
		// front. virtualYaw is deliberate user tuning, not body-turn lag.
		float yaw = radians(clamp(cfg.virtualYaw, -80, 80));
		float yawX = localX * cos(yaw) + localZ * sin(yaw);
		float yawZ = -localX * sin(yaw) + localZ * cos(yaw);

		// This is the complete pitch rule in camera space. At face-on pitch the
		// relative tilt is zero. Above it the far/top edge recedes; below it the
		// opposite edge recedes. Nothing else in the renderer applies pitch.
		float relativePitch = relativeMeshPitch();
		float planeY = localY * cos(relativePitch) - yawZ * sin(relativePitch);
		float planeZ = localY * sin(relativePitch) + yawZ * cos(relativePitch);

		// The centre remains at a waist-height player-relative pose. Convert only
		// that centre through the current look pitch; the mesh orientation above
		// remains one controlled camera-space deformation.
		float viewPitch = radians(clamp(SpatialHud.pitch, -80, 89));
		float centreY = (float) cfg.virtualOffsetY * cos(viewPitch)
				+ (float) Math.max(0.10, cfg.distance) * sin(viewPitch);
		float centreDepth = -(float) cfg.virtualOffsetY * sin(viewPitch)
				+ (float) Math.max(0.10, cfg.distance) * cos(viewPitch);
		return projectCameraSpace((float) cfg.virtualOffsetX + yawX,
				centreY + planeY, centreDepth + planeZ);
	}

	private float relativeMeshPitch() {
		float faceOn = clamp(cfg.virtualFaceOnLookDownPitch, 5, 80);
		float horizonStrength = clamp(cfg.virtualHorizonPerspectivePitch, 15, 85);
		float look = clamp(SpatialHud.pitch, -80, 89);
		float tilt;
		if (look <= faceOn) {
			tilt = horizonStrength * (1.0f - clamp(look, 0.0f, faceOn) / faceOn);
		} else {
			tilt = -horizonStrength * clamp((look - faceOn) / (90.0f - faceOn), 0.0f, 1.0f);
		}
		return radians(clamp(tilt + cfg.virtualPitch, -85, 85));
	}

	/**
	 * Never allow an extreme pose to make the complete plane disappear. This is
	 * a last-resort translation only when every sampled mesh point is above or
	 * below the GUI; it does not flatten, rescale, or otherwise alter the mesh.
	 */
	private float fullyOffscreenVerticalCorrection() {
		float minY = Float.POSITIVE_INFINITY;
		float maxY = Float.NEGATIVE_INFINITY;
		for (int row = 0; row <= 2; row++) {
			float v = row * 0.5f;
			for (int column = 0; column <= 2; column++) {
				float u = column * 0.5f;
				Point point = projectRaw(lerp(sourceLeft, sourceRight, u), lerp(sourceTop, sourceBottom, v));
				minY = Math.min(minY, point.y());
				maxY = Math.max(maxY, point.y());
			}
		}
		float edge = Math.min(16.0f, Math.max(4.0f, guiHeight * 0.04f));
		if (maxY < edge) {
			return edge - maxY;
		}
		if (minY > guiHeight - edge) {
			return guiHeight - edge - minY;
		}
		return 0.0f;
	}

	private Point projectCameraSpace(float x, float y, float depth) {
		depth = Math.max(0.08f, depth);
		return new Point(
				guiWidth * 0.5f + focalLength * x / depth,
				guiHeight * 0.5f - focalLength * y / depth);
	}

	Point projectAt(float u, float v) {
		return project(lerp(sourceLeft, sourceRight, u), lerp(sourceTop, sourceBottom, v));
	}

	float textureU(float sourceX) {
		return sourceX / guiWidth;
	}

	float textureV(float sourceY) {
		return sourceY / guiHeight;
	}

	private float curvedDepth(float u, float planeWidth) {
		float amount = clamp(cfg.experimentalCaptureCurvaturePercent / 100.0f, 0.0f, 1.0f);
		float x = u * 2.0f - 1.0f;
		return x * x * amount * planeWidth * 0.25f;
	}

	private static float lerp(float from, float to, float amount) {
		return from + (to - from) * amount;
	}

	private static float radians(float degrees) {
		return (float) Math.toRadians(degrees);
	}

	private static float sin(float value) {
		return (float) Math.sin(value);
	}

	private static float cos(float value) {
		return (float) Math.cos(value);
	}

	private static float clamp(float value, float min, float max) {
		return value < min ? min : (value > max ? max : value);
	}

	private static double clamp(double value, double min, double max) {
		return value < min ? min : (value > max ? max : value);
	}
}
