package dev.arena.spatialhud;

import net.minecraft.client.Minecraft;

/**
 * One camera-yaw-local physical plane shared by the captured HUD texture and
 * the safe affine fallback. The experimental mesh consumes the full projective
 * mapping; the public HUD API can consume only its centre tangent.
 *
 * <p>The plane stays in front of the camera as the player looks left or right,
 * without turning with player-body yaw. It has a fixed horizon-space pitch, so
 * looking at it from different vertical angles changes its real perspective.
 * At the configured face-on look-down angle (30° by default) its projection is
 * rectangular; away from that angle, opposite edges have different depth and
 * the captured mesh becomes a trapezoid.</p>
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
	 * Projects a finished source pixel. Depth is evaluated at every mesh vertex,
	 * so a hotbar slot, item icon, bar, or glyph is genuinely warped instead of
	 * being moved as an independently affine HUD root.
	 */
	Point project(float sourceX, float sourceY) {
		float u = (sourceX - sourceLeft) / sourceWidth;
		float v = (sourceY - sourceTop) / sourceHeight;
		float planeWidth = clamp((float) cfg.planeWidth, 0.10f, 6.0f);
		float planeHeight = planeWidth * sourceHeight / sourceWidth;

		// Plane-local Y is up, while GUI source Y grows downward.
		float localX = (u - 0.5f) * planeWidth;
		float localY = (0.5f - v) * planeHeight;
		float localZ = curvedDepth(u, planeWidth);
		return projectCameraYawHologram(localX, localY, localZ);
	}

	/**
	 * The only active anchor. It deliberately has no player body-yaw input:
	 * horizontal camera turns keep the panel in front without rotating it around
	 * the player when body and camera headings differ.
	 */
	private Point projectCameraYawHologram(float localX, float localY, float localZ) {
		float planeYaw = radians(clamp(cfg.virtualYaw, -80, 80));
		float yawX = localX * cos(planeYaw) + localZ * sin(planeYaw);
		float yawZ = -localX * sin(planeYaw) + localZ * cos(planeYaw);

		// Fixed physical orientation in horizon space. This makes the plane
		// face-on at the configured downward look angle, and creates the actual
		// near/far-edge depth change needed for a trapezoid everywhere else.
		float planePitch = radians(clamp(
				cfg.virtualFaceOnLookDownPitch + cfg.virtualPitch, -80, 80));
		float planeY = localY * cos(planePitch) - yawZ * sin(planePitch);
		float planeZ = localY * sin(planePitch) + yawZ * cos(planePitch);

		// Camera coordinates already represent camera-yaw anchoring. Only the
		// current look pitch converts this physical waist-height centre and plane
		// into camera space.
		float bodyX = (float) cfg.virtualOffsetX + yawX;
		float bodyY = (float) cfg.virtualOffsetY + planeY;
		float bodyZ = (float) Math.max(0.10, cfg.distance) + planeZ;
		float cameraPitch = radians(clamp(SpatialHud.pitch, -80, 89));
		float cameraY = bodyY * cos(cameraPitch) + bodyZ * sin(cameraPitch);
		float depth = -bodyY * sin(cameraPitch) + bodyZ * cos(cameraPitch);
		return projectCameraSpace(bodyX, cameraY, depth);
	}

	private Point projectCameraSpace(float x, float y, float depth) {
		// Do not permit extreme placement settings to move a vertex behind the
		// virtual camera; normal poses remain well in front of it.
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
