package dev.arena.spatialhud;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.pipeline.TextureTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.renderpearl.api.GpuFormat;
import com.mojang.renderpearl.api.textures.GpuTextureView;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.render.GuiRenderer;
import net.minecraft.client.renderer.state.gui.GuiRenderState;
import net.minecraft.resources.Identifier;
import net.fabricmc.fabric.api.client.rendering.v1.hud.VanillaHudElements;
import org.joml.Vector4f;

import java.util.List;

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
 * <p>Texture capture is the required baseline for both presentation methods
 * (3 and 4). A GPU error latches a fallback for the current session: the
 * vanilla HUD is shown with a red indicator, and the selected method is kept,
 * so a restart or choosing the method again retries.</p>
 */
public final class ExperimentalHudCapture {
	private static final String TARGET_NAME = "Spatial HUD experimental bottom-strip capture";
	// The purple identity of both panel methods. The interior is deliberately
	// translucent so the captured HUD stays readable on top of it.
	private static final int PURPLE_BACKING_COLOR = 0x7031004D;
	private static final int PURPLE_EDGE_COLOR = 0xFFC75CFF;
	// Height, in GUI pixels above the bottom of the screen, of the strip band
	// the purple horizontal panel frames: hotbar, status bars, experience level,
	// and held-item text all draw inside it.
	private static final int PURPLE_SOURCE_HEIGHT = 72;

	private static GuiRenderState capturedState;
	private static GuiRenderer capturedRenderer;
	private static GuiGraphicsExtractor capturedGraphics;
	private static TextureTarget capturedTarget;
	private static boolean frameActive;
	private static boolean frameHasContent;
	private static boolean frameCapturedHotbar;
	private static boolean loggedHotbarExtraction;
	private static boolean loggedMissingHotbar;
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
				|| (!SpatialHudConfig.get().usesPurplePanel()
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
		} catch (Throwable t) {
			fallback(t, "preparing the selected bottom-HUD capture");
		}
	}

	/**
	 * True after a capture error for the currently requested method. Drives the
	 * small red indicator; the vanilla HUD is shown in its place.
	 */
	static boolean hasCaptureFailed() {
		return sessionFallback;
	}

	/** True only while the current gameplay HUD frame is collecting a texture. */
	static boolean isFrameActive() {
		return frameActive && !sessionFallback;
	}

	/**
	 * Extract one already-selected vanilla root into the private state. Returns
	 * false after an error so the caller can draw the vanilla root for
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
	 * Draws the purple identity into the same isolated source texture as the
	 * vanilla roots: a translucent interior (controlled by Show Backing Panel)
	 * and a 3 px border, both on the captured source rectangle. Both panel
	 * methods use it, so the border is lifted onto the panel with the HUD
	 * pixels. It runs before the vanilla roots extract, so the HUD stays
	 * readable on top of the interior.
	 */
	static boolean capturePanelDecorations(SpatialHudConfig cfg) {
		if (!isFrameActive()) {
			return false;
		}
		try {
			// Draw only inside the source rectangle the world panel samples.
			// Painting a margin outside it would make the border disappear
			// once the texture is placed on the panel.
			int left;
			int right;
			int top;
			int bottom;
			if (cfg.usesPurplePanel()) {
				SourceRect band = purpleSourceRect(cfg, guiWidth, guiHeight);
				left = band.left();
				right = band.right();
				top = band.top();
				bottom = band.bottom();
			} else {
				VirtualHudPlane plane = VirtualHudPlane.forGui(cfg, guiWidth, guiHeight);
				left = (int) Math.floor(plane.sourceLeft());
				right = (int) Math.ceil(plane.sourceRight());
				top = (int) Math.floor(plane.sourceTop());
				bottom = (int) Math.ceil(plane.sourceBottom());
			}
			if (cfg.showPanel) {
				capturedGraphics.fill(left, top, right, bottom, PURPLE_BACKING_COLOR);
			}
			capturedGraphics.fill(left, top, right, top + 3, PURPLE_EDGE_COLOR);
			capturedGraphics.fill(left, bottom - 3, right, bottom, PURPLE_EDGE_COLOR);
			capturedGraphics.fill(left, top, left + 3, bottom, PURPLE_EDGE_COLOR);
			capturedGraphics.fill(right - 3, top, right, bottom, PURPLE_EDGE_COLOR);
			frameHasContent = true;
			return true;
		} catch (Throwable t) {
			fallback(t, "adding the selected bottom-HUD panel decorations to the capture");
			return false;
		}
	}

	/**
	 * The strip the purple horizontal panel samples and frames: the full width of
	 * the captured strip, but only its bottom {@link #PURPLE_SOURCE_HEIGHT} GUI
	 * pixels. Those pixels hold the HUD, so the panel is filled by it rather than
	 * showing it in a small band under empty purple. The bottom edge is clamped
	 * to the real GUI, because the strip samples a few pixels below the screen
	 * that would otherwise arrive transparent.
	 */
	static SourceRect purpleSourceRect(SpatialHudConfig cfg, int guiWidth, int guiHeight) {
		VirtualHudPlane plane = VirtualHudPlane.forGui(cfg, guiWidth, guiHeight);
		int left = (int) Math.floor(Math.max(0.0f, plane.sourceLeft()));
		int right = (int) Math.ceil(Math.min(guiWidth, plane.sourceRight()));
		int bottom = (int) Math.ceil(Math.min(guiHeight, plane.sourceBottom()));
		int top = Math.max(0, bottom - PURPLE_SOURCE_HEIGHT);
		return new SourceRect(left, top, right, bottom);
	}

	/** Inclusive-exclusive GUI pixel bounds of a sampled panel band. */
	record SourceRect(int left, int top, int right, int bottom) {
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
	 * those roots to our private texture. The world-space renderer then draws
	 * that texture for both panel methods. Nothing is drawn on the GUI here and
	 * no projection state changes.
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

				// Level rendering happens before GUI extraction. The world renderer
				// intentionally draws this finished texture on the next frame.
				worldTextureReady = true;
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
		// Keep the selected method intact so the failure is never mistaken for a
		// different renderer.
		// A restart—or deliberately choosing a different method—will retry it.
		SpatialHud.LOGGER.error("Spatial HUD texture capture failed while {}. Requested {} remains selected; the vanilla HUD is shown with a red indicator. Choose another method or restart to retry.",
				stage, failedMethod, error);
	}

	@FunctionalInterface
	interface HudRootRenderer {
		void extract(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker);
	}
}
