package dev.arena.spatialhud;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import com.mojang.renderpearl.api.pipeline.PrimitiveTopology;
import com.mojang.renderpearl.api.pipeline.RenderPipeline;
import com.mojang.renderpearl.api.textures.AddressMode;
import com.mojang.renderpearl.api.textures.FilterMode;
import com.mojang.renderpearl.api.textures.GpuTextureView;
import com.mojang.renderpearl.api.vertex.VertexFormat;
import net.fabricmc.fabric.api.client.rendering.v1.level.LevelExtractionContext;
import net.fabricmc.fabric.api.client.rendering.v1.level.LevelExtractionEvents;
import net.fabricmc.fabric.api.client.rendering.v1.level.LevelRenderContext;
import net.fabricmc.fabric.api.client.rendering.v1.level.LevelRenderEvents;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.client.renderer.RenderPipelines;
import net.minecraft.client.renderer.StagedVertexBuffer;
import net.minecraft.client.renderer.rendertype.RenderType;
import net.minecraft.resources.Identifier;
import net.minecraft.world.phys.Vec3;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Vector3f;
import org.joml.Vector4f;

import java.util.Optional;
import java.util.OptionalDouble;

/**
 * Method 3: presents the completed selected-HUD texture on a quad in the
 * rendered level rather than compositing it in GUI coordinates.
 *
 * <p>The current frame's HUD is captured after level drawing, so the level
 * event intentionally presents the completed texture from the preceding frame.
 * This adds at most one frame of display latency and avoids redirecting any
 * normal GUI or world renderer.</p>
 */
public final class WorldSpaceHudRenderer {
	private static final Identifier THROUGH_WORLD_PIPELINE_ID =
			Identifier.fromNamespaceAndPath("spatialhud", "world_texture_through_world");

	/*
	 * Keep GPU resources uninitialized until Method 3 is actually selected.
	 * Method 1 and Method 2 must be able to launch in an Iris-heavy profile
	 * without asking that renderer to compile or override an unused world pass.
	 */
	private static RenderPipeline throughWorldPipeline;
	private static StagedVertexBuffer buffer;
	private static final Vector4f WHITE = new Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
	private static final Vector3f ZERO = new Vector3f();
	private static final Matrix4f IDENTITY = new Matrix4f();
	private static boolean initialized;
	private static PlaneState planeState;

	private WorldSpaceHudRenderer() {
	}

	static void initialize() {
		if (initialized) {
			return;
		}
		initialized = true;
		LevelExtractionEvents.END_EXTRACTION.register(WorldSpaceHudRenderer::extractPlane);
		LevelRenderEvents.AFTER_TRANSLUCENT_TERRAIN.register(WorldSpaceHudRenderer::renderPlane);
	}

	/** Only Method 3 asks Iris/the active renderer to prepare this custom pass. */
	private static RenderPipeline throughWorldPipeline() {
		if (throughWorldPipeline == null) {
			throughWorldPipeline = RenderPipelines.register(
					RenderPipeline.builder(RenderPipelines.ENTITY_SNIPPET)
							.withLocation(THROUGH_WORLD_PIPELINE_ID)
							.withDepthStencilState(Optional.empty())
							.withCull(false)
							.build());
		}
		return throughWorldPipeline;
	}

	private static StagedVertexBuffer buffer() {
		if (buffer == null) {
			buffer = new StagedVertexBuffer(
					() -> "Spatial HUD world-space texture", RenderType.SMALL_BUFFER_SIZE);
		}
		return buffer;
	}

