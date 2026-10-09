package dev.arena.spatialhud;

import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import net.fabricmc.fabric.api.client.rendering.v1.FeatureRendererRegistry;
import net.fabricmc.fabric.api.client.rendering.v1.SubmitRenderPhases;
import net.minecraft.client.renderer.SubmitNodeCollector;
import net.minecraft.client.renderer.feature.FeatureFrameContext;
import net.minecraft.client.renderer.feature.FeatureRendererType;
import net.minecraft.client.renderer.feature.RenderTypeFeatureRenderer;
import net.minecraft.client.renderer.feature.submit.SubmitNode;
import net.minecraft.client.renderer.rendertype.RenderType;
import net.minecraft.client.renderer.rendertype.RenderTypes;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.world.phys.Vec3;

import java.util.List;

/**
 * Draws the Method 4 panel in the world, through the level's own submit collector.
 *
 * <p>Three layers, from back to front, all in the same plane:</p>
 * <ol>
 * <li>The purple fill, inset inside the border (solid phase).</li>
 * <li>The captured HUD band, lifted a tiny step toward the viewer so it never
 * fights the fill (translucent phase). It sits above a strip of fill at the
 * bottom, so the hotbar is not pressed against the border.</li>
 * <li>The white border ring around both (solid phase).</li>
 * </ol>
 *
 * <p>Two depth modes. <em>Occluded</em> is depth-tested, so blocks and mobs in front
 * of the panel hide it. <em>Open</em> has no depth test and no depth write, so
 * the panel draws over the world and water behind it is not hidden. Open uses
 * the see-through text render type with a white texture for solid colour. If that
 * cannot be set up, or a draw in open mode throws, the open path turns itself off
 * and the panel is drawn occluded, with one log line saying why.</p>
 */
final class WorldSpaceSolidQuad {
	/** Opaque purple, the Method 4 fill colour. */
	static final int COLOR = 0xFFC75CFF;
	/** Opaque white border. */
	static final int BORDER_COLOR = 0xFFFFFFFF;
	/** Border width as a fraction of the panel's width and height. */
	static final float BORDER_FRACTION = 0.03f;
	/**
	 * Fill strip between the bottom border and the bottom of the captured band, as
	 * a fraction of the panel height. It moves the HUD picture up in the panel, so
	 * the hotbar's bottom row is clear of the border.
	 */
	static final float BAND_BOTTOM_GAP = 0.10f;
	/** How far the captured band sits toward the viewer, in blocks. */
	static final float CAPTURE_LIFT = 0.004f;

	/** Packed full-bright light, so the HUD is not shaded by the world. */
	private static final int FULL_BRIGHT = 0xF000F0;
	private static final int WHITE = 0xFFFFFFFF;

	/** Which way a textured quad draws. */
	enum Mode {
		/** Captured HUD band, depth-tested, writes depth. */
		BAND_OCCLUDED,
		/** Captured HUD band, no depth test or write. */
		BAND_OPEN,
		/** Solid rectangle (fill or border) with the white texture, no depth test or write. */
		RECT_OPEN
	}

	private static final FeatureRendererType<QuadSubmit> QUAD_TYPE =
			FeatureRendererType.create("spatialhud_solid_quad");
	private static final FeatureRendererType<TexturedSubmit> BAND_TYPE =
			FeatureRendererType.create("spatialhud_captured_band");
	private static final FeatureRendererType<TexturedSubmit> BAND_OPEN_TYPE =
			FeatureRendererType.create("spatialhud_captured_band_open");
	private static final FeatureRendererType<TexturedSubmit> RECT_OPEN_TYPE =
			FeatureRendererType.create("spatialhud_solid_rect_open");

	private static boolean registered;
	private static boolean loggedBandDraw;
	private static boolean openDisabled;
	private static boolean bandDisabled;

	private WorldSpaceSolidQuad() {
	}

	/** Registers the feature renderers. Safe to call more than once. */
	static void register() {
		if (registered) {
			return;
		}
		registered = true;
		FeatureRendererRegistry.register(QUAD_TYPE, QuadRenderer::new);
		FeatureRendererRegistry.register(BAND_TYPE, () -> new TexturedRenderer(Mode.BAND_OCCLUDED));
		FeatureRendererRegistry.register(BAND_OPEN_TYPE, () -> new TexturedRenderer(Mode.BAND_OPEN));
		FeatureRendererRegistry.register(RECT_OPEN_TYPE, () -> new TexturedRenderer(Mode.RECT_OPEN));
	}

	/**
	 * True when open mode can be drawn this session. Creates the white texture on
	 * first use. If that fails, the open path is turned off for good and this returns
	 * false, so the caller draws occluded. Call on the render thread.
	 */
	static boolean openPathReady() {
		if (openDisabled) {
			return false;
		}
		try {
			SolidColorTexture.ensure();
			return true;
		} catch (Throwable t) {
			disableOpenPath("could not create the white texture", t);
			return false;
		}
	}

