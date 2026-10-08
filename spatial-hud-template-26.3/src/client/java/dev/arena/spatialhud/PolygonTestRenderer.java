package dev.arena.spatialhud;

import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Geometry for Method 4's purple four-corner surface. Unlike the normal virtual
 * plane, this quad lives directly in GUI coordinates: changing one configured
 * percentage moves exactly one destination corner of the captured HUD texture.
 *
 * <p>Two parts of it are deliberately treated as real projection rather than as
 * a screen-space pinch:</p>
 *
 * <ul>
 *   <li>the camera pitch response pitches the quad like a flat card in a 3D
 *       renderer, using the same perspective divide the world uses, so looking
 *       down genuinely narrows the far edge and widens the near edge;</li>
 *   <li>the interior mapping is the homography between the source rectangle and
 *       the four corners, so straight HUD lines stay straight on the surface
 *       instead of bowing.</li>
 * </ul>
 */
final class PolygonTestRenderer {
	private static final int GUIDE_EDGE_COLOR = 0xFFFFD6FF;
	private static final int GUIDE_HANDLE_COLOR = 0xFFC75CFF;
	private static final int GUIDE_HANDLE_RADIUS = 5;
	/**
	 * How far the card may pitch away from the screen. Past this the surface is
	 * nearly edge-on and its projection would collapse into a line, so the
	 * response stops here and stays readable at any camera pitch.
	 */
	private static final float MAX_PITCH_TILT_DEGREES = 70.0f;
	/**
	 * Floor for the perspective divide. A normal card on screen never reaches it
	 * (the worst case is about 0.34 at the 70 degree limit); it only stops a
	 * pathological corner set, much taller than the screen, from dividing by
	 * nearly zero at the vanishing line.
	 */
	private static final float MIN_PERSPECTIVE_W = 0.15f;

	private PolygonTestRenderer() {
	}

	/**
	 * Method 4's plain fallback control surface: the outline and its four corner
	 * handles, drawn directly only when the private capture is not running.
	 * While the capture is running, this same quad instead carries the warped
	 * lower-HUD texture, which includes its own purple border and handles.
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

	/**
	 * The one quad definition shared by the warped capture mesh and the fallback
	 * guide, so both always land on the same four corners - including the live
	 * pitch response.
	 */
	static Quad quad(SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		Quad base = new Quad(
				point(cfg.polygonTopLeftXPercent, cfg.polygonTopLeftYPercent, guiWidth, guiHeight),
				point(cfg.polygonTopRightXPercent, cfg.polygonTopRightYPercent, guiWidth, guiHeight),
				point(cfg.polygonBottomRightXPercent, cfg.polygonBottomRightYPercent, guiWidth, guiHeight),
				point(cfg.polygonBottomLeftXPercent, cfg.polygonBottomLeftYPercent, guiWidth, guiHeight));
		return cfg.polygonFollowCameraPitch ? respondToPitch(base, cfg, guiWidth, guiHeight) : base;
	}

	/**
	 * Pitches the quad about its own left-to-right axis, the way a 3D renderer
	 * moves a flat card: positive Minecraft pitch looks down, which tips the top
	 * (far) edge away from the camera. The far edge then narrows and the near
	 * edge widens because every corner is divided by its own depth, rather than
	 * both edges being scaled by invented constants.
	 *
	 * <p>At level view (pitch zero) the eight saved values are used exactly, and
	 * a response strength of zero freezes the card completely. The card centre
	 * stays where it was configured, so the handles keep placing the surface.</p>
	 */
	private static Quad respondToPitch(Quad base, SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		float response = clamp(cfg.polygonPitchResponsePercent, 0, 100) / 100.0f;
		float degrees = clamp(SpatialHud.pitch, -90.0f, 90.0f) * response;
		float angle = clamp(degrees, -MAX_PITCH_TILT_DEGREES, MAX_PITCH_TILT_DEGREES);
		if (Math.abs(angle) < 0.0001f) {
			return base;
		}

		float radians = (float) Math.toRadians(angle);
		float sine = (float) Math.sin(radians);
		float cosine = (float) Math.cos(radians);
		// The projection's focal length comes from the player's own field of
		// view, so the tilt matches the perspective of the world on screen.
		float focal = VirtualHudPlane.focalLengthFor(guiHeight);
		float screenCentreX = guiWidth * 0.5f;
		float screenCentreY = guiHeight * 0.5f;
		float cardCentreY = (base.topLeft().y() + base.topRight().y()
				+ base.bottomRight().y() + base.bottomLeft().y()) / 4.0f;

		return new Quad(
				pitchPoint(base.topLeft(), screenCentreX, screenCentreY, cardCentreY, sine, cosine, focal),
				pitchPoint(base.topRight(), screenCentreX, screenCentreY, cardCentreY, sine, cosine, focal),
				pitchPoint(base.bottomRight(), screenCentreX, screenCentreY, cardCentreY, sine, cosine, focal),
				pitchPoint(base.bottomLeft(), screenCentreX, screenCentreY, cardCentreY, sine, cosine, focal));
	}

