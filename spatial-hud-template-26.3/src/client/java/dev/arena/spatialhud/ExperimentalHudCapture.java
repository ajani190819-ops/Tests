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
import net.minecraft.resources.Identifier;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
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
 * <p>Texture capture is the required baseline for all three presentation
 * methods. A GPU error still latches a safe affine display for the current
 * session, but it never rewrites the player's selected method: a restart or
 * selecting another method retries the requested captured path.</p>
 */
public final class ExperimentalHudCapture {
	private static final String TARGET_NAME = "Spatial HUD experimental bottom-strip capture";
	// Dense enough that curvature and per-icon projective deformation do not
	// reveal the old root-by-root affine seams.
	private static final int MESH_COLUMNS = 32;
	private static final int MESH_ROWS = 24;
	private static final RenderPipeline WARP_PIPELINE = RenderPipelines.GUI_TEXTURED;
	private static final StagedVertexBuffer WARP_BUFFER = new StagedVertexBuffer(
			() -> "Spatial HUD experimental warp mesh", RenderType.SMALL_BUFFER_SIZE);
	// Method 4's purple identity colours. The interior is deliberately
	// translucent so the warped HUD stays readable on top of it.
	private static final int POLYGON_BACKING_COLOR = 0x7031004D;
	private static final int POLYGON_EDGE_COLOR = 0xFFC75CFF;
	private static final int POLYGON_HANDLE_COLOR = 0xFFFFD6FF;
	private static final int POLYGON_HANDLE_INNER_COLOR = 0xFF6C1D8B;
	// Height, in GUI pixels above the bottom of the screen, of the strip band
	// that Method 4 warps onto its quad: hotbar, status bars, experience level,
	// and held-item text.
	private static final int POLYGON_SOURCE_HEIGHT = 72;
	private static final Matrix4f IDENTITY = new Matrix4f();
	private static final Vector3f ZERO = new Vector3f();
	private static final Vector4f WHITE = new Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

	private static GuiRenderState capturedState;
	private static GuiRenderer capturedRenderer;
	private static GuiGraphicsExtractor capturedGraphics;
	private static TextureTarget capturedTarget;
	private static boolean frameActive;
	private static boolean frameHasContent;
	private static boolean frameCapturedHotbar;
	private static boolean loggedHotbarExtraction;
	private static boolean loggedMissingHotbar;
	private static boolean loggedPolygonComposite;
	/** Becomes true after a completed capture can be drawn by Method 3 next frame. */
	private static boolean worldTextureReady;
	private static boolean sessionFallback;
	/** Lets a deliberate mode change retry capture after a one-session failure. */
	private static SpatialHudConfig.RenderMethod failedMethod;
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
		frameCapturedHotbar = false;

		SpatialHudConfig.RenderMethod requestedMethod = SpatialHudConfig.get().selectedRenderMethod();
		// Do not make a transient driver/companion-mod failure permanently turn
		// the requested warp off. Changing method is an explicit request to retry
		// the private capture path during this client session.
		if (sessionFallback && requestedMethod != failedMethod) {
			sessionFallback = false;
			failedMethod = null;
			SpatialHud.LOGGER.info("Spatial HUD retrying the forced texture warp after render-method change to {}.", requestedMethod);
		}