	/** Turns the open path off for the rest of the session and logs the reason once. */
	static void disableOpenPath(String reason, Throwable cause) {
		if (openDisabled) {
			return;
		}
		openDisabled = true;
		SpatialHud.LOGGER.warn("Spatial HUD: occlusion off is unavailable ({}). The panel is drawn with occlusion on instead. {}",
				reason, cause == null ? "" : cause.toString());
	}

	/** Height of the panel relative to its band, so the band keeps its aspect with the gap added. */
	static float panelHeightScale() {
		float side = 1.0f - 2.0f * BORDER_FRACTION;
		return side / (side - BAND_BOTTOM_GAP);
	}

	/**
	 * Submits the purple fill (when enabled) and the white border ring. The corners
	 * are absolute world coordinates, given in the order bottom-left, bottom-right,
	 * top-right, top-left. The panel is a parallelogram, so each point inside it is
	 * found by interpolating along the two edges.
	 */
	static void submitPanel(SubmitNodeCollector collector, Vec3 camera, boolean showFill, boolean showBorder,
			boolean hideEdges, boolean open, Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft) {
		PoseStack.Pose pose = cameraPose(camera);
		float f = BORDER_FRACTION;
		float inner = 1.0f - f;
		// Fill: the inner rectangle only, and only when the config allows it.
		if (showFill) {
			// With Hide Edge Lines, the fill reaches half a border width under the ring,
			// so no purple shows where the fill meets the ring.
			float fillInset = hideEdges ? f * 0.5f : f;
			float fillInner = 1.0f - fillInset;
			submitRect(collector, pose, COLOR, open, bottomLeft, bottomRight, topRight, topLeft,
					fillInset, fillInner, fillInset, fillInner);
		}
		if (!showBorder) {
			return;
		}
		// Border ring: bottom, top, left and right strips.
		submitRect(collector, pose, BORDER_COLOR, open, bottomLeft, bottomRight, topRight, topLeft, 0f, 1f, 0f, f);
		submitRect(collector, pose, BORDER_COLOR, open, bottomLeft, bottomRight, topRight, topLeft, 0f, 1f, inner, 1f);
		submitRect(collector, pose, BORDER_COLOR, open, bottomLeft, bottomRight, topRight, topLeft, 0f, f, f, inner);
		submitRect(collector, pose, BORDER_COLOR, open, bottomLeft, bottomRight, topRight, topLeft, inner, 1f, f, inner);
	}

	/**
	 * Submits the captured HUD band inside the border, above the fill strip at the
	 * bottom. The texture coordinates span the whole band: {@code uLeft}–{@code uRight}
	 * across, and {@code vBottom} (the hotbar side) to {@code vTop} up the panel.
	 */
	static void submitCapturedBand(SubmitNodeCollector collector, Vec3 camera, boolean open,
			float uLeft, float uRight, float vTop, float vBottom,
			Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft) {
		PoseStack.Pose pose = cameraPose(camera);
		// Normal of the panel, turned to face the viewer, so the band is lifted
		// toward them and lit consistently.
		Vec3 across = new Vec3(bottomRight.x - bottomLeft.x, bottomRight.y - bottomLeft.y, bottomRight.z - bottomLeft.z);
		Vec3 up = new Vec3(topLeft.x - bottomLeft.x, topLeft.y - bottomLeft.y, topLeft.z - bottomLeft.z);
		Vec3 normal = normalize(cross(across, up));
		double toViewer = (camera.x - bottomLeft.x) * normal.x
				+ (camera.y - bottomLeft.y) * normal.y
				+ (camera.z - bottomLeft.z) * normal.z;
		if (toViewer < 0) {
			normal = new Vec3(-normal.x, -normal.y, -normal.z);
		}
		Vec3 lift = new Vec3(normal.x * CAPTURE_LIFT, normal.y * CAPTURE_LIFT, normal.z * CAPTURE_LIFT);

		float f = BORDER_FRACTION;
		float inner = 1.0f - f;
		// Band: inside the border on the sides and top; above the fill strip at the bottom.
		float bandBottom = f + BAND_BOTTOM_GAP;
		submitCapturedRect(collector, pose, normal, lift, open, bottomLeft, bottomRight, topLeft,
				f, inner, bandBottom, inner, uLeft, uRight, vBottom, vTop);
	}