	/**
	 * Reads player and configuration state during level extraction, then stores
	 * four immutable world points for the later GPU drawing phase.
	 */
	private static void extractPlane(LevelExtractionContext ignored) {
		if (!SpatialHud.isWorldSpaceTextureActive()) {
			planeState = null;
			return;
		}

		Minecraft mc = Minecraft.getInstance();
		LocalPlayer player = mc.player;
		if (player == null) {
			planeState = null;
			return;
		}

		SpatialHudConfig cfg = SpatialHudConfig.get();
		float anchorYaw = cfg.worldSpaceAnchor == SpatialHudConfig.WorldSpaceAnchor.PLAYER_BODY
				? player.yBodyRot
				: player.getYRot();
		float yaw = radians(anchorYaw);
		float rightX = cos(yaw);
		float rightZ = sin(yaw);
		float forwardX = -sin(yaw);
		float forwardZ = cos(yaw);

		// virtualYaw turns the panel within the selected horizontal anchor frame.
		float panelYaw = radians(clamp(cfg.virtualYaw, -80, 80));
		float panelRightX = rightX * cos(panelYaw) + forwardX * sin(panelYaw);
		float panelRightZ = rightZ * cos(panelYaw) + forwardZ * sin(panelYaw);
		float panelForwardX = -rightX * sin(panelYaw) + forwardX * cos(panelYaw);
		float panelForwardZ = -rightZ * sin(panelYaw) + forwardZ * cos(panelYaw);

		Vec3 eye = player.getEyePosition();
		float centerX = (float) eye.x + rightX * (float) cfg.virtualOffsetX
				+ forwardX * (float) Math.max(0.10, cfg.distance);
		float centerY = (float) eye.y + (float) cfg.virtualOffsetY;
		float centerZ = (float) eye.z + rightZ * (float) cfg.virtualOffsetX
				+ forwardZ * (float) Math.max(0.10, cfg.distance);

		float width = clamp((float) cfg.planeWidth, 0.10f, 6.0f);
		float height = width * (VirtualHudPlane.SOURCE_TOP_FROM_BOTTOM
				+ VirtualHudPlane.SOURCE_BOTTOM_BELOW_SCREEN) / (VirtualHudPlane.SOURCE_HALF_WIDTH * 2.0f);
		float pitch = radians(clamp(cfg.virtualFaceOnLookDownPitch + cfg.virtualPitch, -80, 80));
		// This top vector is the world equivalent of VirtualHudPlane's fixed
		// horizon-space pitch: the upper edge shifts farther forward.
		float topX = panelForwardX * sin(pitch);
		float topY = cos(pitch);
		float topZ = panelForwardZ * sin(pitch);

		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;
		Point bottomLeft = point(centerX - panelRightX * halfWidth - topX * halfHeight,
				centerY - topY * halfHeight,
				centerZ - panelRightZ * halfWidth - topZ * halfHeight);
		Point bottomRight = point(centerX + panelRightX * halfWidth - topX * halfHeight,
				centerY - topY * halfHeight,
				centerZ + panelRightZ * halfWidth - topZ * halfHeight);
		Point topRight = point(centerX + panelRightX * halfWidth + topX * halfHeight,
				centerY + topY * halfHeight,
				centerZ + panelRightZ * halfWidth + topZ * halfHeight);
		Point topLeft = point(centerX - panelRightX * halfWidth + topX * halfHeight,
				centerY + topY * halfHeight,
				centerZ - panelRightZ * halfWidth + topZ * halfHeight);
		planeState = new PlaneState(bottomLeft, bottomRight, topRight, topLeft,
				cfg.worldSpaceOccludeBehindWorld);
	}

	private static void renderPlane(LevelRenderContext context) {
		PlaneState state = planeState;
		GpuTextureView texture = ExperimentalHudCapture.worldTextureView();
		if (state == null || texture == null) {
			return;
		}

		try {
			RenderPipeline pipeline = state.occludeBehindWorld()
					? RenderPipelines.ENTITY_TRANSLUCENT
					: throughWorldPipeline();
			VertexFormat format = pipeline.getVertexFormatBinding(0);
			PrimitiveTopology primitive = pipeline.getPrimitiveTopology();
			if (format == null || primitive != PrimitiveTopology.QUADS) {
				throw new IllegalStateException("world-space HUD pipeline does not expose textured QUADS");
			}

			StagedVertexBuffer buffer = buffer();
			StagedVertexBuffer.Draw draw = buffer.appendDraw(format, primitive,
					RenderSystem.getProjectionType().vertexSorting());
			try {
				PoseStack matrices = context.poseStack();
				Vec3 camera = context.levelState().cameraRenderState.pos;
				matrices.pushPose();
				try {
					matrices.translate(-camera.x, -camera.y, -camera.z);
					addQuad(buffer.getVertexBuilder(draw), matrices.last().pose(), state);
				} finally {
					matrices.popPose();
				}
				buffer.upload();
				StagedVertexBuffer.ExecuteInfo info = buffer.getExecuteInfo(draw);
				if (info != null) {
					drawToLevelTarget(info, pipeline, texture, state.occludeBehindWorld());
				}
			} finally {
				buffer.endFrame();
			}
		} catch (Throwable t) {
			ExperimentalHudCapture.worldTextureFailed(t);
		}
	}

