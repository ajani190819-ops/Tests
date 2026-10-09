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
import net.minecraft.world.phys.Vec3;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Vector3f;
import org.joml.Vector4f;

import java.util.Optional;
import java.util.OptionalDouble;

/**
 * Both purple panel methods present the completed selected-HUD texture on a
 * quad in the rendered level rather than compositing it in GUI coordinates.
 * Method 3 is the camera-yaw flat map; Method 4 is the purple horizontal panel.
 *
 * <p>The current frame's HUD is captured after level drawing, so the level
 * event intentionally presents the completed texture from the preceding frame.
 * This adds at most one frame of display latency and avoids redirecting any
 * normal GUI or world renderer.</p>
 */
public final class WorldSpaceHudRenderer {
	/*
	 * The world-space method intentionally uses only vanilla precompiled
	 * pipelines. A custom pipeline caused F5W/Iris resource reload failure when
	 * the entity snippet declared sampler uniforms the shader did not provide.
	 */
	private static StagedVertexBuffer buffer;
	/** True once renderPlane has used the buffer this frame, so endFrame has work. */
	private static boolean bufferUsedThisFrame;
	private static final Vector4f WHITE = new Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
	private static final Vector3f ZERO = new Vector3f();
	private static final Matrix4f IDENTITY = new Matrix4f();
	private static boolean initialized;
	private static PlaneState planeState;
	/** Vertices already uploaded during extraction, drawn later in the level pass. */
	private static StagedVertexBuffer.ExecuteInfo planeDraw;
	private static RenderPipeline planePipeline;

	private WorldSpaceHudRenderer() {
	}

