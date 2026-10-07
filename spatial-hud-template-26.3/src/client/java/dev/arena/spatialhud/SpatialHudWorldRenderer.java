package dev.arena.spatialhud;

import com.mojang.blaze3d.ProjectionType;
import com.mojang.blaze3d.pipeline.TextureTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.renderpearl.api.GpuFormat;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import com.mojang.renderpearl.api.pipeline.PrimitiveTopology;
import com.mojang.renderpearl.api.pipeline.RenderPipeline;
import com.mojang.renderpearl.api.textures.AddressMode;
import com.mojang.renderpearl.api.textures.FilterMode;
import com.mojang.renderpearl.api.textures.GpuTextureView;
import com.mojang.renderpearl.api.vertex.VertexFormat;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.render.GuiRenderer;
import net.minecraft.client.renderer.RenderPipelines;
import net.minecraft.client.renderer.StagedVertexBuffer;
import net.minecraft.client.renderer.rendertype.RenderType;
import net.minecraft.client.renderer.state.gui.GuiRenderState;
import net.minecraft.client.DeltaTracker;
import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector4f;

import java.util.List;
import java.util.Optional;
import java.util.OptionalDouble;

/**
 * Captures the vanilla bottom HUD into an isolated GUI render state, then draws
 * that texture as a real camera-relative 3D quad. This is the same rendering
 * architecture used by Spatial GUI: GUI extraction is kept separate from its
 * destination, so the game still owns every icon, bar, item decoration, font,
 * resource-pack texture, and compatibility detail.
 */
public final class SpatialHudWorldRenderer {
	private static final RenderPipeline HUD_PIPELINE = RenderPipelines.GUI_TEXTURED;
	private static final StagedVertexBuffer HUD_BUFFER = new StagedVertexBuffer(
			() -> "Spatial HUD world panel", RenderType.SMALL_BUFFER_SIZE);
	private static final Matrix4f IDENTITY_MATRIX = new Matrix4f();
	private static final Vector3f ZERO_VECTOR = new Vector3f();
	private static final Vector4f COLOR_MODULATOR = new Vector4f(1f, 1f, 1f, 1f);

	private static final SpatialHudWorldRenderer INSTANCE = new SpatialHudWorldRenderer();

	private GuiRenderState isolatedState;
	private GuiRenderer isolatedGuiRenderer;
	private GuiGraphicsExtractor isolatedGraphics;
	private TextureTarget hudTarget;
	private boolean frameOpen;
	private GpuBufferSlice capturedProjection;
	private ProjectionType capturedProjectionType;
	private final PoseStack capturedPose = new PoseStack();

	private SpatialHudWorldRenderer() {
	}

	public static SpatialHudWorldRenderer get() {
		return INSTANCE;
	}

	public boolean isIsolatedRenderer(Object renderer) {
		return renderer != null && renderer == isolatedGuiRenderer;
	}

	/** Used by the GuiRenderer mixin only for our isolated render state. */
	public com.mojang.blaze3d.pipeline.RenderTarget getTargetOr(
			com.mojang.blaze3d.pipeline.RenderTarget fallback) {
		return hudTarget != null ? hudTarget : fallback;
	}

	/** Starts exactly one isolated HUD extraction per game frame. */
	public void beginFrame() {
		if (!SpatialHud.isEnabled() || Minecraft.getInstance().player == null || frameOpen) {
			return;
		}

		try {
			ensureGuiRenderer();
			ensureTarget();
			clearTarget();
			SpatialHud.updateRenderSway();
			Minecraft mc = Minecraft.getInstance();
			isolatedGraphics = new GuiGraphicsExtractor(mc, isolatedState, 0, 0);
			frameOpen = true;
		} catch (Throwable t) {
			SpatialHud.safeDisable(t);
		}
	}

	GuiGraphicsExtractor graphics() {
		beginFrame(); // fallback for unusual HUD ordering (for example spectator UI)
		return isolatedGraphics;
	}

	void extract(HudElement vanilla, DeltaTracker deltaTracker) {
		GuiGraphicsExtractor graphics = graphics();
		if (graphics != null) {
			vanilla.extractRenderState(graphics, deltaTracker);
		}
	}

