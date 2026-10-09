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
 * Roadmap stage 0: draws one solid quad in the world through the level's own
 * submit collector.
 *
 * <p>The earlier world-space draw opened its own render pass inside the level's
 * translucent pass, and the game refused it. Submitting a node instead lets the
 * level draw the quad in its own pass, the way Fabric's test mods do. The quad is
 * submitted to the SOLID phase, so it is depth-tested and walls can hide it.</p>
 *
 * <p>Only the draw path changes here. The corners still come from
 * {@link WorldSpaceHudRenderer}. This class is a test of the route, not the final
 * outline or texture.</p>
 */
final class WorldSpaceSolidQuad {
	/** Opaque purple, the same as the Method 4 failure marker. */
	static final int COLOR = 0xFFC75CFF;

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
	 * Submits one quad. The corners are absolute world coordinates, given in the
	 * order bottom-left, bottom-right, top-right, top-left. The camera offset
	 * goes in the pose, as the level's own renderers do.
	 */
	static void submit(SubmitNodeCollector collector, Vec3 camera,
			Vec3 bottomLeft, Vec3 bottomRight, Vec3 topRight, Vec3 topLeft) {
		PoseStack toCamera = new PoseStack();
		toCamera.translate(-camera.x, -camera.y, -camera.z);
		PoseStack.Pose pose = toCamera.last().copy();
		collector.submitCustom(SubmitRenderPhases.SOLID,
				new QuadSubmit(pose, bottomLeft, bottomRight, topRight, topLeft));
	}

	/** One quad waiting to be drawn in the SOLID phase. */
	record QuadSubmit(PoseStack.Pose pose, Vec3 bottomLeft, Vec3 bottomRight,
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
				vertex(buffer, submit.pose(), submit.bottomLeft());
				vertex(buffer, submit.pose(), submit.bottomRight());
				vertex(buffer, submit.pose(), submit.topRight());
				vertex(buffer, submit.pose(), submit.topLeft());
			}
		}

		private static void vertex(VertexConsumer buffer, PoseStack.Pose pose, Vec3 point) {
			buffer.addVertex(pose, (float) point.x, (float) point.y, (float) point.z).setColor(COLOR);
		}
	}
}
