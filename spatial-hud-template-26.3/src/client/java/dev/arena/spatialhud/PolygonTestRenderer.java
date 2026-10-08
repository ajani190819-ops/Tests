package dev.arena.spatialhud;

import net.minecraft.client.gui.GuiGraphicsExtractor;

import java.util.Arrays;

/**
 * Purple diagnostic surface for checking four-corner geometry independently of
 * HUD capture, world rendering, and vanilla HUD-root placement. The controls
 * are percentage coordinates in {@link SpatialHudConfig}; moving one pair of
 * sliders moves exactly one visible square handle.
 */
final class PolygonTestRenderer {
	private static final int FILL_COLOR = 0x683C096E;
	private static final int EDGE_COLOR = 0xFFC75CFF;
	private static final int HANDLE_FILL_COLOR = 0xFFFFD6FF;
	private static final int HANDLE_EDGE_COLOR = 0xFF6C1D8B;
	private static final int HANDLE_RADIUS = 5;

	private PolygonTestRenderer() {
	}

	static void draw(GuiGraphicsExtractor graphics, SpatialHudConfig cfg) {
		Point[] corners = {
				point(cfg.polygonTopLeftXPercent, cfg.polygonTopLeftYPercent, graphics),
				point(cfg.polygonTopRightXPercent, cfg.polygonTopRightYPercent, graphics),
				point(cfg.polygonBottomRightXPercent, cfg.polygonBottomRightYPercent, graphics),
				point(cfg.polygonBottomLeftXPercent, cfg.polygonBottomLeftYPercent, graphics)
		};

		fillPolygon(graphics, corners);
		for (int index = 0; index < corners.length; index++) {
			drawEdge(graphics, corners[index], corners[(index + 1) % corners.length]);
		}
		for (Point corner : corners) {
			drawHandle(graphics, corner);
		}
	}

	private static Point point(int xPercent, int yPercent, GuiGraphicsExtractor graphics) {
		float x = graphics.guiWidth() * clampPercent(xPercent) / 100.0f;
		float y = graphics.guiHeight() * clampPercent(yPercent) / 100.0f;
		return new Point(x, y);
	}

	/**
	 * Filled scanlines keep this diagnostic renderer within the released GUI
	 * API—no pipeline, texture target, or shader is involved. The first/last
	 * intersection rule remains useful even for a deliberately crossed test
	 * quadrilateral, which makes invalid-corner-order defects obvious.
	 */
	private static void fillPolygon(GuiGraphicsExtractor graphics, Point[] corners) {
		float lowest = Float.POSITIVE_INFINITY;
		float highest = Float.NEGATIVE_INFINITY;
		for (Point corner : corners) {
			lowest = Math.min(lowest, corner.y());
			highest = Math.max(highest, corner.y());
		}
		int startY = Math.max(0, (int) Math.floor(lowest));
		int endY = Math.min(graphics.guiHeight() - 1, (int) Math.ceil(highest));
		float[] intersections = new float[corners.length];

		for (int y = startY; y <= endY; y++) {
			float scanY = y + 0.5f;
			int count = 0;
			for (int index = 0; index < corners.length; index++) {
				Point from = corners[index];
				Point to = corners[(index + 1) % corners.length];
				if ((from.y() <= scanY && to.y() > scanY) || (to.y() <= scanY && from.y() > scanY)) {
					float progress = (scanY - from.y()) / (to.y() - from.y());
					intersections[count++] = from.x() + (to.x() - from.x()) * progress;
				}
			}
			if (count < 2) {
				continue;
			}
			Arrays.sort(intersections, 0, count);
			int left = Math.max(0, (int) Math.floor(intersections[0]));
			int right = Math.min(graphics.guiWidth(), (int) Math.ceil(intersections[count - 1]));
			if (right > left) {
				graphics.fill(left, y, right, y + 1, FILL_COLOR);
			}
		}
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
			graphics.fill(0, -1, (int) Math.ceil(length), 2, EDGE_COLOR);
		} finally {
			graphics.pose().popMatrix();
		}
	}

	private static void drawHandle(GuiGraphicsExtractor graphics, Point point) {
		int x = Math.round(point.x());
		int y = Math.round(point.y());
		graphics.fill(x - HANDLE_RADIUS - 1, y - HANDLE_RADIUS - 1,
				x + HANDLE_RADIUS + 2, y + HANDLE_RADIUS + 2, HANDLE_EDGE_COLOR);
		graphics.fill(x - HANDLE_RADIUS, y - HANDLE_RADIUS,
				x + HANDLE_RADIUS + 1, y + HANDLE_RADIUS + 1, HANDLE_FILL_COLOR);
	}

	private static int clampPercent(int value) {
		return Math.max(0, Math.min(100, value));
	}

	private record Point(float x, float y) {
	}
}