	private static void addQuad(VertexConsumer vertices, Matrix4fc matrix, PlaneState state) {
		// Entity-texture pipelines use UV0, overlay UV1, lightmap UV2, and a
		// normal. Full-bright keeps the HUD legible under Method 3 lighting.
		addVertex(vertices, matrix, state.bottomLeft(), 0.0f, 1.0f);
		addVertex(vertices, matrix, state.bottomRight(), 1.0f, 1.0f);
		addVertex(vertices, matrix, state.topRight(), 1.0f, 0.0f);
		addVertex(vertices, matrix, state.topLeft(), 0.0f, 0.0f);
	}

	private static void addVertex(VertexConsumer vertices, Matrix4fc matrix, Point point, float u, float v) {
		vertices.addVertex(matrix, point.x(), point.y(), point.z())
				.setColor(255, 255, 255, 255)
				.setUv(u, v)
				.setUv1(0, 10)
				.setUv2(240, 240)
				.setNormal(0.0f, 1.0f, 0.0f);
	}

	private static void drawToLevelTarget(StagedVertexBuffer.ExecuteInfo info, RenderPipeline pipeline,
			GpuTextureView texture, boolean occludeBehindWorld) {
		Minecraft mc = Minecraft.getInstance();
		RenderTarget target = mc.gameRenderer.mainRenderTarget();
		GpuTextureView output = target.getColorTextureView();
		if (output == null) {
			throw new IllegalStateException("world-space HUD target has no color texture view");
		}
		if (occludeBehindWorld && target.getDepthTextureView() == null) {
			throw new IllegalStateException("world-space HUD requested depth occlusion without a depth texture");
		}

		GpuBufferSlice transforms = RenderSystem.getDynamicUniforms().writeTransform(
				RenderSystem.getModelViewMatrixCopy(), WHITE, ZERO, IDENTITY);
		try (var pass = RenderSystem.getDevice().createCommandEncoder().createRenderPass(
				() -> "Spatial HUD world-space texture",
				output,
				Optional.empty(),
				occludeBehindWorld ? target.getDepthTextureView() : null,
				OptionalDouble.empty())) {
			pass.setPipeline(RenderSystem.getCompiledPipeline(pipeline));
			RenderSystem.bindDefaultUniforms(pass);
			pass.setUniform("DynamicTransforms", transforms);
			pass.setUniform("Sampler0", texture, RenderSystem.getSamplerCache().getSampler(
					AddressMode.CLAMP_TO_EDGE,
					AddressMode.CLAMP_TO_EDGE,
					FilterMode.NEAREST,
					FilterMode.NEAREST,
					false));
			pass.setVertexBuffer(0, info.vertexBuffer().slice());
			pass.setIndexBuffer(info.indexBuffer(), info.indexType());
			pass.drawIndexed(info.indexCount(), 1, info.firstIndex(), info.baseVertex(), 0);
		}
	}

	public static void close() {
		if (buffer != null) {
			buffer.close();
			buffer = null;
		}
	}

	private record Point(float x, float y, float z) {
	}

	private record PlaneState(Point bottomLeft, Point bottomRight, Point topRight, Point topLeft,
						  boolean occludeBehindWorld) {
	}

	private static Point point(float x, float y, float z) {
		return new Point(x, y, z);
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
		return value < min ? min : Math.min(value, max);
	}
}
