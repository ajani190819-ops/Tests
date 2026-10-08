package dev.arena.spatialhud;

import net.minecraft.client.Minecraft;

/**
 * One camera-yaw-local, waist-height flat map shared by the captured HUD
 * texture and the safe affine fallback. The experimental mesh consumes the
 * full projective mapping; the public HUD API can consume only its centre
 * tangent.
 *
 * <p>The plane stays in front as the player turns horizontally but never rotates
 * with camera pitch to remain visible. Its near-horizontal default resembles a
 * Minecraft map laid flat in front of the player. No arbitrary look-down angle
 * controls visibility: if the physical corners intersect the viewport and are
 * in front of the camera, the panel is rendered; otherwise it is culled.</p>
 */
final class VirtualHudPlane {
	static final float SOURCE_HALF_WIDTH = 112.0f;
	// This deliberately reaches well above the hotbar: status bars, XP, held
	// item names, and common modded lower-HUD additions share the same surface.
	static final float SOURCE_TOP_FROM_BOTTOM = 184.0f;
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
		return toScreenPoint(projectPhysicalPoint(sourceX, sourceY));
	}

	/**
	 * True only when all four physical panel corners are in front of the camera
	 * and their projected bounds intersect the current GUI viewport. This avoids
	 * clamping behind-camera vertices into the giant screen-filling card that
	 * earlier builds showed while looking up.
	 */
	boolean intersectsViewport() {
		Projection[] corners = {
				projectPhysicalPoint(sourceLeft, sourceTop),
				projectPhysicalPoint(sourceRight, sourceTop),
				projectPhysicalPoint(sourceRight, sourceBottom),
				projectPhysicalPoint(sourceLeft, sourceBottom)
		};
		float minX = Float.POSITIVE_INFINITY;
		float minY = Float.POSITIVE_INFINITY;
		float maxX = Float.NEGATIVE_INFINITY;
		float maxY = Float.NEGATIVE_INFINITY;
		for (Projection corner : corners) {
			if (corner.depth() <= 0.08f) {
				return false;
			}
			Point screen = toScreenPoint(corner);
			minX = Math.min(minX, screen.x());
			minY = Math.min(minY, screen.y());
			maxX = Math.max(maxX, screen.x());
			maxY = Math.max(maxY, screen.y());
		}
		return maxX >= 0.0f && minX <= guiWidth && maxY >= 0.0f && minY <= guiHeight;
	}

	/**
	 * Converts a source pixel to the panel's camera-space physical position.
	 * Local X is panel left/right, local Y is panel bottom/top, and local Z is
	 * the panel normal. Roll is deliberately applied first around that normal:
	 * positive roll raises the right edge before yaw/tilt place the map surface.
	 */
	private Projection projectPhysicalPoint(float sourceX, float sourceY) {
		float u = (sourceX - sourceLeft) / sourceWidth;
		float v = (sourceY - sourceTop) / sourceHeight;
		float planeWidth = clamp((float) cfg.planeWidth, 0.10f, 6.0f);
		float planeHeight = planeWidth * sourceHeight / sourceWidth;

		// Plane-local Y is up, while GUI source Y grows downward.
		float localX = (u - 0.5f) * planeWidth;
		float localY = (0.5f - v) * planeHeight;
		float localZ = curvedDepth(u, planeWidth);
		float roll = radians(clamp(cfg.virtualRoll, -45, 45));
		float rolledX = localX * cos(roll) - localY * sin(roll);
		float rolledY = localX * sin(roll) + localY * cos(roll);
		return projectCameraYawMap(rolledX, rolledY, localZ);
	}

	/**
	 * Methods 1 and 2 use a camera-yaw map surface. Horizontal turns keep it in
	 * front, but camera pitch only observes the fixed waist-height plane—it
	 * never rotates the panel to keep it on-screen.
	 */
	private Projection projectCameraYawMap(float localX, float localY, float localZ) {
		float planeYaw = radians(clamp(cfg.virtualYaw, -80, 80));
		// Keep this local up-axis turn identical to WorldSpaceHudRenderer:
		// positive turn moves the right edge farther forward.
		float yawX = localX * cos(planeYaw) - localZ * sin(planeYaw);
		float yawZ = localX * sin(planeYaw) + localZ * cos(planeYaw);

		// This is a literal flat-map tilt around the panel's left-to-right axis.
		// Positive tilt places the far/top edge farther away. A near-horizontal
		// map remains a real plane—Method 2 still applies depth per mesh vertex.
		float planeTilt = radians(clamp(
				cfg.virtualFaceOnLookDownPitch + cfg.virtualPitch, -89, 89));
		float planeY = localY * cos(planeTilt) - yawZ * sin(planeTilt);
		float planeZ = localY * sin(planeTilt) + yawZ * cos(planeTilt);

		float bodyX = (float) cfg.virtualOffsetX + yawX;
		float bodyY = (float) cfg.virtualOffsetY + planeY;
		float bodyZ = (float) Math.max(0.10, cfg.distance) + planeZ;
		// Read the live camera pitch here rather than using the prior frame's HUD
		// extraction sample. Viewport culling and texture activation therefore
		// change at the same instant the finite map crosses the screen edge.
		Minecraft mc = Minecraft.getInstance();
		float livePitch = mc.player == null ? SpatialHud.pitch : mc.player.getXRot();
		float cameraPitch = radians(clamp(livePitch, -80, 89));
		float cameraY = bodyY * cos(cameraPitch) + bodyZ * sin(cameraPitch);
		float depth = -bodyY * sin(cameraPitch) + bodyZ * cos(cameraPitch);
		return new Projection(bodyX, cameraY, depth);
	}

	private Point toScreenPoint(Projection point) {
		// Callers gate through intersectsViewport before accepting a panel. The
		// clamp is a final guard for a configuration edited outside Cloth Config,
		// not a normal rendering path.
		float depth = Math.max(0.08f, point.depth());
		return new Point(
				guiWidth * 0.5f + focalLength * point.x() / depth,
				guiHeight * 0.5f - focalLength * point.y() / depth);
	}

	private record Projection(float x, float y, float depth) {
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