	/** Captures the perspective projection at the end of level rendering. */
	void capturePerspective() {
		if (!SpatialHud.isEnabled()) {
			return;
		}

		Minecraft mc = Minecraft.getInstance();
		if (mc.player == null || mc.level == null) {
			return;
		}

		var camera = mc.gameRenderer.mainCamera();
		capturedPose.setIdentity();
		capturedPose.mulPose(new Quaternionf()
				.rotateX((float) Math.toRadians(camera.xRot()))
				.rotateY((float) Math.toRadians(camera.yRot() + 180.0f))
				.get(new Matrix4f()));
		capturedProjection = RenderSystem.getProjectionMatrixBuffer();
		capturedProjectionType = RenderSystem.getProjectionType();
	}

	/** Called immediately before Minecraft draws its main GUI. */
	public void renderBeforeMainGui() {
		if (!frameOpen) {
			return;
		}

		try {
			// GuiRendererMixin redirects this specific renderer to hudTarget.
			isolatedGuiRenderer.render();
			isolatedGuiRenderer.endFrame();
			isolatedGraphics = null;
			frameOpen = false;
			renderWorldPanel();
		} catch (Throwable t) {
			frameOpen = false;
			isolatedGraphics = null;
			SpatialHud.safeDisable(t);
		}
	}

	private void ensureGuiRenderer() {
		if (isolatedGuiRenderer != null) {
			return;
		}
		Minecraft mc = Minecraft.getInstance();
		isolatedState = new GuiRenderState();
		// HUD elements do not use picture-in-picture renderers. Supplying an
		// empty list avoids borrowing mutable state from Minecraft's main GUI.
		isolatedGuiRenderer = new GuiRenderer(
				isolatedState,
				mc.gameRenderer.featureRenderDispatcher(),
				List.of());
	}

	private void ensureTarget() {
		Minecraft mc = Minecraft.getInstance();
		int width = Math.max(1, mc.getWindow().getWidth());
		int height = Math.max(1, mc.getWindow().getHeight());
		if (hudTarget == null) {
			hudTarget = new TextureTarget("Spatial HUD", width, height,
					GpuFormat.RGBA8_UNORM, GpuFormat.D16_UNORM);
		} else if (hudTarget.width != width || hudTarget.height != height) {
			hudTarget.resize(width, height);
		}
	}

	private void clearTarget() {
		if (hudTarget == null || hudTarget.getColorTexture() == null) {
			return;
		}
		var encoder = RenderSystem.getDevice().createCommandEncoder();
		encoder.clearColorTexture(hudTarget.getColorTexture(), new Vector4f(0f, 0f, 0f, 0f));
		if (hudTarget.getDepthTexture() != null) {
			encoder.clearDepthTexture(hudTarget.getDepthTexture(), 1.0);
		}
	}

	private void renderWorldPanel() {
		Minecraft mc = Minecraft.getInstance();
		if (hudTarget == null || capturedProjection == null || capturedProjectionType == null || mc.player == null) {
			return;
		}

		GpuTextureView texture = hudTarget.getColorTextureView();
		if (texture == null) {
			return;
		}

		var previousProjection = RenderSystem.getProjectionMatrixBuffer();
		var previousType = RenderSystem.getProjectionType();
		try {
			RenderSystem.setProjectionMatrix(capturedProjection, capturedProjectionType);
			drawPanel(texture);
		} finally {
			RenderSystem.setProjectionMatrix(previousProjection, previousType);
		}
	}

	private void drawPanel(GpuTextureView texture) {
		VertexFormat format = HUD_PIPELINE.getVertexFormatBinding(0);
		if (format == null) {
			return;
		}
		PrimitiveTopology primitive = HUD_PIPELINE.getPrimitiveTopology();
		StagedVertexBuffer.Draw draw = HUD_BUFFER.appendDraw(
				format,
				primitive,
				primitive == PrimitiveTopology.QUADS ? RenderSystem.getProjectionType().vertexSorting() : null);

		PoseStack matrices = new PoseStack();
		matrices.last().pose().set(capturedPose.last().pose());
		matrices.last().normal().set(capturedPose.last().normal());
		applyFirstPersonTransform(matrices);

		VertexConsumer vertices = HUD_BUFFER.getVertexBuilder(draw);
		float aspect = (float) hudTarget.width / (float) hudTarget.height;
		addScreenQuad(vertices, matrices.last().pose(), aspect);
		HUD_BUFFER.upload();

		StagedVertexBuffer.ExecuteInfo info = HUD_BUFFER.getExecuteInfo(draw);
		if (info == null) {
			HUD_BUFFER.endFrame();
			return;
		}

		try {
			drawTexture(info, texture);
		} finally {
			HUD_BUFFER.endFrame();
		}
	}