	/**
	 * One textured rectangle inside the panel. {@code s0..s1} and {@code t0..t1} are
	 * panel fractions (0 to 1, bottom-left to top-right). The texture coordinates
	 * run from {@code uLeft} to {@code uRight} across, and from {@code vBottom} to
	 * {@code vTop} up.
	 */
	private static void submitCapturedRect(SubmitNodeCollector collector, PoseStack.Pose pose,
			Vec3 normal, Vec3 lift, boolean open, Vec3 bottomLeft, Vec3 bottomRight, Vec3 topLeft,
			float s0, float s1, float t0, float t1,
			float uLeft, float uRight, float vBottom, float vTop) {
		Vec3 p00 = offset(at(bottomLeft, bottomRight, topLeft, s0, t0), lift);
		Vec3 p10 = offset(at(bottomLeft, bottomRight, topLeft, s1, t0), lift);
		Vec3 p11 = offset(at(bottomLeft, bottomRight, topLeft, s1, t1), lift);
		Vec3 p01 = offset(at(bottomLeft, bottomRight, topLeft, s0, t1), lift);
		float u0 = lerp(uLeft, uRight, s0);
		float u1 = lerp(uLeft, uRight, s1);
		float v0 = lerp(vBottom, vTop, t0);
		float v1 = lerp(vBottom, vTop, t1);
		Mode mode = open ? Mode.BAND_OPEN : Mode.BAND_OCCLUDED;
		collector.submitCustom(SubmitRenderPhases.TRANSLUCENT_CUSTOM_GEOMETRY, new TexturedSubmit(mode, pose, normal, WHITE,
				new Corner(p00, u0, v0), new Corner(p10, u1, v0),
				new Corner(p11, u1, v1), new Corner(p01, u0, v1)));
	}

	/**
	 * Submits one rectangle given in panel coordinates. Both ranges run from 0 to 1
	 * across the panel: s along the bottom edge, t up the side. Depth-tested when
	 * occluded; with the white texture, tinted by {@code color}, when open.
	 */
	private static void submitRect(SubmitNodeCollector collector, PoseStack.Pose pose, int color, boolean open,
			Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft,
			float s0, float s1, float t0, float t1) {
		Vec3 p00 = at(bottomLeft, bottomRight, topLeft, s0, t0);
		Vec3 p10 = at(bottomLeft, bottomRight, topLeft, s1, t0);
		Vec3 p11 = at(bottomLeft, bottomRight, topLeft, s1, t1);
		Vec3 p01 = at(bottomLeft, bottomRight, topLeft, s0, t1);
		if (open) {
			// The white texel is 1x1, so every corner samples the same colour.
			collector.submitCustom(SubmitRenderPhases.SOLID, new TexturedSubmit(Mode.RECT_OPEN, pose, new Vec3(0, 1, 0), color,
					new Corner(p00, 0f, 0f), new Corner(p10, 0f, 0f),
					new Corner(p11, 0f, 0f), new Corner(p01, 0f, 0f)));
		} else {
			collector.submitCustom(SubmitRenderPhases.SOLID, new QuadSubmit(pose, color, p00, p10, p11, p01));
		}
	}

	private static PoseStack.Pose cameraPose(Vec3 camera) {
		PoseStack toCamera = new PoseStack();
		toCamera.translate(-camera.x, -camera.y, -camera.z);
		return toCamera.last().copy();
	}

	/** The point at (s, t) on the panel: bottom-left plus s along the bottom and t up. */
	private static Vec3 at(Vec3 bottomLeft, Vec3 bottomRight, Vec3 topLeft, float s, float t) {
		return new Vec3(
				bottomLeft.x + (bottomRight.x - bottomLeft.x) * s + (topLeft.x - bottomLeft.x) * t,
				bottomLeft.y + (bottomRight.y - bottomLeft.y) * s + (topLeft.y - bottomLeft.y) * t,
				bottomLeft.z + (bottomRight.z - bottomLeft.z) * s + (topLeft.z - bottomLeft.z) * t);
	}

	private static Vec3 offset(Vec3 point, Vec3 by) {
		return new Vec3(point.x + by.x, point.y + by.y, point.z + by.z);
	}