	/**
	 * One corner through the card's projection. {@code dy} is the corner's
	 * distance from the card's own tilting axis, so a corner above the axis
	 * moves behind the screen plane (a divide greater than one: it shrinks and
	 * pulls toward the axis) while the edge below it comes forward, grows, and
	 * pushes away from the axis. Corners are intentionally not clamped to the
	 * viewport: a genuinely tilted surface may reach past the screen edge, and
	 * clamping would flatten the perspective the mode exists to show.
	 */
	private static Point pitchPoint(Point corner, float screenCentreX, float screenCentreY,
			float cardCentreY, float sine, float cosine, float focal) {
		float dy = corner.y() - cardCentreY;
		float w = Math.max(MIN_PERSPECTIVE_W, 1.0f - (dy * sine) / focal);
		float x = screenCentreX + (corner.x() - screenCentreX) / w;
		float y = screenCentreY + (cardCentreY - screenCentreY + dy * cosine) / w;
		return new Point(x, y);
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
	 * The four destination corners plus the exact plane mapping between them.
	 *
	 * <p>The mapping is the homography that sends the source rectangle onto the
	 * quad, which is what a texture on a flat plane looks like under
	 * perspective: parallel lines converge toward one vanishing point and every
	 * straight line stays straight. The previous bilinear interpolation bowed
	 * straight HUD lines whenever the quad was not a parallelogram, so this also
	 * makes a hand-placed quad render correctly at level pitch.</p>
	 */
	static final class Quad {
		private final Point topLeft;
		private final Point topRight;
		private final Point bottomRight;
		private final Point bottomLeft;
		/** Nine homography coefficients, or null when the corners define none. */
		private final float[] projection;

		Quad(Point topLeft, Point topRight, Point bottomRight, Point bottomLeft) {
			this.topLeft = topLeft;
			this.topRight = topRight;
			this.bottomRight = bottomRight;
			this.bottomLeft = bottomLeft;
			this.projection = squareToQuad(topLeft, topRight, bottomRight, bottomLeft);
		}

		Point topLeft() {
			return topLeft;
		}

		Point topRight() {
			return topRight;
		}

		Point bottomRight() {
			return bottomRight;
		}

		Point bottomLeft() {
			return bottomLeft;
		}

		/** Maps the source rectangle, u and v in 0..1, onto the quad as a plane. */
		Point project(float u, float v) {
			float[] h = projection;
			if (h != null) {
				float w = h[6] * u + h[7] * v + h[8];
				if (Math.abs(w) > 1.0e-5f) {
					return new Point((h[0] * u + h[1] * v + h[2]) / w,
							(h[3] * u + h[4] * v + h[5]) / w);
				}
			}
			// Deliberately crossed or collapsed handles cannot define a plane.
			// Fall back to the old bilinear surface so a stress-test corner set
			// still draws something continuous instead of nothing.
			return bilinear(u, v);
		}

		private Point bilinear(float u, float v) {
			Point top = lerp(topLeft, topRight, u);
			Point bottom = lerp(bottomLeft, bottomRight, u);
			return lerp(top, bottom, v);
		}

		/**
		 * Square-to-quad homography: (0,0), (1,0), (1,1), (0,1) map exactly onto
		 * top-left, top-right, bottom-right, bottom-left. Returns null when the
		 * four corners cannot define one, so the caller can fall back.
		 */
		private static float[] squareToQuad(Point topLeft, Point topRight, Point bottomRight, Point bottomLeft) {
			float x0 = topLeft.x();
			float y0 = topLeft.y();
			float x1 = topRight.x();
			float y1 = topRight.y();
			float x2 = bottomRight.x();
			float y2 = bottomRight.y();
			float x3 = bottomLeft.x();
			float y3 = bottomLeft.y();

			float dx1 = x1 - x2;
			float dx2 = x3 - x2;
			float dx3 = x0 - x1 + x2 - x3;
			float dy1 = y1 - y2;
			float dy2 = y3 - y2;
			float dy3 = y0 - y1 + y2 - y3;
			if (dx3 == 0.0f && dy3 == 0.0f) {
				// Parallelogram: the mapping is affine, so there is no
				// perspective row and no division at all.
				return new float[] {
						x1 - x0, x2 - x1, x0,
						y1 - y0, y2 - y1, y0,
						0.0f, 0.0f, 1.0f };
			}

			float denominator = dx1 * dy2 - dx2 * dy1;
			if (Math.abs(denominator) < 1.0e-6f) {
				return null;
			}
			float g = (dx3 * dy2 - dx2 * dy3) / denominator;
			float h = (dx1 * dy3 - dx3 * dy1) / denominator;
			return new float[] {
					x1 - x0 + g * x1, x3 - x0 + h * x3, x0,
					y1 - y0 + g * y1, y3 - y0 + h * y3, y0,
					g, h, 1.0f };
		}

		private static Point lerp(Point from, Point to, float amount) {
			return new Point(from.x() + (to.x() - from.x()) * amount,
					from.y() + (to.y() - from.y()) * amount);
		}
	}
}
