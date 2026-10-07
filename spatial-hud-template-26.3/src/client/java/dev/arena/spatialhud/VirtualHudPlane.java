package dev.arena.spatialhud;

import net.minecraft.client.Minecraft;

/**
 * One player-local virtual plane shared by the backing and captured HUD
 * texture. The mesh renderer consumes the full projective mapping; the public
 * HUD API can consume its centre tangent as a deliberately safe affine fallback.
 *
 * <p>The default pose is a hologram in front of the player near waist height.
 * It follows camera yaw, but not camera pitch, so it stays parallel to the
 * horizon. The view therefore changes the camera's angle to a fixed plane
 * instead of merely animating a screen-space card.</p>
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
	 * Project one source pixel through the same physical plane used by every
	 * captured texture vertex. Because depth is evaluated per vertex, a finished
	 * heart, slot icon, or glyph really becomes a trapezoid rather than merely
	 * moving as an affine HUD root.
	 */
	Point project(float sourceX, float sourceY) {
		float u = (sourceX - sourceLeft) / sourceWidth;
		float v = (sourceY - sourceTop) / sourceHeight;
		float planeWidth = clamp((float) cfg.planeWidth, 0.10f, 6.0f);
		float planeHeight = planeWidth * sourceHeight / sourceWidth;

		// Plane-local Y is up, whereas GUI source Y grows downward.
		float localX = (u - 0.5f) * planeWidth;
		float localY = (0.5f - v) * planeHeight;
		float localZ = curvedDepth(u, planeWidth);

		if (cfg.virtualAnchorMode == SpatialHudConfig.VirtualAnchorMode.CAMERA_YAW) {
			return projectCameraYawHologram(localX, localY, localZ);
		}
		if (cfg.virtualAnchorMode == SpatialHudConfig.VirtualAnchorMode.PLAYER_BODY) {
			return projectPlayerBody(localX, localY, localZ);
		}
		return projectCameraRelative(localX, localY, localZ);
	}

	/**
	 * Default hologram anchor. The plane follows camera yaw so it remains in
	 * front while the player looks left or right, but it never follows camera
	 * pitch. It therefore stays parallel to the horizon while pitch changes the
	 * perspective of a fixed plane.
	 */
	private Point projectCameraYawHologram(float localX, float localY, float localZ) {
		return projectHologram(localX, localY, localZ, 0.0f);
	}

	/** Compatibility option for a plane that remains aligned to body yaw. */
	private Point projectPlayerBody(float localX, float localY, float localZ) {
		float yawDifference = (float) Math.toRadians(SpatialHud.wrapDegrees(SpatialHud.bodyYaw - SpatialHud.yaw));
		return projectHologram(localX, localY, localZ, yawDifference);
	}

	private Point projectHologram(float localX, float localY, float localZ, float yawDifference) {
		float planeYaw = (float) Math.toRadians(clamp(cfg.virtualYaw, -80, 80));
		float yawX = localX * (float) Math.cos(planeYaw) + localZ * (float) Math.sin(planeYaw);
		float yawZ = -localX * (float) Math.sin(planeYaw) + localZ * (float) Math.cos(planeYaw);

		// This is a fixed orientation in horizon/player space, not a look-pitch
		// animation. A 30-degree face-on setting is rectangular when the camera
		// looks 30 degrees below the horizon.
		float planePitch = (float) Math.toRadians(clamp(
				cfg.virtualFaceOnLookDownPitch + cfg.virtualPitch, -80, 80));
		float bodyPlaneY = localY * (float) Math.cos(planePitch) - yawZ * (float) Math.sin(planePitch);
		float bodyPlaneZ = localY * (float) Math.sin(planePitch) + yawZ * (float) Math.cos(planePitch);

		float bodyX = (float) cfg.virtualOffsetX + yawX;
		float bodyY = (float) cfg.virtualOffsetY + bodyPlaneY;
		float bodyZ = (float) Math.max(0.10, cfg.distance) + bodyPlaneZ;

		float cameraX = bodyX * (float) Math.cos(yawDifference) + bodyZ * (float) Math.sin(yawDifference);
		float cameraForward = -bodyX * (float) Math.sin(yawDifference) + bodyZ * (float) Math.cos(yawDifference);
		float cameraPitch = (float) Math.toRadians(SpatialHud.pitch);
		float cameraY = bodyY * (float) Math.cos(cameraPitch) + cameraForward * (float) Math.sin(cameraPitch);
		float depth = -bodyY * (float) Math.sin(cameraPitch) + cameraForward * (float) Math.cos(cameraPitch);
		return projectCameraSpace(cameraX, cameraY, depth);
	}

	/**
	 * Compatibility implementation for the earlier camera-relative anchor
	 * choices. PLAYER_BODY is the default and is the only mode that models a
	 * stable hologram at a position in front of the player.
	 */
	private Point projectCameraRelative(float localX, float localY, float localZ) {
		float yaw = (float) Math.toRadians(clamp(cfg.virtualYaw, -80, 80));
		float effectivePitch = cfg.virtualPitch;
		if (cfg.virtualTiltWithLook) {
			float faceOnAt = clamp(cfg.virtualFaceOnLookDownPitch, 5, 80);
			float lookingDown = clamp(SpatialHud.pitch, 0.0f, faceOnAt);
			effectivePitch += 80.0f * (1.0f - lookingDown / faceOnAt);
		}
		float pitch = (float) Math.toRadians(clamp(effectivePitch, -80, 80));

		float yawX = localX * (float) Math.cos(yaw) + localZ * (float) Math.sin(yaw);
		float yawZ = -localX * (float) Math.sin(yaw) + localZ * (float) Math.cos(yaw);
		float planeY = localY * (float) Math.cos(pitch) - yawZ * (float) Math.sin(pitch);
		float planeZ = localY * (float) Math.sin(pitch) + yawZ * (float) Math.cos(pitch);

		float centreX = (float) cfg.virtualOffsetX;
		float centreY = (float) cfg.virtualOffsetY;
		if (cfg.virtualAnchorMode == SpatialHudConfig.VirtualAnchorMode.WORLD_LIKE) {
			float strength = clamp((float) cfg.virtualWorldParallaxStrength, 0.0f, 2.0f);
			float yawError = SpatialHud.wrapDegrees(SpatialHud.yaw - SpatialHud.smoothYaw);
			float pitchError = SpatialHud.pitch - SpatialHud.smoothPitch;
			centreX -= (float) Math.toRadians(yawError) * Math.max(0.10, cfg.distance) * strength;
			centreY += (float) Math.toRadians(pitchError) * Math.max(0.10, cfg.distance) * strength;
		}
		return projectCameraSpace(centreX + yawX, centreY + planeY,
				(float) Math.max(0.10, cfg.distance) + planeZ);
	}

	private Point projectCameraSpace(float x, float y, float depth) {
		// A vertex may never travel behind the virtual camera. This only protects
		// extreme user values; ordinary player-body poses remain well in front.
		depth = Math.max(0.08f, depth);
		float screenX = guiWidth * 0.5f + focalLength * x / depth;
		float screenY = guiHeight * 0.5f - focalLength * y / depth;
		return new Point(screenX, screenY);
	}

	Point projectAt(float u, float v) {
		return project(lerp(sourceLeft, sourceRight, u), lerp(sourceTop, sourceBottom, v));
	}

	/** Source-texture U for a GUI X coordinate. */
	float textureU(float sourceX) {
		return sourceX / guiWidth;
	}

	/** Source-texture V for a GUI Y coordinate. */
	float textureV(float sourceY) {
		return sourceY / guiHeight;
	}

	/**
	 * Curvature is a real mesh depth change: the centre stays closest and both
	 * edges recede. Zero retains the requested flat projective trapezoid.
	 */
	private float curvedDepth(float u, float planeWidth) {
		float amount = clamp(cfg.experimentalCaptureCurvaturePercent / 100.0f, 0.0f, 1.0f);
		float x = u * 2.0f - 1.0f;
		return x * x * amount * planeWidth * 0.25f;
	}

	private static float lerp(float from, float to, float amount) {
		return from + (to - from) * amount;
	}

	private static float clamp(float value, float min, float max) {
		return value < min ? min : (value > max ? max : value);
	}

	private static double clamp(double value, double min, double max) {
		return value < min ? min : (value > max ? max : value);
	}
}
