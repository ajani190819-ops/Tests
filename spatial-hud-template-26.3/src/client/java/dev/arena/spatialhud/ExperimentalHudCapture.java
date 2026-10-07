package dev.arena.spatialhud;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.pipeline.TextureTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.renderpearl.api.GpuFormat;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import com.mojang.renderpearl.api.pipeline.PrimitiveTopology;
import com.mojang.renderpearl.api.pipeline.RenderPipeline;
import com.mojang.renderpearl.api.textures.AddressMode;
import com.mojang.renderpearl.api.textures.FilterMode;
import com.mojang.renderpearl.api.textures.GpuTextureView;
import com.mojang.renderpearl.api.vertex.VertexFormat;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.render.GuiRenderer;
import net.minecraft.client.renderer.RenderPipelines;
import net.minecraft.client.renderer.StagedVertexBuffer;
import net.minecraft.client.renderer.rendertype.RenderType;
import net.minecraft.client.renderer.state.gui.GuiRenderState;
import org.joml.Matrix4f;
import org.joml.Vector3f;
import org.joml.Vector4f;

import java.util.List;
import java.util.Optional;
import java.util.OptionalDouble;

/**
 * Experimental, identity-scoped capture path for the vanilla bottom HUD.
 *
 * <p>This deliberately does <strong>not</strong> replace Minecraft's GUI
 * renderer or redirect its normal renderer. The Fabric HUD wrappers feed only
 * the selected bottom-strip roots into this private {@link GuiRenderState}; a
 * private {@link GuiRenderer} then renders that state into an equally private
 * texture target. The two mixins merely identify that private renderer by
 * object identity. Normal screens, chat, minimaps, debug text, and every
 * other GUI renderer therefore retain their original target and draw call.</p>
 *
 * <p>The feature is disabled by default. Any extraction, target, or GPU error
 * latches this class into its safe affine fallback for the rest of the client
 * session and turns the config switch off. The next HUD frame then follows the
 * released public-HUD-API renderer exactly.</p>
 */
public final class ExperimentalHudCapture {
	private static final String TARGET_NAME = "Spatial HUD experimental bottom-strip capture";
	private static final int MESH_COLUMNS = 12;
	private static final int MESH_ROWS = 8;
	private static final RenderPipeline WARP_PIPELINE = RenderPipelines.GUI_TEXTURED;
	private static final StagedVertexBuffer WARP_BUFFER = new StagedVertexBuffer(
			() -> "Spatial HUD experimental warp mesh", RenderType.SMALL_BUFFER_SIZE);
	private static final Matrix4f IDENTITY = new Matrix4f();
	private static final Vector3f ZERO = new Vector3f();
	private static final Vector4f WHITE = new Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

	private static GuiRenderState capturedState;
	private static GuiRenderer capturedRenderer;
	private static GuiGraphicsExtractor capturedGraphics;
	private static TextureTarget capturedTarget;
	private static boolean frameActive;
	private static boolean frameHasContent;
	private static boolean sessionFallback;
	private static int guiWidth;
	private static int guiHeight;

	private ExperimentalHudCapture() {
	}

	/**
	 * Called by the panel element before the selected vanilla HUD roots extract.
	 * No target is allocated and no GUI is intercepted here.
	 */
	static void beginFrame(GuiGraphicsExtractor sourceGraphics) {
		frameActive = false;
		frameHasContent = false;

		if (!SpatialHud.isExperimentalCaptureActive() || sessionFallback) {
			return;
		}

		try {
			ensureRenderer();
			guiWidth = sourceGraphics.guiWidth();
			guiHeight = sourceGraphics.guiHeight();
			if (guiWidth <= 0 || guiHeight <= 0) {
				throw new IllegalStateException("invalid bottom-HUD capture dimensions " + guiWidth + "x" + guiHeight);
			}
			// HUD roots do not use pointer hit testing. Giving the isolated
			// extractor a harmless coordinate also prevents a screen's current
			// mouse state from leaking into this selected-only render state.
			capturedGraphics = new GuiGraphicsExtractor(Minecraft.getInstance(), capturedState, -1, -1);
			frameActive = true;
		} catch (Throwable t) {
			fallback(t, "preparing the selected bottom-HUD capture");
		}
	}

	/** True only while the current gameplay HUD frame is collecting a texture. */
	static boolean isFrameActive() {
		return frameActive && !sessionFallback;
	}

