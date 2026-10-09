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
import net.minecraft.world.phys.Vec3;

import java.util.List;

/**
 * Draws the Method 4 panel as solid quads in the world, through the level's own
 * submit collector.
 *
 * <p>The earlier world-space draw opened its own render pass inside the level's
 * translucent pass, and the game refused it. Submitting a node instead lets the
 * level draw the quads in its own pass, the way Fabric's test mods do. The quads
 * are submitted to the SOLID phase, so they are depth-tested and walls can hide
 * them.</p>
 *
 * <p>The panel is a purple fill with a white border ring. The fill is shrunk to
 * the inner rectangle and the border is four strips around it, so the two never
 * overlap and cannot z-fight. The corners still come from
 * {@link WorldSpaceHudRenderer}.</p>
 */
final class WorldSpaceSolidQuad {
	/** Opaque purple, the same as the Method 4 failure marker. */
	static final int COLOR = 0xFFC75CFF;
	/** Opaque white border. */
	static final int BORDER_COLOR = 0xFFFFFFFF;
	/** Border width as a fraction of the panel's width and height. */
	static final float BORDER_FRACTION = 0.03f;

	private static final FeatureRendererType<QuadSubmit> TYPE =
			FeatureRendererType.create("spatialhud_solid_quad");
	private static boolean registered;

	private WorldSpaceSolidQuad() {
	}

	/** Registers the feature renderer. Safe to call more than once. */
	static void register() {
		if (registered) {
			return;
		}
		registered = true;
		FeatureRendererRegistry.register(TYPE, Renderer::new);
	}

	/**
	 * Submits the panel: the purple fill (when enabled), then the white border ring. The corners
	 * are absolute world coordinates, given in the order bottom-left, bottom-right,
	 * top-right, top-left. The panel is a parallelogram, so each point inside it is
	 * found by interpolating along the two edges.
	 */
	static void submitPanel(SubmitNodeCollector collector, Vec3 camera, boolean showFill,
			Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft) {
		PoseStack toCamera = new PoseStack();
		toCamera.translate(-camera.x, -camera.y, -camera.z);
		PoseStack.Pose pose = toCamera.last().copy();

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

	/** The point at (s, t) on the panel: bottom-left plus s along the bottom and t up. */
	private static Vec3 at(Vec3 bottomLeft, Vec3 bottomRight, Vec3 topLeft, float s, float t) {
		return new Vec3(
				bottomLeft.x + (bottomRight.x - bottomLeft.x) * s + (topLeft.x - bottomLeft.x) * t,
				bottomLeft.y + (bottomRight.y - bottomLeft.y) * s + (topLeft.y - bottomLeft.y) * t,
				bottomLeft.z + (bottomRight.z - bottomLeft.z) * s + (topLeft.z - bottomLeft.z) * t);
	}

	/** One quad waiting to be drawn in the SOLID phase. */
	record QuadSubmit(PoseStack.Pose pose, int color, Vec3 bottomLeft, Vec3 bottomRight,
			Vec3 topRight, Vec3 topLeft) implements SubmitNode {
		@Override
		public FeatureRendererType<? extends SubmitNode> featureType() {
			return TYPE;
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
}