		if (!SpatialHud.isTextureCaptureActive() || sessionFallback
				|| (!SpatialHudConfig.get().usesPolygonTest()
						&& !SpatialHud.isPhysicalPanelVisibleInGui(sourceGraphics.guiWidth(), sourceGraphics.guiHeight()))) {
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
			if (SpatialHudConfig.get().usesPolygonTest()) {
				// Method 4's purple interior is painted before any vanilla root
				// extracts, so the captured hotbar, bars, icons and text stay
				// readable on top of it instead of being tinted by it.
				capturePolygonBackdrop();
			}
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
	static boolean capture(Identifier id, HudRootRenderer root, DeltaTracker deltaTracker) {
		if (!isFrameActive()) {
			return false;
		}

		try {
			root.extract(capturedGraphics, deltaTracker);
			if (id.equals(VanillaHudElements.HOTBAR)) {
				frameCapturedHotbar = true;
				if (!loggedHotbarExtraction) {
					loggedHotbarExtraction = true;
					SpatialHud.LOGGER.info("Spatial HUD experimental capture extracted the hotbar root into its private texture.");
				}
			}
			frameHasContent = true;
			return true;
		} catch (Throwable t) {
			fallback(t, "extracting a selected bottom-HUD root");
			return false;
		}
	}

	/**
	 * Draw the optional backing and the active-method indicator into the same
	 * isolated source texture as the vanilla roots. The green/blue/red marker is
	 * part of the captured surface in both texture methods, never an unrelated
	 * GUI overlay. It remains visible as a full-width identity band even when
	 * the player hides the backing panel.
	 */
	static boolean capturePanelDecorations(SpatialHudConfig cfg) {
		if (!isFrameActive()) {
			return false;
		}
		try {
			VirtualHudPlane plane = VirtualHudPlane.forGui(cfg, guiWidth, guiHeight);
			// Draw only inside the source rectangle sampled by Methods 2 and 3.
			// Painting a margin outside it would make an indicator disappear during
			// the texture presentation even though it appeared in Method 1.
			int left = (int) Math.floor(plane.sourceLeft());
			int right = (int) Math.ceil(plane.sourceRight());
			int top = (int) Math.floor(plane.sourceTop());
			int bottom = (int) Math.ceil(plane.sourceBottom());
			if (cfg.usesPolygonTest()) {
				// Method 4's purple border and four corner handles are captured
				// together with the hotbar/status roots. The polygon composite
				// maps these same pixels onto the four GUI-space control points,
				// so the visible outline lands exactly on the configured corners
				// and follows the live pitch response with the rest of the quad.
				SourceRect polygon = polygonSourceRect(cfg);
				int edgeLeft = polygon.left();
				int edgeTop = polygon.top();
				int edgeRight = polygon.right();
				int edgeBottom = polygon.bottom();
				capturedGraphics.fill(edgeLeft, edgeTop, edgeRight, edgeTop + 3, POLYGON_EDGE_COLOR);
				capturedGraphics.fill(edgeLeft, edgeBottom - 3, edgeRight, edgeBottom, POLYGON_EDGE_COLOR);
				capturedGraphics.fill(edgeLeft, edgeTop, edgeLeft + 3, edgeBottom, POLYGON_EDGE_COLOR);
				capturedGraphics.fill(edgeRight - 3, edgeTop, edgeRight, edgeBottom, POLYGON_EDGE_COLOR);
				drawPolygonHandle(edgeLeft, edgeTop, 1, 1);
				drawPolygonHandle(edgeRight, edgeTop, -1, 1);
				drawPolygonHandle(edgeRight, edgeBottom, -1, -1);
				drawPolygonHandle(edgeLeft, edgeBottom, 1, -1);
			} else {
				if (cfg.showPanel) {
					capturedGraphics.fill(left, top, right, bottom, 0x80101018);
					capturedGraphics.fill(left, top, right, top + 8, 0x5038384A);
				}
				// A full-width, 8px top band is a deliberately obvious live method
				// indicator: green = affine, blue = projective mesh, red = world quad.
				// It lives in this texture, so it receives exactly the same perspective
				// and world-depth treatment as the selected HUD pixels.
				capturedGraphics.fill(left, top, right, top + 8, cfg.modeIndicatorColor());
			}
			frameHasContent = true;
			return true;
		} catch (Throwable t) {
			fallback(t, "adding the selected bottom-HUD panel decorations to the capture");
			return false;
		}
	}

	/** Draws an inward-facing 10px source marker that lands on a quad corner. */
	private static void drawPolygonHandle(int cornerX, int cornerY, int xDirection, int yDirection) {
		int x0 = xDirection > 0 ? cornerX : cornerX - 10;
		int y0 = yDirection > 0 ? cornerY : cornerY - 10;
		capturedGraphics.fill(x0, y0, x0 + 10, y0 + 10, POLYGON_HANDLE_COLOR);
		capturedGraphics.fill(x0 + 2, y0 + 2, x0 + 8, y0 + 8, POLYGON_HANDLE_INNER_COLOR);
	}

	/**
	 * Method 4's translucent purple interior. It is painted before the selected
	 * roots so the captured pixels stay readable on top of it. The border and
	 * corner handles are added afterwards by {@link #capturePanelDecorations},
	 * and the whole rectangle is mapped onto the configured corners, so the
	 * pitch response moves this surface with the rest of the quad.
	 */
	private static void capturePolygonBackdrop() {
		// Whatever the other settings are, the quad itself is the control
		// surface for Method 4, so the frame always has visible content.
		frameHasContent = true;
		SpatialHudConfig cfg = SpatialHudConfig.get();
		if (!cfg.showPanel) {
			// Show Backing Panel owns the interior tint here exactly as it owns
			// the backing of the other methods. The border and handles remain.
			return;
		}
		SourceRect rect = polygonSourceRect(cfg);
		capturedGraphics.fill(rect.left(), rect.top(), rect.right(), rect.bottom(), POLYGON_BACKING_COLOR);
	}

	/**
	 * The source band Method 4 maps onto the four corners: the full width of the
	 * captured strip, but only its bottom {@link #POLYGON_SOURCE_HEIGHT} GUI
	 * pixels. That is where the hotbar, status bars, experience and held-item
	 * text actually draw, so the quad fills with HUD instead of showing the
	 * hotbar in its bottom quarter under mostly empty purple.
	 *
	 * <p>The bottom edge is clamped to the real GUI because the plane
	 * deliberately samples a few pixels below the screen: that line is clipped
	 * away, and its pixels arrive transparent.</p>
	 */
	private static SourceRect polygonSourceRect(SpatialHudConfig cfg) {
		VirtualHudPlane plane = VirtualHudPlane.forGui(cfg, guiWidth, guiHeight);
		int left = (int) Math.floor(Math.max(0.0f, plane.sourceLeft()));
		int right = (int) Math.ceil(Math.min(guiWidth, plane.sourceRight()));
		int bottom = (int) Math.ceil(Math.min(guiHeight, plane.sourceBottom()));
		int top = Math.max(0, bottom - POLYGON_SOURCE_HEIGHT);
		return new SourceRect(left, top, right, bottom);
	}

	/** Inclusive-exclusive GUI pixel bounds of the painted capture surface. */
	private record SourceRect(int left, int top, int right, int bottom) {
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

	/** A completed previous-frame texture for Method 3's world render pass. */
	static GpuTextureView worldTextureView() {
		if (!worldTextureReady || sessionFallback || capturedTarget == null
				|| !SpatialHud.isWorldSpaceTextureActive()) {
			return null;
		}
		return capturedTarget.getColorTextureView();
	}

	/** World rendering must use the same loud, safe failure behavior as capture. */
	static void worldTextureFailed(Throwable error) {
		fallback(error, "drawing the world-space HUD texture");
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
		if (SpatialHudConfig.get().showHotbar && !frameCapturedHotbar && !loggedMissingHotbar) {
			loggedMissingHotbar = true;
			SpatialHud.LOGGER.warn("Spatial HUD experimental capture did not receive the vanilla hotbar root. The backing may render without hotbar pixels; check the Hotbar and Spectator Menu setting and mod HUD replacement order.");
		}

		try {
				ensureTarget();
				clearTarget();
				capturedRenderer.render();
				capturedRenderer.endFrame();

				SpatialHudConfig cfg = SpatialHudConfig.get();
				if (cfg.selectedRenderMethod() == SpatialHudConfig.RenderMethod.WORLD_SPACE_TEXTURE) {
					// Level rendering happens before GUI extraction. The world renderer
					// intentionally draws this finished texture on the next frame.
					worldTextureReady = true;
				} else if (cfg.usesPolygonTest()) {
					worldTextureReady = false;
					if (!loggedPolygonComposite) {
						loggedPolygonComposite = true;
						SpatialHud.LOGGER.info("Spatial HUD Method 4 is mapping the captured lower HUD to its four purple GUI corners.");
					}
					compositePolygonTestMesh(cfg);
				} else {
					// Both screen-space choices are mandatory texture meshes. Their
					// different warp strengths make green and blue visibly different
					// without ever falling back to a root-by-root flat card.
					worldTextureReady = false;
					compositeProjectiveMesh(cfg);
				}
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
	private static void compositeProjectiveMesh(SpatialHudConfig cfg) {
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
			addWarpMesh(vertices, cfg);
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

	/**
	 * Method 4 uses the exact same isolated lower-HUD texture as the regular
	 * mesh path, but maps its source rectangle to four user-controlled GUI
	 * points. A dense grid preserves continuous item/icon/text deformation while
	 * allowing a normal trapezoid or any deliberate corner stress case.
	 */
	private static void compositePolygonTestMesh(SpatialHudConfig cfg) {
		if (capturedTarget == null || capturedTarget.getColorTextureView() == null) {
			throw new IllegalStateException("polygon test capture texture view is unavailable");
		}
		GpuTextureView texture = capturedTarget.getColorTextureView();
		VertexFormat format = WARP_PIPELINE.getVertexFormatBinding(0);
		PrimitiveTopology primitive = WARP_PIPELINE.getPrimitiveTopology();
		if (format == null || primitive != PrimitiveTopology.QUADS) {
			throw new IllegalStateException("polygon test pipeline does not expose textured QUADS");
		}

		StagedVertexBuffer.Draw draw = WARP_BUFFER.appendDraw(format, primitive, null);
		try {
			VertexConsumer vertices = WARP_BUFFER.getVertexBuilder(draw);
			addPolygonTestMesh(vertices, cfg);
			WARP_BUFFER.upload();
			StagedVertexBuffer.ExecuteInfo info = WARP_BUFFER.getExecuteInfo(draw);
			if (info == null) {
				throw new IllegalStateException("GPU rejected the four-corner polygon mesh");
			}
			drawToMainTarget(info, texture);
		} finally {
			WARP_BUFFER.endFrame();
		}
	}

	/**
	 * Every cell uses the one {@link VirtualHudPlane} projection. This is the
	 * critical distinction from the former root-scale approximation: UVs stay
	 * tied to finished capture pixels while vertex depth changes across the
	 * entire strip, so a single heart or hotbar slot itself becomes trapezoidal.
	 *
	 * <p>The two mesh methods deliberately add a far-edge pinch to the real
	 * physical projection. This pulls the top two corners together and widens
	 * the bottom edge, making a proper flat-map trapezoid instead of a vague
	 * independent X/Y stretch.</p>
	 */
	private static void addWarpMesh(VertexConsumer vertices, SpatialHudConfig cfg) {
		VirtualHudPlane plane = VirtualHudPlane.forGui(cfg, guiWidth, guiHeight);
		float topEdgeWidth = cfg.meshTopEdgeWidthMultiplier();
		float bottomEdgeWidth = cfg.meshBottomEdgeWidthMultiplier();
		for (int row = 0; row < MESH_ROWS; row++) {
			float v0 = row / (float) MESH_ROWS;
			float v1 = (row + 1) / (float) MESH_ROWS;
			for (int column = 0; column < MESH_COLUMNS; column++) {
				float u0 = column / (float) MESH_COLUMNS;
				float u1 = (column + 1) / (float) MESH_COLUMNS;
				addWarpVertex(vertices, plane, u0, v0, topEdgeWidth, bottomEdgeWidth);
				addWarpVertex(vertices, plane, u1, v0, topEdgeWidth, bottomEdgeWidth);
				addWarpVertex(vertices, plane, u1, v1, topEdgeWidth, bottomEdgeWidth);
				addWarpVertex(vertices, plane, u0, v1, topEdgeWidth, bottomEdgeWidth);
			}
		}
	}

	private static void addWarpVertex(VertexConsumer vertices, VirtualHudPlane plane, float u, float v,
			float topEdgeWidth, float bottomEdgeWidth) {
		float sourceX = lerp(plane.sourceLeft(), plane.sourceRight(), u);
		float sourceY = lerp(plane.sourceTop(), plane.sourceBottom(), v);
		VirtualHudPlane.Point destination = plane.projectWarped(sourceX, sourceY,
				topEdgeWidth, bottomEdgeWidth);
		vertices.addVertex(IDENTITY, destination.x(), destination.y(), 0.0f)
				.setUv(plane.textureU(sourceX), plane.textureV(sourceY))
				.setColor(255, 255, 255, 255);
	}

	private static void addPolygonTestMesh(VertexConsumer vertices, SpatialHudConfig cfg) {
		VirtualHudPlane source = VirtualHudPlane.forGui(cfg, guiWidth, guiHeight);
		PolygonTestRenderer.Quad target = PolygonTestRenderer.quad(cfg, guiWidth, guiHeight);
		// Sample exactly the band that carries the painted purple border and the
		// HUD pixels, so the configured corners land on painted source and the
		// whole quad fills with the warped strip.
		SourceRect painted = polygonSourceRect(cfg);
		for (int row = 0; row < MESH_ROWS; row++) {
			float v0 = row / (float) MESH_ROWS;
			float v1 = (row + 1) / (float) MESH_ROWS;
			for (int column = 0; column < MESH_COLUMNS; column++) {
				float u0 = column / (float) MESH_COLUMNS;
				float u1 = (column + 1) / (float) MESH_COLUMNS;
				addPolygonTestVertex(vertices, source, painted, target, u0, v0);
				addPolygonTestVertex(vertices, source, painted, target, u1, v0);
				addPolygonTestVertex(vertices, source, painted, target, u1, v1);
				addPolygonTestVertex(vertices, source, painted, target, u0, v1);
			}
		}
	}

	private static void addPolygonTestVertex(VertexConsumer vertices, VirtualHudPlane source,
			SourceRect painted, PolygonTestRenderer.Quad target, float u, float v) {
		float sourceX = lerp(painted.left(), painted.right(), u);
		float sourceY = lerp(painted.top(), painted.bottom(), v);
		PolygonTestRenderer.Point destination = target.project(u, v);
		vertices.addVertex(IDENTITY, destination.x(), destination.y(), 0.0f)
				.setUv(source.textureU(sourceX), source.textureV(sourceY))
				.setColor(255, 255, 255, 255);
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
		worldTextureReady = false;
		capturedGraphics = null;
		failedMethod = SpatialHudConfig.get().selectedRenderMethod();
		// Keep the selected method intact. Rewriting it to the old affine mode
		// made a user-requested mesh/world method appear to ignore its selector.
		// A restart—or deliberately choosing a different method—will retry it.
		SpatialHud.LOGGER.error("Spatial HUD forced texture warp failed while {}. Requested {} remains selected; choose another method or restart to retry. A safe affine display is used only for this session.",
				stage, failedMethod, error);
	}

	private static float lerp(float from, float to, float amount) {
		return from + (to - from) * amount;
	}

	@FunctionalInterface
	interface HudRootRenderer {
		void extract(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker);
	}
}