	/**
	 * Extract one already-selected vanilla root into the private state. Returns
	 * false after an error so the caller can use the released affine path for
	 * its current root rather than losing a future HUD frame.
	 */
	static boolean capture(HudRootRenderer root, DeltaTracker deltaTracker) {
		if (!isFrameActive()) {
			return false;
		}

		try {
			root.extract(capturedGraphics, deltaTracker);
			frameHasContent = true;
			return true;
		} catch (Throwable t) {
			fallback(t, "extracting a selected bottom-HUD root");
			return false;
		}
	}

	/** The GuiRenderer mixin uses this strict identity check for target routing. */
	public static boolean isCapturedRenderer(Object renderer) {
		return frameActive && !sessionFallback && renderer == capturedRenderer;
	}

	/**
	 * Called only after {@link #isCapturedRenderer(Object)} succeeds inside the
	 * identity-scoped GuiRenderer mixin. A null target is deliberately treated
	 * as a no-op route to keep an unexpected render-order change fail-safe.
	 */
	public static RenderTarget captureTargetOr(RenderTarget original) {
		return capturedTarget != null ? capturedTarget : original;
	}

	/**
	 * Invoked immediately before Minecraft draws its normal GUI renderer. By
	 * then the Fabric HUD wrappers have extracted only the selected roots. Draw
	 * those roots to our texture, then composite a tessellated projective mesh
	 * onto the main GUI target. It is not a world renderer and it never changes
	 * any projection state.
	 */
	public static void renderAndComposite() {
		if (!frameActive || !frameHasContent || sessionFallback) {
			return;
		}

		try {
			ensureTarget();
			clearTarget();
			capturedRenderer.render();
			capturedRenderer.endFrame();
			compositeProjectiveMesh();
		} catch (Throwable t) {
			fallback(t, "rendering the selected bottom-HUD texture");
		} finally {
			// The isolated GuiRenderer has consumed this exact state. Always clear
			// the identity gate so no later GUI draw can ever be redirected.
			frameActive = false;
			frameHasContent = false;
			capturedGraphics = null;
		}
	}

	private static void ensureRenderer() {
		if (capturedRenderer != null) {
			return;
		}
		Minecraft mc = Minecraft.getInstance();
		capturedState = new GuiRenderState();
		// Picture-in-picture renderers are intentionally omitted. The captured
		// state receives only the explicitly wrapped vanilla bottom HUD roots,
		// never a screen or a globally registered GUI layer.
		capturedRenderer = new GuiRenderer(capturedState, mc.gameRenderer.featureRenderDispatcher(), List.of());
	}

	private static void ensureTarget() {
		Minecraft mc = Minecraft.getInstance();
		int width = mc.getWindow().getWidth();
		int height = mc.getWindow().getHeight();
		if (width <= 0 || height <= 0) {
			throw new IllegalStateException("invalid capture target dimensions " + width + "x" + height);
		}

		if (capturedTarget == null) {
			capturedTarget = new TextureTarget(TARGET_NAME, width, height, GpuFormat.RGBA8_UNORM, GpuFormat.D16_UNORM);
		} else if (capturedTarget.width != width || capturedTarget.height != height) {
			capturedTarget.resize(width, height);
		}
	}

