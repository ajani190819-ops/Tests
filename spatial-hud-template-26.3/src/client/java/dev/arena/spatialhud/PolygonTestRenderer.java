package dev.arena.spatialhud;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;

/**
 * Geometry for Method 4's purple four-corner surface. Unlike the normal virtual
 * plane, this quad lives directly in GUI coordinates: changing one configured
 * percentage moves exactly one destination corner of the captured HUD texture.
 *
 * <p>The pitch response is a physical sheet, not a screen-space effect. The
 * saved corners are the sheet as seen from a standing eye looking straight
 * ahead. The sheet then stays put in the world: anchored to the player's feet
 * and to head yaw, so turning left and right keeps it in front of you, while
 * head pitch and crouching move it exactly as a real sheet held in place would
 * move. The interior mapping is the homography of that plane, so straight HUD
 * lines stay straight.
 */
final class PolygonTestRenderer {
	private static final int GUIDE_EDGE_COLOR = 0xFFFFD6FF;
	private static final int GUIDE_HANDLE_COLOR = 0xFFC75CFF;
	private static final int GUIDE_HANDLE_RADIUS = 5;
	/** Standing eye height above the feet, in blocks. */
	private static final float REFERENCE_EYE_HEIGHT = 1.62f;
	/**
	 * Distance from the player to the sheet, in blocks. Head rotation alone does
	 * not depend on it; it sets how far the sheet moves on screen when you crouch,
	 * because crouching moves your eye but not the sheet.
	 */
	private static final float SHEET_DISTANCE = 2.0f;
	/**
	 * Largest head pitch the sheet responds to, in degrees. Beyond this the
	 * sheet's lower edge would swing past the eye and the picture would blow up.
	 */
	private static final float MAX_TILT_DEGREES = 70.0f;
	/**
	 * Smallest depth allowed for a sheet corner, as a fraction of the sheet
	 * distance. This keeps a corner from reaching the eye at extreme tilts.
	 */
	private static final float MIN_DEPTH_FRACTION = 0.25f;


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
	 * Projects the saved corners as a physical sheet. The sheet is fixed relative
	 * to the feet and head yaw, so only head pitch and eye height change the
	 * picture. At level pitch with a standing eye, the saved corners are returned
	 * exactly.
	 */
	private static Quad respondToPitch(Quad base, SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		float pitch = clamp(SpatialHud.pitch, -MAX_TILT_DEGREES, MAX_TILT_DEGREES);
		float eyeOffset = currentEyeHeight() - REFERENCE_EYE_HEIGHT;
		if (Math.abs(pitch) < 0.0001f && Math.abs(eyeOffset) < 0.0001f) {
			return base;
		}

		float radians = (float) Math.toRadians(pitch);
		float sine = (float) Math.sin(radians);
		float cosine = (float) Math.cos(radians);
		// Same focal length as Method 3, so perspective matches the world at
		// the player's own field of view.
		float focal = VirtualHudPlane.focalLengthFor(guiHeight);
		float centreX = guiWidth * 0.5f;
		float centreY = guiHeight * 0.5f;
		return new Quad(
				sheetCorner(base.topLeft(), centreX, centreY, focal, sine, cosine, eyeOffset),
				sheetCorner(base.topRight(), centreX, centreY, focal, sine, cosine, eyeOffset),
				sheetCorner(base.bottomRight(), centreX, centreY, focal, sine, cosine, eyeOffset),
				sheetCorner(base.bottomLeft(), centreX, centreY, focal, sine, cosine, eyeOffset));
	}

	/**
	 * One saved corner through the physical sheet. The screen point is first
	 * placed on the sheet at the reference eye, then the eye's current height
	 * is applied so crouching moves the sheet relative to you. Finally the
	 * camera's pitch projects it back to the screen.
	 */
	private static Point sheetCorner(Point screen, float centreX, float centreY, float focal,
			float sine, float cosine, float eyeOffset) {
		float distance = SHEET_DISTANCE;
		float x = (screen.x() - centreX) * distance / focal;
		float y = -(screen.y() - centreY) * distance / focal - eyeOffset;
		float depth = Math.max(distance * cosine - y * sine, MIN_DEPTH_FRACTION * distance);
		float height = y * cosine + distance * sine;
		return new Point(centreX + focal * x / depth, centreY - focal * height / depth);
	}

	private static float currentEyeHeight() {
		Minecraft minecraft = Minecraft.getInstance();
		return minecraft.player != null ? minecraft.player.getEyeHeight() : REFERENCE_EYE_HEIGHT;
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
