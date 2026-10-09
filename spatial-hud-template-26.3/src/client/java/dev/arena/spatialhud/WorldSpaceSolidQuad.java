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
import net.minecraft.client.renderer.rendertype.RenderTypes;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.world.phys.Vec3;

import java.util.List;

/**
 * Draws the Method 4 panel in the world, through the level's own submit collector.
 *
 * <p>The earlier world-space draw opened its own render pass inside the level's
 * translucent pass, and the game refused it. Submitting a node instead lets the
 * level draw the quads in its own pass, the way Fabric's test mods do.</p>
 *
 * <p>Three layers, from back to front, all in the same plane:</p>
 * <ol>
 * <li>The purple fill, inset inside the border (depth-tested, solid phase).</li>
 * <li>The captured HUD band, inset the same amount and lifted a tiny step toward
 * the viewer so it never fights the fill (depth-tested, translucent phase).</li>
 * <li>The white border ring around both (depth-tested, solid phase).</li>
 * </ol>
 * <p>The border and fill never overlap, so they cannot z-fight. The band is
 * lifted by {@link #CAPTURE_LIFT}, which is far smaller than the panel.</p>
 */
final class WorldSpaceSolidQuad {
	/** Opaque purple, the same as the Method 4 failure marker. */
	static final int COLOR = 0xFFC75CFF;
	/** Opaque white border. */
	static final int BORDER_COLOR = 0xFFFFFFFF;
	/** Border width as a fraction of the panel's width and height. */
	static final float BORDER_FRACTION = 0.03f;
	/** How far the captured band sits toward the viewer, in blocks. */
	static final float CAPTURE_LIFT = 0.004f;
	/** Packed full-bright light, so the HUD is not shaded by the world. */
	private static final int FULL_BRIGHT = 0xF000F0;

	private static final FeatureRendererType<QuadSubmit> TYPE =
			FeatureRendererType.create("spatialhud_solid_quad");
	private static final FeatureRendererType<TexturedSubmit> TEXTURED_TYPE =
			FeatureRendererType.create("spatialhud_captured_band");
	private static boolean registered;

	private WorldSpaceSolidQuad() {
	}

	/** Registers the feature renderers. Safe to call more than once. */
	static void register() {
		if (registered) {
			return;
		}
		registered = true;
		FeatureRendererRegistry.register(TYPE, Renderer::new);
		FeatureRendererRegistry.register(TEXTURED_TYPE, TexturedRenderer::new);
	}

	/**
	 * Submits the purple fill (when enabled) and the white border ring. The corners
	 * are absolute world coordinates, given in the order bottom-left, bottom-right,
	 * top-right, top-left. The panel is a parallelogram, so each point inside it is
	 * found by interpolating along the two edges.
	 */
	static void submitPanel(SubmitNodeCollector collector, Vec3 camera, boolean showFill,
			Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft) {
		PoseStack.Pose pose = cameraPose(camera);
		float f = BORDER_FRACTION;
		float inner = 1.0f - f;
		// Fill: the inner rectangle only, and only when the config allows it.
		if (showFill) {
			submitRect(collector, pose, COLOR, bottomLeft, bottomRight, topRight, topLeft,
					f, inner, f, inner);
		}
		// Border ring: bottom, top, left and right strips.
		submitRect(collector, pose, BORDER_COLOR, bottomLeft, bottomRight, topRight, topLeft,
				0f, 1f, 0f, f);
		submitRect(collector, pose, BORDER_COLOR, bottomLeft, bottomRight, topRight, topLeft,
				0f, 1f, inner, 1f);
		submitRect(collector, pose, BORDER_COLOR, bottomLeft, bottomRight, topRight, topLeft,
				0f, f, f, inner);
		submitRect(collector, pose, BORDER_COLOR, bottomLeft, bottomRight, topRight, topLeft,
				inner, 1f, f, inner);
	}

	/**
	 * Roadmap stage 3. Submits the captured HUD band onto the inner rectangle of
	 * the panel, inside the border. The texture coordinates span the band:
	 * {@code uLeft}–{@code uRight} across, and {@code vBottom} (the hotbar side) to
	 * {@code vTop} up the panel.
	 */
	static void submitCapturedBand(SubmitNodeCollector collector, Vec3 camera,
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
		submitCapturedRect(collector, pose, normal, lift, bottomLeft, bottomRight, topLeft,
				f, inner, f, inner, uLeft, uRight, vBottom, vTop);
	}