	private static void clearTarget() {
		if (capturedTarget == null || capturedTarget.getColorTexture() == null) {
			throw new IllegalStateException("bottom-HUD capture target has no color texture");
		}
		var encoder = RenderSystem.getDevice().createCommandEncoder();
		encoder.clearColorTexture(capturedTarget.getColorTexture(), new Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
		if (capturedTarget.getDepthTexture() != null) {
			encoder.clearDepthTexture(capturedTarget.getDepthTexture(), 1.0);
		}
	}

	/**
	 * Builds a grid rather than one affine rectangle. Each cell is textured from
	 * the finished HUD texture, so all pixels inside a hotbar slot, icon, bar,
	 * or tooltip are genuinely warped by the trapezoidal/projective surface.
	 * Extra columns make optional cylindrical curvature smooth without affecting
	 * the safe default mode (whose curvature value is zero).
	 */
	private static void compositeProjectiveMesh() {
		if (capturedTarget == null || capturedTarget.getColorTextureView() == null) {
			throw new IllegalStateException("bottom-HUD capture texture view is unavailable");
		}
		GpuTextureView texture = capturedTarget.getColorTextureView();
		VertexFormat format = WARP_PIPELINE.getVertexFormatBinding(0);
		PrimitiveTopology primitive = WARP_PIPELINE.getPrimitiveTopology();
		if (format == null || primitive != PrimitiveTopology.QUADS) {
			throw new IllegalStateException("GUI textured pipeline does not expose QUADS");
		}

		StagedVertexBuffer.Draw draw = WARP_BUFFER.appendDraw(format, primitive, null);
		try {
			VertexConsumer vertices = WARP_BUFFER.getVertexBuilder(draw);
			addWarpMesh(vertices, SpatialHudConfig.get());
			WARP_BUFFER.upload();
			StagedVertexBuffer.ExecuteInfo info = WARP_BUFFER.getExecuteInfo(draw);
			if (info == null) {
				throw new IllegalStateException("GPU rejected the bottom-HUD warp mesh");
			}
			drawToMainTarget(info, texture);
		} finally {
			WARP_BUFFER.endFrame();
		}
	}

	private static void addWarpMesh(VertexConsumer vertices, SpatialHudConfig cfg) {
		// This source area tightly covers the selected vanilla roots: held-name
		// tooltip at the far edge through the hotbar at the near edge. The
		// texture target itself is window-sized only to retain the game's native
		// GUI scaling and avoid an unsafe global Window/GUI-scale redirect.
		float sourceCenterX = guiWidth * 0.5f;
		float sourceCenterY = guiHeight - 33.0f;
		float sourceLeft = sourceCenterX - 112.0f;
		float sourceRight = sourceCenterX + 112.0f;
		float sourceTop = Math.max(0.0f, guiHeight - 128.0f);
		float sourceBottom = guiHeight + 4.0f;

		for (int row = 0; row < MESH_ROWS; row++) {
			float v0 = row / (float) MESH_ROWS;
			float v1 = (row + 1) / (float) MESH_ROWS;
			for (int column = 0; column < MESH_COLUMNS; column++) {
				float u0 = column / (float) MESH_COLUMNS;
				float u1 = (column + 1) / (float) MESH_COLUMNS;
				addWarpVertex(vertices, cfg, sourceCenterX, sourceCenterY, sourceLeft, sourceRight, sourceTop, sourceBottom, u0, v0);
				addWarpVertex(vertices, cfg, sourceCenterX, sourceCenterY, sourceLeft, sourceRight, sourceTop, sourceBottom, u1, v0);
				addWarpVertex(vertices, cfg, sourceCenterX, sourceCenterY, sourceLeft, sourceRight, sourceTop, sourceBottom, u1, v1);
				addWarpVertex(vertices, cfg, sourceCenterX, sourceCenterY, sourceLeft, sourceRight, sourceTop, sourceBottom, u0, v1);
			}
		}
	}

	private static void addWarpVertex(
			VertexConsumer vertices,
			SpatialHudConfig cfg,
			float sourceCenterX,
			float sourceCenterY,
			float sourceLeft,
			float sourceRight,
			float sourceTop,
			float sourceBottom,
			float u,
			float v) {
		float sx = lerp(sourceLeft, sourceRight, u);
		float sy = lerp(sourceTop, sourceBottom, v);
		float[] destination = transformPoint(cfg, sourceCenterX, sourceCenterY, sx, sy, u, v);

		// The target is native-window sized while the GuiRenderer retains native
		// GUI scale. GUI-space source coordinates therefore normalize against
		// the logical GUI bounds for correct texels at every UI scale.
		float textureU = sx / guiWidth;
		float textureV = sy / guiHeight;
		vertices.addVertex(IDENTITY, destination[0], destination[1], 0.0f)
				.setUv(textureU, textureV)
				.setColor(255, 255, 255, 255);
	}

	/** The projective plane equation used by every mesh vertex. */
	private static float[] transformPoint(
			SpatialHudConfig cfg, float sourceCenterX, float sourceCenterY, float sx, float sy, float u, float v) {
		Minecraft mc = Minecraft.getInstance();
		double fov = clamp(mc.options.fov().get(), 30.0, 150.0);
		double focal = (guiHeight * 0.5) / Math.tan(Math.toRadians(fov) * 0.5);
		double distance = Math.max(0.10, cfg.distance);
		double width = Math.max(0.10, cfg.planeWidth);
		double fovCompensation = 1.0;
		if (cfg.autoScaleByFov) {
			double baseline = clamp(cfg.fovBaseline, 30.0, 110.0);
			fovCompensation = Math.pow(fov / baseline, 1.2);
		}
		float scale = clamp((float) ((width * focal / distance) / 182.0 * fovCompensation), 0.15f, 4.0f);

		float revealY = guiHeight * 0.5f + (float) (cfg.height * focal / distance);
		float targetY = revealY;
		if (cfg.revealWhenLookingDown) {
			float start = clamp(cfg.revealStartPitch, 0.0f, 89.0f);
			float full = Math.max(start + 1.0f, clamp(cfg.revealFullPitch, 1.0f, 90.0f));
			float reveal = smoothstep(start, full, SpatialHud.pitch);
			targetY = lerp(guiHeight + Math.max(20, cfg.hiddenBelowScreenPixels), revealY, reveal);
		}

		float tilt = SpatialHudElement.planeTiltAmount(cfg);
		float vertical = cfg.lookDownPlaneTilt
				? lerp(clamp(cfg.planeHorizonHeightPercent / 100.0f, 0.05f, 1.0f), 1.0f, tilt)
				: 1.0f;
		float farWidth = cfg.lookDownPlaneTilt
				? lerp(clamp(cfg.planeHorizonFarEdgeWidthPercent / 100.0f, 0.20f, 1.0f), 1.0f, tilt)
				: 1.0f;

		float strength = clamp((float) cfg.sway, 0.0f, 2.0f);
		float yawError = SpatialHud.wrapDegrees(SpatialHud.yaw - SpatialHud.smoothYaw);
		float pitchError = SpatialHud.pitch - SpatialHud.smoothPitch;
		float swayX = clamp((float) (-Math.toRadians(yawError) * focal * 0.30 * strength), -60.0f, 60.0f);
		float swayY = clamp((float) (Math.toRadians(pitchError) * focal * 0.30 * strength), -60.0f, 60.0f);
		float rotation = cfg.rotateWithSway ? clamp(yawError * 0.20f * strength, -5.0f, 5.0f) : 0.0f;

		// Top = far side. Interpolating reciprocal depth is the projective
		// perspective law, rather than simply scaling every HUD root as in the
		// released performance path. The mesh carries that non-affine mapping
		// into each finished icon, bar, glyph, and tooltip pixel.
		float depth = lerp(farWidth, 1.0f, v);
		float localX = (sx - sourceCenterX) * scale * depth;
		float localY = (sy - sourceCenterY) * scale * vertical;

		// Optional, explicitly experimental screen-space cylinder bow. Zero is
		// the default; it is intentionally never implied by safe mode.
		if (cfg.experimentalCaptureCurvaturePercent > 0) {
			float normalizedX = u * 2.0f - 1.0f;
			float curve = cfg.experimentalCaptureCurvaturePercent / 100.0f;
			localY -= normalizedX * normalizedX * curve * 24.0f * scale * depth;
		}

		if (rotation != 0.0f) {
			float radians = (float) Math.toRadians(rotation);
			float cos = (float) Math.cos(radians);
			float sin = (float) Math.sin(radians);
			float rx = localX * cos - localY * sin;
			localY = localX * sin + localY * cos;
			localX = rx;
		}
		return new float[] {sourceCenterX + swayX + localX, targetY + swayY + localY};
	}

	private static void drawToMainTarget(StagedVertexBuffer.ExecuteInfo info, GpuTextureView texture) {
		RenderTarget mainTarget = Minecraft.getInstance().gameRenderer.mainRenderTarget();
		GpuTextureView output = mainTarget.getColorTextureView();
		if (output == null) {
			throw new IllegalStateException("main GUI target has no color texture view");
		}
		GpuBufferSlice transforms = RenderSystem.getDynamicUniforms().writeTransform(IDENTITY, WHITE, ZERO, IDENTITY);
		try (var pass = RenderSystem.getDevice().createCommandEncoder().createRenderPass(
				() -> "Spatial HUD experimental projective composite",
				output,
				Optional.empty(),
				null,
				OptionalDouble.empty())) {
			pass.setPipeline(RenderSystem.getCompiledPipeline(WARP_PIPELINE));
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

	private static void fallback(Throwable error, String stage) {
		if (sessionFallback) {
			return;
		}
		sessionFallback = true;
		frameActive = false;
		frameHasContent = false;
		capturedGraphics = null;
		SpatialHudConfig cfg = SpatialHudConfig.get();
		cfg.experimentalCaptureWarp = false;
		SpatialHudConfig.save();
		SpatialHud.LOGGER.error("Spatial HUD experimental capture failed while {}; switched to the safe affine renderer for this session.", stage, error);
	}

	private static float smoothstep(float edge0, float edge1, float value) {
		float t = clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	private static float lerp(float from, float to, float amount) {
		return from + (to - from) * amount;
	}

	private static float clamp(float value, float min, float max) {
		return value < min ? min : (value > max ? max : value);
	}

	private static double clamp(double value, double min, double max) {
		return value < min ? min : (value > max ? max : value);
	}

	@FunctionalInterface
	interface HudRootRenderer {
		void extract(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker);
	}
}