	private static float lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}

	private static Vec3 cross(Vec3 a, Vec3 b) {
		return new Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	}

	private static Vec3 normalize(Vec3 v) {
		double length = Math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		if (length < 1.0e-9) {
			return new Vec3(0, 1, 0);
		}
		return new Vec3(v.x / length, v.y / length, v.z / length);
	}

	/** One occluded solid quad, waiting to be drawn in the SOLID phase. */
	record QuadSubmit(PoseStack.Pose pose, int color, Vec3 bottomLeft, Vec3 bottomRight,
			Vec3 topRight, Vec3 topLeft) implements SubmitNode {
		@Override
		public FeatureRendererType<? extends SubmitNode> featureType() {
			return QUAD_TYPE;
		}
	}

	/** One corner of a textured quad: a world position and its texture coordinates. */
	record Corner(Vec3 point, float u, float v) {
	}

	/** One textured quad. The mode picks its render type, phase, and vertex format. */
	record TexturedSubmit(Mode mode, PoseStack.Pose pose, Vec3 normal, int color,
			Corner c0, Corner c1, Corner c2, Corner c3) implements SubmitNode {
		@Override
		public FeatureRendererType<? extends SubmitNode> featureType() {
			return switch (mode) {
				case BAND_OCCLUDED -> BAND_TYPE;
				case BAND_OPEN -> BAND_OPEN_TYPE;
				case RECT_OPEN -> RECT_OPEN_TYPE;
			};
		}
	}

	private static final class QuadRenderer extends RenderTypeFeatureRenderer<QuadSubmit> {
		@Override
		protected void buildGroup(FeatureFrameContext context, List<QuadSubmit> submits) {
			if (submits.isEmpty()) {
				return;
			}
			// Depth-tested fill, no depth write.
			VertexConsumer buffer = getVertexBuilder(RenderTypes.debugFilledBox());
			for (QuadSubmit submit : submits) {
				buffer.addVertex(submit.pose(), (float) submit.bottomLeft().x, (float) submit.bottomLeft().y,
						(float) submit.bottomLeft().z).setColor(submit.color());
				buffer.addVertex(submit.pose(), (float) submit.bottomRight().x, (float) submit.bottomRight().y,
						(float) submit.bottomRight().z).setColor(submit.color());
				buffer.addVertex(submit.pose(), (float) submit.topRight().x, (float) submit.topRight().y,
						(float) submit.topRight().z).setColor(submit.color());
				buffer.addVertex(submit.pose(), (float) submit.topLeft().x, (float) submit.topLeft().y,
						(float) submit.topLeft().z).setColor(submit.color());
			}
		}
	}

	private static final class TexturedRenderer extends RenderTypeFeatureRenderer<TexturedSubmit> {
		private final Mode mode;

		TexturedRenderer(Mode mode) {
			this.mode = mode;
		}

		@Override
		protected void buildGroup(FeatureFrameContext context, List<TexturedSubmit> submits) {
			if (submits.isEmpty()) {
				return;
			}
			if (mode == Mode.BAND_OCCLUDED ? bandDisabled : openDisabled) {
				return;
			}
			try {
				if (!loggedBandDraw && mode == Mode.BAND_OCCLUDED) {
					loggedBandDraw = true;
					SpatialHud.LOGGER.info("Spatial HUD stage 3: drawing the captured HUD band ({} quad(s)).",
							submits.size());
				}
				VertexConsumer buffer = getVertexBuilder(renderType());
				for (TexturedSubmit submit : submits) {
					writeQuad(buffer, submit);
				}
			} catch (Throwable t) {
				// Never let a draw take the game down. Open modes fall back to occluded;
				// the occluded band has no fallback, so it stops drawing.
				if (mode == Mode.BAND_OCCLUDED) {
					bandDisabled = true;
					SpatialHud.LOGGER.error("Spatial HUD: the captured band stopped drawing after an error.", t);
				} else {
					disableOpenPath("a draw in open mode failed", t);
				}
			}
		}

		private RenderType renderType() {
			return switch (mode) {
				case BAND_OCCLUDED -> RenderTypes.entityTranslucent(CapturedHudTexture.ID, false);
				case BAND_OPEN -> RenderTypes.textSeeThrough(CapturedHudTexture.ID);
				case RECT_OPEN -> RenderTypes.textSeeThrough(SolidColorTexture.ID);
			};
		}

		/**
		 * Writes one quad. Each vertex sets every element of its layout, so none can
		 * be left incomplete: the occluded layout (position, colour, UV, overlay,
		 * light, normal) and the see-through layout (position, UV, colour, light).
		 */
		private void writeQuad(VertexConsumer buffer, TexturedSubmit submit) {
			writeCorner(buffer, submit, submit.c0());
			writeCorner(buffer, submit, submit.c1());
			writeCorner(buffer, submit, submit.c2());
			writeCorner(buffer, submit, submit.c3());
		}

		private void writeCorner(VertexConsumer buffer, TexturedSubmit submit, Corner corner) {
			Vec3 p = corner.point();
			if (mode == Mode.BAND_OCCLUDED) {
				Vec3 n = submit.normal();
				buffer.addVertex(submit.pose(), (float) p.x, (float) p.y, (float) p.z)
						.setColor(submit.color())
						.setUv(corner.u(), corner.v())
						.setOverlay(OverlayTexture.NO_OVERLAY)
						.setLight(FULL_BRIGHT)
						.setNormal((float) n.x, (float) n.y, (float) n.z);
			} else {
				buffer.addVertex(submit.pose(), (float) p.x, (float) p.y, (float) p.z)
						.setColor(submit.color())
						.setUv(corner.u(), corner.v())
						.setLight(FULL_BRIGHT);
			}
		}
	}
}