	/**
	 * One textured rectangle inside the panel. {@code s0..s1} and {@code t0..t1} are
	 * panel fractions (0 to 1, bottom-left to top-right). The texture coordinates
	 * run from {@code uLeft} to {@code uRight} across, and from {@code vBottom} to
	 * {@code vTop} up.
	 */
	private static void submitCapturedRect(SubmitNodeCollector collector, PoseStack.Pose pose,
			Vec3 normal, Vec3 lift, Vec3 bottomLeft, Vec3 bottomRight, Vec3 topLeft,
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
		collector.submitCustom(SubmitRenderPhases.TRANSLUCENT_CUSTOM_GEOMETRY, new TexturedSubmit(pose, normal,
				new Corner(p00, u0, v0), new Corner(p10, u1, v0),
				new Corner(p11, u1, v1), new Corner(p01, u0, v1)));
	}

	/**
	 * Submits one rectangle given in panel coordinates. Both ranges run from 0 to 1
	 * across the panel: s along the bottom edge, t up the side.
	 */
	private static void submitRect(SubmitNodeCollector collector, PoseStack.Pose pose, int color,
			Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft,
			float s0, float s1, float t0, float t1) {
		collector.submitCustom(SubmitRenderPhases.SOLID, new QuadSubmit(pose, color,
				at(bottomLeft, bottomRight, topLeft, s0, t0),
				at(bottomLeft, bottomRight, topLeft, s1, t0),
				at(bottomLeft, bottomRight, topLeft, s1, t1),
				at(bottomLeft, bottomRight, topLeft, s0, t1)));
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

	/** One quad waiting to be drawn in the SOLID phase. */
	record QuadSubmit(PoseStack.Pose pose, int color, Vec3 bottomLeft, Vec3 bottomRight,
			Vec3 topRight, Vec3 topLeft) implements SubmitNode {
		@Override
		public FeatureRendererType<? extends SubmitNode> featureType() {
			return TYPE;
		}
	}

	/** One corner of a textured quad: a world position and its texture coordinates. */
	record Corner(Vec3 point, float u, float v) {
	}

	/** One textured quad waiting to be drawn in the translucent phase. */
	record TexturedSubmit(PoseStack.Pose pose, Vec3 normal, Corner c0, Corner c1, Corner c2, Corner c3)
			implements SubmitNode {
		@Override
		public FeatureRendererType<? extends SubmitNode> featureType() {
			return TEXTURED_TYPE;
		}
	}

	private static final class Renderer extends RenderTypeFeatureRenderer<QuadSubmit> {
		@Override
		protected void buildGroup(FeatureFrameContext context, List<QuadSubmit> submits) {
			if (submits.isEmpty()) {
				return;
			}
			VertexConsumer buffer = getVertexBuilder(RenderTypes.debugFilledBox());
			for (QuadSubmit submit : submits) {
				vertex(buffer, submit, submit.bottomLeft());
				vertex(buffer, submit, submit.bottomRight());
				vertex(buffer, submit, submit.topRight());
				vertex(buffer, submit, submit.topLeft());
			}
		}

		private static void vertex(VertexConsumer buffer, QuadSubmit submit, Vec3 point) {
			buffer.addVertex(submit.pose(), (float) point.x, (float) point.y, (float) point.z)
					.setColor(submit.color());
		}
	}

	private static final class TexturedRenderer extends RenderTypeFeatureRenderer<TexturedSubmit> {
		@Override
		protected void buildGroup(FeatureFrameContext context, List<TexturedSubmit> submits) {
			if (submits.isEmpty()) {
				return;
			}
			// No outline: the HUD band should not glow when the player is outlined.
			VertexConsumer buffer = getVertexBuilder(
					RenderTypes.entityTranslucent(CapturedHudTexture.ID, false));
			for (TexturedSubmit submit : submits) {
				vertex(buffer, submit, submit.c0());
				vertex(buffer, submit, submit.c1());
				vertex(buffer, submit, submit.c2());
				vertex(buffer, submit, submit.c3());
			}
		}

		private static void vertex(VertexConsumer buffer, TexturedSubmit submit, Corner corner) {
			Vec3 n = submit.normal();
			Vec3 p = corner.point();
			buffer.addVertex(submit.pose(), (float) p.x, (float) p.y, (float) p.z)
					.setColor(0xFFFFFFFF)
					.setUv(corner.u(), corner.v())
					.setOverlay(OverlayTexture.NO_OVERLAY)
					.setLight(FULL_BRIGHT)
					.setNormal((float) n.x, (float) n.y, (float) n.z);
		}
	}
}
