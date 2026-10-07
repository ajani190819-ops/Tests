package dev.arena.spatialhud;

import net.minecraft.client.Minecraft;

/**
 * One camera-relative virtual plane shared by the backing and the captured HUD
 * texture. The mesh renderer consumes the full projective mapping; the public
 * HUD API can consume its centre tangent as a deliberately safe affine fallback.
 *
 * <p>Coordinates are expressed in familiar Spatial-GUI-style blocks: positive
 * X is right, positive Y is up, and positive Z is farther from the viewer.
 * The source rectangle is the intentionally small bottom-HUD envelope rather
 * than the complete GUI texture, so no unrelated overlay can enter the mesh.</p>
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
	 * heart, slot icon, or glyph really becomes a trapezoid/curved shape rather
	 * than merely moving as an affine HUD root.
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

		float yaw = (float) Math.toRadians(clamp(cfg.virtualYaw, -80, 80));
		float effectivePitch = cfg.virtualPitch;
		if (cfg.virtualTiltWithLook) {
			float faceOnAt = clamp(cfg.virtualFaceOnLookDownPitch, 20, 89);
			float lookingDown = clamp(SpatialHud.pitch, 0.0f, faceOnAt);
			// Relative to the camera, a horizontal plane is edge-on at the
			// horizon and turns face-on as the view pitches downward. This is
			// deliberately linear so the perspective starts changing immediately
			// when the player begins looking down, rather than waiting through a
			// smoothstep dead zone.
			effectivePitch += 80.0f * (1.0f - lookingDown / faceOnAt);
		}
		float pitch = (float) Math.toRadians(clamp(effectivePitch, -80, 80));

		// Rotate first about vertical (yaw), then horizontal (pitch). Positive
		// pitch moves the far/top edge away, creating the expected floor-plane
		// trapezoid without hand-authored far-edge scaling.
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

		float depth = (float) Math.max(0.10, cfg.distance) + planeZ;
		// A plane edge may never travel behind the virtual camera. Clamping here
		// is a deterministic safe fallback for an extreme config instead of a
		// NaN that could poison a complete GUI frame.
		depth = Math.max(0.08f, depth);
		float screenX = guiWidth * 0.5f + focalLength * (centreX + yawX) / depth;
		float screenY = guiHeight * 0.5f - focalLength * (centreY + planeY) / depth;
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