	/**
	 * Place a real plane in front of the camera, orient it toward the smoothed
	 * look vector, and then let the captured world projection provide the
	 * perspective. This is the core difference from v0.4's flat screen pose.
	 */
	private void applyFirstPersonTransform(PoseStack matrices) {
		SpatialHudConfig cfg = SpatialHudConfig.get();
		float yaw = SpatialHud.smoothYaw;
		float pitch = SpatialHud.smoothPitch;
		float yawRadians = (float) Math.toRadians(yaw);
		float pitchRadians = (float) Math.toRadians(pitch);

		float lookX = (float) (-Math.sin(yawRadians) * Math.cos(pitchRadians));
		float lookY = (float) -Math.sin(pitchRadians);
		float lookZ = (float) (Math.cos(yawRadians) * Math.cos(pitchRadians));
		float rightX = (float) Math.cos(yawRadians);
		float rightZ = (float) Math.sin(yawRadians);

		float distance = clamp((float) cfg.worldDistance, 0.35f, 6.0f);
		float side = clamp((float) cfg.worldSideOffset, -3.0f, 3.0f);
		float height = clamp((float) cfg.worldHeight, -3.0f, 3.0f);
		float fovScale = cfg.autoScaleByFov ? calculateFovScale() : 1.0f;
		float scale = clamp((float) cfg.worldScale * fovScale, 0.25f, 8.0f);

		matrices.translate(lookX * distance + rightX * side,
				lookY * distance + height,
				lookZ * distance + rightZ * side);
		matrices.mulPose(new Quaternionf()
				.rotateY(-yawRadians)
				.rotateX(-pitchRadians)
				.get(new Matrix4f()));
		matrices.scale(scale, scale, scale);
	}

	private float calculateFovScale() {
		double fov = Minecraft.getInstance().options.fov().get();
		double baseline = Math.max(30.0, Math.min(110.0, SpatialHudConfig.get().fovBaseline));
		return (float) Math.pow(Math.max(30.0, Math.min(150.0, fov)) / baseline, 1.2);
	}

	private void addScreenQuad(VertexConsumer buffer, Matrix4f pose, float aspect) {
		float halfWidth = aspect * 0.5f;
		float halfHeight = 0.5f;
		buffer.addVertex(pose, -halfWidth, -halfHeight, 0.0f).setUv(0.0f, 0.0f).setColor(255, 255, 255, 255);
		buffer.addVertex(pose, halfWidth, -halfHeight, 0.0f).setUv(1.0f, 0.0f).setColor(255, 255, 255, 255);
		buffer.addVertex(pose, halfWidth, halfHeight, 0.0f).setUv(1.0f, 1.0f).setColor(255, 255, 255, 255);
		buffer.addVertex(pose, -halfWidth, halfHeight, 0.0f).setUv(0.0f, 1.0f).setColor(255, 255, 255, 255);
	}

	private void drawTexture(StagedVertexBuffer.ExecuteInfo info, GpuTextureView texture) {
		Minecraft mc = Minecraft.getInstance();
		var mainTarget = mc.gameRenderer.mainRenderTarget();
		var output = mainTarget.getColorTextureView();
		if (output == null) {
			return;
		}

		GpuBufferSlice transforms = RenderSystem.getDynamicUniforms().writeTransform(
				IDENTITY_MATRIX, COLOR_MODULATOR, ZERO_VECTOR, IDENTITY_MATRIX);
		try (var renderPass = RenderSystem.getDevice().createCommandEncoder().createRenderPass(
				() -> "Spatial HUD world panel",
				output,
				Optional.empty(),
				mainTarget.getDepthTextureView(),
				OptionalDouble.empty())) {
			renderPass.setPipeline(RenderSystem.getCompiledPipeline(HUD_PIPELINE));
			RenderSystem.bindDefaultUniforms(renderPass);
			renderPass.setUniform("DynamicTransforms", transforms);
			renderPass.setUniform("Sampler0", texture, RenderSystem.getSamplerCache().getSampler(
					AddressMode.CLAMP_TO_EDGE, AddressMode.CLAMP_TO_EDGE,
					FilterMode.LINEAR, FilterMode.LINEAR, true));
			renderPass.setVertexBuffer(0, info.vertexBuffer().slice());
			renderPass.setIndexBuffer(info.indexBuffer(), info.indexType());
			renderPass.drawIndexed(info.indexCount(), 1, info.firstIndex(), info.baseVertex(), 0);
		}
	}

	private static float clamp(float value, float min, float max) {
		return Math.max(min, Math.min(max, value));
	}
}
