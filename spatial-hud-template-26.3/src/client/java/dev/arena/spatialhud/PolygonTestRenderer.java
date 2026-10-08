package dev.arena.spatialhud;

/**
 * Geometry for Method 4's purple four-corner diagnostic. Unlike the normal
 * virtual plane, this quad lives directly in GUI coordinates: changing one
 * configured percentage moves exactly one destination corner of the captured
 * HUD texture.
 */
final class PolygonTestRenderer {
	private PolygonTestRenderer() {
	}

	static Quad quad(SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		return new Quad(
				point(cfg.polygonTopLeftXPercent, cfg.polygonTopLeftYPercent, guiWidth, guiHeight),
				point(cfg.polygonTopRightXPercent, cfg.polygonTopRightYPercent, guiWidth, guiHeight),
				point(cfg.polygonBottomRightXPercent, cfg.polygonBottomRightYPercent, guiWidth, guiHeight),
				point(cfg.polygonBottomLeftXPercent, cfg.polygonBottomLeftYPercent, guiWidth, guiHeight));
	}

	private static Point point(int xPercent, int yPercent, int guiWidth, int guiHeight) {
		return new Point(guiWidth * clampPercent(xPercent) / 100.0f,
				guiHeight * clampPercent(yPercent) / 100.0f);
	}

	private static int clampPercent(int value) {
		return Math.max(0, Math.min(100, value));
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