	static void initialize() {
		if (initialized) {
			return;
		}
		initialized = true;
		LevelExtractionEvents.END_EXTRACTION.register(WorldSpaceHudRenderer::extractPlane);
		WorldSpaceSolidQuad.register();
		// Roadmap stage 0: submit the quad to the level's own collector instead of
		// opening a render pass inside the translucent pass, which the game refuses.
		LevelRenderEvents.COLLECT_SUBMITS.register(WorldSpaceHudRenderer::submitPlane);
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
	private static void extractPlane(LevelExtractionContext context) {
		try {
			// The frame's partial tick, so the panel moves between game ticks as the
			// camera does instead of stepping 20 times a second.
			computePlaneState(context.deltaTracker().getGameTimeDeltaPartialTick(false));
		} catch (Throwable t) {
			planeState = null;
			ExperimentalHudCapture.worldTextureFailed(t);
		}
	}

	/**
	 * Roadmap stages 0 to 3. Submits the Method 4 panel: the purple fill (when
	 * enabled), the captured HUD band on top of it, and the white border. Only
	 * Method 4 is drawn for now, following the roadmap order.
	 */
	private static void submitPlane(LevelRenderContext context) {
		PlaneState state = planeState;
		if (state == null || !SpatialHudConfig.get().usesPurplePanel()) {
			return;
		}
		try {
			Vec3 camera = context.levelState().cameraRenderState.pos;
			Vec3 bottomLeft = new Vec3(state.bottomLeft().x(), state.bottomLeft().y(), state.bottomLeft().z());
			Vec3 bottomRight = new Vec3(state.bottomRight().x(), state.bottomRight().y(), state.bottomRight().z());
			Vec3 topRight = new Vec3(state.topRight().x(), state.topRight().y(), state.topRight().z());
			Vec3 topLeft = new Vec3(state.topLeft().x(), state.topLeft().y(), state.topLeft().z());
			WorldSpaceSolidQuad.submitPanel(context.submitNodeCollector(), camera,
					SpatialHudConfig.get().horizontalPanelFill,
					bottomLeft, bottomRight, topRight, topLeft);
			// Roadmap stage 3: the captured HUD, only once a frame of it exists.
			if (ExperimentalHudCapture.worldTextureView() != null) {
				CapturedHudTexture.register();
				WorldSpaceSolidQuad.submitCapturedBand(context.submitNodeCollector(), camera,
						state.uLeft(), state.uRight(), state.vTop(), state.vBottom(),
						bottomLeft, bottomRight, topRight, topLeft);
			}
		} catch (Throwable t) {
			planeState = null;
			ExperimentalHudCapture.worldTextureFailed(t);
		}
	}

	private static void computePlaneState(float partialTick) {
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
		if (cfg.usesPurplePanel()) {
			planeState = purplePanelState(player, cfg, partialTick,
					mc.getWindow().getGuiScaledWidth(), mc.getWindow().getGuiScaledHeight());
			return;
		}

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
		float pitch = radians(clamp(cfg.virtualFaceOnLookDownPitch + cfg.virtualPitch, -89, 89));
		// This top vector is the world equivalent of VirtualHudPlane's fixed
		// horizon-space pitch: the upper edge shifts farther forward.
		float topX = panelForwardX * sin(pitch);
		float topY = cos(pitch);
		float topZ = panelForwardZ * sin(pitch);

		// Roll is around the panel normal, not the player vertical. Positive
		// roll raises the right edge: apply it by rotating the established right
		// and top basis vectors before emitting real world-space corners.
		float roll = radians(clamp(cfg.virtualRoll, -45, 45));
		float rolledRightX = panelRightX * cos(roll) + topX * sin(roll);
		float rolledRightY = topY * sin(roll);
		float rolledRightZ = panelRightZ * cos(roll) + topZ * sin(roll);
		float rolledTopX = topX * cos(roll) - panelRightX * sin(roll);
		float rolledTopY = topY * cos(roll);
		float rolledTopZ = topZ * cos(roll) - panelRightZ * sin(roll);

		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;
		Point bottomLeft = point(centerX - rolledRightX * halfWidth - rolledTopX * halfHeight,
				centerY - rolledRightY * halfWidth - rolledTopY * halfHeight,
				centerZ - rolledRightZ * halfWidth - rolledTopZ * halfHeight);
		Point bottomRight = point(centerX + rolledRightX * halfWidth - rolledTopX * halfHeight,
				centerY + rolledRightY * halfWidth - rolledTopY * halfHeight,
				centerZ + rolledRightZ * halfWidth - rolledTopZ * halfHeight);
		Point topRight = point(centerX + rolledRightX * halfWidth + rolledTopX * halfHeight,
				centerY + rolledRightY * halfWidth + rolledTopY * halfHeight,
				centerZ + rolledRightZ * halfWidth + rolledTopZ * halfHeight);
		Point topLeft = point(centerX - rolledRightX * halfWidth + rolledTopX * halfHeight,
				centerY - rolledRightY * halfWidth + rolledTopY * halfHeight,
				centerZ - rolledRightZ * halfWidth + rolledTopZ * halfHeight);
		// The isolated capture target is full-window sized. Method 3 must sample
		// only the selected lower-HUD source rectangle, rather
		// than shrinking that rectangle into the bottom of an otherwise blank quad.
		VirtualHudPlane source = VirtualHudPlane.forGui(cfg,
				mc.getWindow().getGuiScaledWidth(), mc.getWindow().getGuiScaledHeight());
		planeState = new PlaneState(bottomLeft, bottomRight, topRight, topLeft,
				source.textureU(source.sourceLeft()), source.textureU(source.sourceRight()),
				source.textureV(source.sourceTop()), source.textureV(source.sourceBottom()),
				cfg.worldSpaceOccludeBehindWorld);
	}

	/**
	 * Method 4: a purple horizontal panel anchored to the player's feet and body
	 * heading, so turning the head does not move it. Its bottom edge (the
	 * hotbar side) is nearest the player and its top edge points away. The
	 * angle sets the look-down pitch at which the panel faces you square-on:
	 * 90 lies it flat, and smaller angles tilt its near edge toward you.
	 */
	/** Smoothed Method 4 placement for the wiggle option. Updated once per frame. */
	private static float wiggleYaw;
	private static double wiggleX;
	private static double wiggleY;
	private static double wiggleZ;
	private static double wiggleHeight;
	private static boolean wiggleReady;
	private static long wiggleLastNanos;

	/**
	 * Moves the smoothed placement toward the player's current placement. Each part
	 * ticked in the config eases toward its target with a time constant of
	 * {@code panelWiggleSeconds}, so the panel lags and then catches up. Parts not
	 * ticked, and every part when the wiggle is off, follow the target exactly.
	 */
	private static void updateWiggle(SpatialHudConfig cfg, float targetYaw, Vec3 targetFeet, double targetHeight) {
		long now = System.nanoTime();
		double dt = wiggleLastNanos == 0 ? 0.0 : Math.min(0.1, Math.max(0.0, (now - wiggleLastNanos) / 1.0e9));
		wiggleLastNanos = now;
		double jump = Math.hypot(targetFeet.x - wiggleX, targetFeet.z - wiggleZ);
		if (!wiggleReady || !cfg.panelWiggle || dt <= 0.0 || jump > 4.0) {
			// A teleport or a first frame snaps, so the panel never sweeps across the map.
			wiggleYaw = targetYaw;
			wiggleX = targetFeet.x;
			wiggleY = targetFeet.y;
			wiggleZ = targetFeet.z;
			wiggleHeight = targetHeight;
			wiggleReady = true;
			return;
		}
		double tau = Math.max(0.05, Math.min(1.0, cfg.panelWiggleSeconds));
		double alpha = 1.0 - Math.exp(-dt / tau);
		wiggleYaw = cfg.panelWiggleHeading
				? wiggleYaw + wrapRadians(targetYaw - wiggleYaw) * (float) alpha
				: targetYaw;
		wiggleX = cfg.panelWigglePosition ? wiggleX + (targetFeet.x - wiggleX) * alpha : targetFeet.x;
		wiggleY = cfg.panelWigglePosition ? wiggleY + (targetFeet.y - wiggleY) * alpha : targetFeet.y;
		wiggleZ = cfg.panelWigglePosition ? wiggleZ + (targetFeet.z - wiggleZ) * alpha : targetFeet.z;
		wiggleHeight = cfg.panelWiggleHeight ? wiggleHeight + (targetHeight - wiggleHeight) * alpha : targetHeight;
	}

	/** Wraps an angle to the range minus pi to pi, so the shortest turn is used. */
	private static float wrapRadians(float angle) {
		float a = angle;
		while (a > Math.PI) {
			a -= (float) (2.0 * Math.PI);
		}
		while (a < -Math.PI) {
			a += (float) (2.0 * Math.PI);
		}
		return a;
	}

	private static PlaneState purplePanelState(LocalPlayer player, SpatialHudConfig cfg, float partialTick,
			int guiWidth, int guiHeight) {
		ExperimentalHudCapture.SourceRect band = ExperimentalHudCapture.purpleSourceRect(cfg, guiWidth, guiHeight);
		// Method 4's own heading setting: body heading by default, or the camera's
		// horizontal view when the config says so.
		// Interpolated to the partial tick, like the camera, so it turns smoothly.
		float targetYaw = cfg.horizontalPanelAnchor == SpatialHudConfig.HorizontalPanelAnchor.CAMERA_YAW
				? radians(player.getViewYRot(partialTick))
				: radians(net.minecraft.util.Mth.rotLerp(partialTick, player.yBodyRotO, player.yBodyRot));
		Vec3 targetFeet = player.getPosition(partialTick);
		updateWiggle(cfg, targetYaw, targetFeet, cfg.horizontalPanelHeight);
		float bodyYaw = wiggleYaw;
		float rightX = cos(bodyYaw);
		float rightZ = sin(bodyYaw);
		float forwardX = -sin(bodyYaw);
		float forwardZ = cos(bodyYaw);

		float distance = (float) Math.max(0.10, cfg.horizontalPanelDistance);
		float centerX = (float) wiggleX + forwardX * distance;
		float centerY = (float) wiggleY + (float) wiggleHeight;
		float centerZ = (float) wiggleZ + forwardZ * distance;

		// The panel is exactly the sampled band, so its aspect matches the HUD.
		float width = clamp((float) cfg.planeWidth, 0.10f, 6.0f);
		float height = width * (band.bottom() - band.top()) / (float) (band.right() - band.left());
		float lookDown = radians(clamp(cfg.horizontalPanelAngle, 20, 90));
		// Unit vector along the panel's height, lying in the plane that faces
		// the player at the chosen look-down angle. At 90 it is the forward
		// direction, so the panel is exactly horizontal (normal along Y).
		float topX = forwardX * sin(lookDown);
		float topY = cos(lookDown);
		float topZ = forwardZ * sin(lookDown);

		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;
		Point bottomLeft = point(centerX - rightX * halfWidth - topX * halfHeight,
				centerY - topY * halfHeight,
				centerZ - rightZ * halfWidth - topZ * halfHeight);
		Point bottomRight = point(centerX + rightX * halfWidth - topX * halfHeight,
				centerY - topY * halfHeight,
				centerZ + rightZ * halfWidth - topZ * halfHeight);
		Point topRight = point(centerX + rightX * halfWidth + topX * halfHeight,
				centerY + topY * halfHeight,
				centerZ + rightZ * halfWidth + topZ * halfHeight);
		Point topLeft = point(centerX - rightX * halfWidth + topX * halfHeight,
				centerY + topY * halfHeight,
				centerZ - rightZ * halfWidth + topZ * halfHeight);
		// Always depth-tested: a flat sheet in the world must not show through
		// walls or terrain.
		return new PlaneState(bottomLeft, bottomRight, topRight, topLeft,
				band.left() / (float) guiWidth, band.right() / (float) guiWidth,
				band.top() / (float) guiHeight, band.bottom() / (float) guiHeight,
				true);
	}

	private static void renderPlane(LevelRenderContext context) {
		StagedVertexBuffer.ExecuteInfo info = planeDraw;
		RenderPipeline pipeline = planePipeline;
		PlaneState state = planeState;
		planeDraw = null;
		planePipeline = null;
		GpuTextureView texture = ExperimentalHudCapture.worldTextureView();
		if (info == null || pipeline == null || state == null || texture == null) {
			return;
		}
		try {
			// Only the draw happens here. The vertices were uploaded during
			// extraction, because an upload inside this translucent pass is refused.
			drawToLevelTarget(info, pipeline, texture, state.occludeBehindWorld());
		} catch (Throwable t) {
			ExperimentalHudCapture.worldTextureFailed(t);
		}
	}

	/**
	 * Writes the quad's vertices to the GPU buffer during extraction, when no
	 * render pass is open. The vertices are relative to the camera, so the
	 * draw in the level pass needs no pose stack.
	 */
	private static void stagePlaneGeometry(Vec3 camera) {
		PlaneState state = planeState;
		if (state == null) {
			return;
		}
		if (ExperimentalHudCapture.worldTextureView() == null) {
			return;
		}
		RenderPipeline pipeline = state.occludeBehindWorld()
				? RenderPipelines.ENTITY_TRANSLUCENT
				: RenderPipelines.GUI_TEXTURED;
		VertexFormat format = pipeline.getVertexFormatBinding(0);
		PrimitiveTopology primitive = pipeline.getPrimitiveTopology();
		if (format == null || primitive != PrimitiveTopology.QUADS) {
			throw new IllegalStateException("world-space HUD pipeline does not expose textured QUADS");
		}
		StagedVertexBuffer buffer = buffer();
		bufferUsedThisFrame = true;
		StagedVertexBuffer.Draw draw = buffer.appendDraw(format, primitive,
				RenderSystem.getProjectionType().vertexSorting());
		Matrix4f toCamera = new Matrix4f().translation((float) -camera.x, (float) -camera.y, (float) -camera.z);
		addQuad(buffer.getVertexBuilder(draw), toCamera, state, state.occludeBehindWorld());
		buffer.upload();
		planeDraw = buffer.getExecuteInfo(draw);
		planePipeline = pipeline;
	}

	/**
	 * Closes the staged buffer's frame. It must not run inside renderPlane:
	 * that callback is inside the level's translucent render pass, and the
	 * buffer's end-of-frame fence is refused while a pass is open. The
	 * GameRenderer hook calls this after the level pass has finished, the same
	 * point where the capture composite already ends its own frame.
	 */
	public static void endFrame() {
		if (!bufferUsedThisFrame || buffer == null) {
			return;
		}
		bufferUsedThisFrame = false;
		try {
			buffer.endFrame();
		} catch (Throwable t) {
			ExperimentalHudCapture.worldTextureFailed(t);
		}
	}

	private static void addQuad(VertexConsumer vertices, Matrix4fc matrix, PlaneState state,
			boolean occludeBehindWorld) {
		if (occludeBehindWorld) {
			// ENTITY_TRANSLUCENT uses UV0, overlay UV1, lightmap UV2, and a normal.
			// Full-bright keeps the HUD legible on the depth-tested world plane.
			addEntityVertex(vertices, matrix, state.bottomLeft(), state.uLeft(), state.vBottom());
			addEntityVertex(vertices, matrix, state.bottomRight(), state.uRight(), state.vBottom());
			addEntityVertex(vertices, matrix, state.topRight(), state.uRight(), state.vTop());
			addEntityVertex(vertices, matrix, state.topLeft(), state.uLeft(), state.vTop());
		} else {
			// GUI_TEXTURED is a vanilla no-depth texture pipeline, with the same
			// simple position/color/UV layout used by the GUI mesh renderer.
			addGuiVertex(vertices, matrix, state.bottomLeft(), state.uLeft(), state.vBottom());
			addGuiVertex(vertices, matrix, state.bottomRight(), state.uRight(), state.vBottom());
			addGuiVertex(vertices, matrix, state.topRight(), state.uRight(), state.vTop());
			addGuiVertex(vertices, matrix, state.topLeft(), state.uLeft(), state.vTop());
		}
	}

	private static void addEntityVertex(VertexConsumer vertices, Matrix4fc matrix, Point point, float u, float v) {
		vertices.addVertex(matrix, point.x(), point.y(), point.z())
				.setColor(255, 255, 255, 255)
				.setUv(u, v)
				.setUv1(0, 10)
				.setUv2(240, 240)
				.setNormal(0.0f, 1.0f, 0.0f);
	}

	private static void addGuiVertex(VertexConsumer vertices, Matrix4fc matrix, Point point, float u, float v) {
		vertices.addVertex(matrix, point.x(), point.y(), point.z())
				.setColor(255, 255, 255, 255)
				.setUv(u, v);
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
						  float uLeft, float uRight, float vTop, float vBottom,
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
