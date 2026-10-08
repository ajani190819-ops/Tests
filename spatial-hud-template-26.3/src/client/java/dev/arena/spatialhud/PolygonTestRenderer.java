package dev.arena.spatialhud;

import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Geometry for Method 4's purple four-corner diagnostic. Unlike the normal
 * virtual plane, this quad lives directly in GUI coordinates: changing one
 * configured percentage moves exactly one destination corner of the captured
 * HUD texture.
 */
final class PolygonTestRenderer {
	private static final int GUIDE_EDGE_COLOR = 0xFFFFD6FF;
	private static final int GUIDE_HANDLE_COLOR = 0xFFC75CFF;
	private static final int GUIDE_HANDLE_RADIUS = 5;

	private PolygonTestRenderer() {
	}

	/**
	 * Method 4's one and only visible surface. Its corners start at the saved
	 * percentage positions, then optionally respond to the player's live camera
	 * pitch. It deliberately has no captured-HUD duplicate behind it.
	 */
	static void drawGuide(GuiGraphicsExtractor graphics, SpatialHudConfig cfg) {
		Quad quad = quad(cfg, graphics.guiWidth(), graphics.guiHeight());
		Point[] corners = {quad.topLeft(), quad.topRight(), quad.bottomRight(), quad.bottomLeft()};
		for (int index = 0; index < corners.length; index++) {
			drawEdge(graphics, corners[index], corners[(index + 1) % corners.length]);
		}
		for (Point corner : corners) {
			int x = Math.round(corner.x());
			int y = Math.round(corner.y());
			graphics.fill(x - GUIDE_HANDLE_RADIUS, y - GUIDE_HANDLE_RADIUS,
					x + GUIDE_HANDLE_RADIUS + 1, y + GUIDE_HANDLE_RADIUS + 1, GUIDE_HANDLE_COLOR);
			graphics.fill(x - 2, y - 2, x + 3, y + 3, GUIDE_EDGE_COLOR);
		}
	}

	static Quad quad(SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		Quad base = new Quad(
				point(cfg.polygonTopLeftXPercent, cfg.polygonTopLeftYPercent, guiWidth, guiHeight),
				point(cfg.polygonTopRightXPercent, cfg.polygonTopRightYPercent, guiWidth, guiHeight),
				point(cfg.polygonBottomRightXPercent, cfg.polygonBottomRightYPercent, guiWidth, guiHeight),
				point(cfg.polygonBottomLeftXPercent, cfg.polygonBottomLeftYPercent, guiWidth, guiHeight));
		return cfg.polygonFollowCameraPitch ? respondToPitch(base, cfg, guiWidth, guiHeight) : base;
	}

	/**
	 * At level view (pitch zero), preserve the eight saved coordinates exactly.
	 * Looking down pulls the far/top edge inward and upward while opening the
	 * near/bottom edge; looking up reverses the motion. The response uses all
	 * four saved corners as its starting point instead of replacing them with a
	 * hard-coded rectangle.
	 */
	private static Quad respondToPitch(Quad base, SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		float amount = clamp(SpatialHud.pitch / 90.0f, -1.0f, 1.0f)
				* clamp(cfg.polygonPitchResponsePercent, 0, 100) / 100.0f;
		if (Math.abs(amount) < 0.0001f) {
			return base;
		}
		return new Quad(
				pitchPoint(base.topLeft(), guiWidth, guiHeight, 0.35f, -0.10f, amount),
				pitchPoint(base.topRight(), guiWidth, guiHeight, 0.35f, -0.10f, amount),
				pitchPoint(base.bottomRight(), guiWidth, guiHeight, 0.12f, 0.05f, amount),
				pitchPoint(base.bottomLeft(), guiWidth, guiHeight, 0.12f, 0.05f, amount));
	}

	private static Point pitchPoint(Point base, int guiWidth, int guiHeight,
			float horizontalResponse, float verticalResponse, float amount) {
		float centreX = guiWidth * 0.5f;
		float x = centreX + (base.x() - centreX) * (1.0f - horizontalResponse * amount);
		float y = base.y() + guiHeight * verticalResponse * amount;
		return new Point(clamp(x, 0.0f, guiWidth), clamp(y, 0.0f, guiHeight));
	}

	private static void drawEdge(GuiGraphicsExtractor graphics, Point from, Point to) {
		float deltaX = to.x() - from.x();
		float deltaY = to.y() - from.y();
		float length = (float) Math.hypot(deltaX, deltaY);
		if (length < 0.1f) {
			return;
		}
		graphics.pose().pushMatrix();
		try {
			graphics.pose().translate(from.x(), from.y());
			graphics.pose().rotateAbout((float) Math.atan2(deltaY, deltaX), 0.0f, 0.0f);
			graphics.fill(0, -1, (int) Math.ceil(length), 2, GUIDE_EDGE_COLOR);
		} finally {
			graphics.pose().popMatrix();
		}
	}

	private static Point point(int xPercent, int yPercent, int guiWidth, int guiHeight) {
		return new Point(guiWidth * clampPercent(xPercent) / 100.0f,
				guiHeight * clampPercent(yPercent) / 100.0f);
	}

	private static int clampPercent(int value) {
		return Math.max(0, Math.min(100, value));
	}

	private static float clamp(float value, float minimum, float maximum) {
		return Math.max(minimum, Math.min(maximum, value));
	}

	record Point(float x, float y) {
	}

	/**
	 * Dense mesh vertices are bilinearly interpolated between the four handles.
	 * This gives the source HUD a continuous, intentionally editable quad warp;
	 * a normal ordered quad becomes a trapezoid with its top corners converging.
	 */
	record Quad(Point topLeft, Point topRight, Point bottomRight, Point bottomLeft) {
		Point project(float u, float v) {
			Point top = lerp(topLeft, topRight, u);
			Point bottom = lerp(bottomLeft, bottomRight, u);
			return lerp(top, bottom, v);
		}

		private static Point lerp(Point from, Point to, float amount) {
			return new Point(from.x() + (to.x() - from.x()) * amount,
					from.y() + (to.y() - from.y()) * amount);
		}
	}
}
