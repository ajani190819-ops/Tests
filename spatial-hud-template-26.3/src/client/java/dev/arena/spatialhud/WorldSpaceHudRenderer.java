package dev.arena.spatialhud;

import com.mojang.renderpearl.api.commands.RenderPass;
import net.minecraft.client.renderer.SubmitNodeStorage;
import net.minecraft.client.renderer.feature.FeatureRenderDispatcher;
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
import com.mojang.math.Axis;
import net.minecraft.client.Options;
import net.minecraft.client.renderer.state.level.CameraEntityRenderState;
import net.minecraft.client.renderer.state.level.CameraRenderState;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.Vec3;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Matrix4fStack;
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
			CameraRenderState cs = context.levelState().cameraRenderState;
			// The world pass projection carries the head bob. Cancel it, so the panel
			// stays still while you run. The sprint FOV and your FOV still apply.
			PoseStack.Pose pose = WorldSpaceSolidQuad.cameraPose(cs.pos, inverseHeadBob(cs), cs.viewRotationMatrix);
			// Drawn later, over the hand, by drawOverHand.
			if (state.overHand()) {
				return;
			}
			WorldSpaceSolidQuad.Shape shape = state.shape();
			boolean open = !state.occludeBehindWorld();
			WorldSpaceSolidQuad.submitPanel(context.submitNodeCollector(), pose,
					SpatialHudConfig.get().horizontalPanelFill,
					SpatialHudConfig.get().horizontalPanelBorder,
					SpatialHudConfig.get().horizontalPanelHideEdges,
					open, shape);
			// Roadmap stage 3: the captured HUD, only once a frame of it exists.
			if (ExperimentalHudCapture.worldTextureView() != null) {
				CapturedHudTexture.register();
				WorldSpaceSolidQuad.submitCapturedBand(context.submitNodeCollector(), pose, open,
						WorldSpaceSolidQuad.bandGap(SpatialHudConfig.get()),
						state.uLeft(), state.uRight(), state.vTop(), state.vBottom(), shape);
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
				cfg.worldSpaceOccludeBehindWorld,
				WorldSpaceSolidQuad.Shape.of(toVec(bottomLeft), toVec(bottomRight), toVec(topLeft), 0.0,
						player.getEyePosition(partialTick)),
				false);
	}

	/**
	 * Method 4: a purple horizontal panel anchored to the player's feet and body
	 * heading, so turning the head does not move it. Its bottom edge (the
	 * hotbar side) is nearest the player and its top edge points away. The
	 * angle sets the look-down pitch at which the panel faces you square-on:
	 * 90 lies it flat, and smaller angles tilt its near edge toward you.
	 */
	/**
	 * Smoothed Method 4 placement for the wiggle option. The {@code wiggle*} fields
	 * are the lagging state, updated once per frame. The {@code shown*} fields are
	 * what is drawn: the lag scaled by the strength settings, and the position lag
	 * clamped to {@code panelWigglePositionMaxBlocks}.
	 */
	private static float wiggleYaw;
	/** The view's look-down pitch, lagged the same way as the heading. Used by view-locked presets. */
	private static float wigglePitch;
	private static float shownPitch;
	private static double wiggleX;
	private static double wiggleY;
	private static double wiggleZ;
	private static double wiggleHeight;
	private static boolean wiggleReady;
	/** The game time (level ticks plus partial tick) at the last wiggle update. */
	private static double wiggleGameTicks;
	private static boolean wiggleHasTime;
	private static float shownYaw;
	private static double shownX;
	private static double shownY;
	private static double shownZ;
	private static double shownHeight;

	/**
	 * Moves the lagging state toward the player's current placement, then works
	 * out what to draw. Each part ticked in the config eases toward its target
	 * with a time constant of its catch-up setting. Parts not ticked, and every
	 * part when the wiggle is off, follow the target exactly.
	 *
	 * <p>Easing is exponential in game time, so the same catch-up time gives the
	 * same lag at any frame rate. The step is capped, so a long hitch cannot
	 * sweep the panel. Nothing overshoots.</p>
	 */
	private static void updateWiggle(SpatialHudConfig cfg, float targetYaw, float targetPitch, Vec3 targetFeet,
			double targetHeight, double deltaSeconds) {
		double jump = Math.hypot(targetFeet.x - wiggleX, targetFeet.z - wiggleZ);
		if (!wiggleReady || !cfg.panelWiggle || jump > 4.0 || deltaSeconds < 0.0) {
			// A teleport, a first frame, a new world, a long gap while the panel was
			// hidden, or the wiggle being off snaps, so the panel never sweeps across
			// the map (deltaSeconds is -1 for these gap cases).
			wiggleYaw = targetYaw;
			wigglePitch = targetPitch;
			wiggleX = targetFeet.x;
			wiggleY = targetFeet.y;
			wiggleZ = targetFeet.z;
			wiggleHeight = targetHeight;
			wiggleReady = true;
		} else if (deltaSeconds > 0.0) {
			// Heading and height share one catch-up time; position has its own.
			// A frozen game (delta 0) holds the lag where it is.
			double step = Math.min(0.1, deltaSeconds);
			double alphaTurn = 1.0 - Math.exp(-step / clampedSeconds(cfg.panelWiggleSeconds));
			double alphaPosition = 1.0 - Math.exp(-step / clampedSeconds(cfg.panelWigglePositionSeconds));
			wiggleYaw = cfg.panelWiggleHeading
					? wiggleYaw + wrapRadians(targetYaw - wiggleYaw) * (float) alphaTurn
					: targetYaw;
			// Pitch lags with the heading, so tilting your view swings the panel too.
			wigglePitch = cfg.panelWiggleHeading
					? wigglePitch + (targetPitch - wigglePitch) * (float) alphaTurn
					: targetPitch;
			wiggleX = cfg.panelWigglePosition ? wiggleX + (targetFeet.x - wiggleX) * alphaPosition : targetFeet.x;
			wiggleY = cfg.panelWigglePosition ? wiggleY + (targetFeet.y - wiggleY) * alphaPosition : targetFeet.y;
			wiggleZ = cfg.panelWigglePosition ? wiggleZ + (targetFeet.z - wiggleZ) * alphaPosition : targetFeet.z;
			wiggleHeight = cfg.panelWiggleHeight
					? wiggleHeight + (targetHeight - wiggleHeight) * alphaTurn
					: targetHeight;
		}

		// What is drawn: scale each lag by its strength. Scaling keeps the same
		// direction and never overshoots the target.
		double headingScale = clamp(cfg.panelWiggleHeadingStrength, 0, 100) / 100.0;
		double positionScale = clamp(cfg.panelWigglePositionStrength, 0, 100) / 100.0;
		shownYaw = targetYaw + wrapRadians(wiggleYaw - targetYaw) * (float) headingScale;
		shownPitch = targetPitch + (wigglePitch - targetPitch) * (float) headingScale;

		double dx = (wiggleX - targetFeet.x) * positionScale;
		double dy = (wiggleY - targetFeet.y) * positionScale;
		double dz = (wiggleZ - targetFeet.z) * positionScale;
		double maxBlocks = clamp(cfg.panelWigglePositionMaxBlocks, 0.05, 1.0);
		double length = Math.sqrt(dx * dx + dy * dy + dz * dz);
		if (length > maxBlocks) {
			double clampScale = maxBlocks / length;
			dx *= clampScale;
			dy *= clampScale;
			dz *= clampScale;
		}
		shownX = targetFeet.x + dx;
		shownY = targetFeet.y + dy;
		shownZ = targetFeet.z + dz;
		shownHeight = targetHeight + (wiggleHeight - targetHeight) * positionScale;
	}

	/**
	 * Seconds of game time since the last wiggle update, from the level's tick
	 * counter plus the partial tick. Game time pauses with the game, so a frozen
	 * game gives 0 and the lag holds. Returns -1 for a new world or a gap longer
	 * than a quarter second, which tells the wiggle to snap.
	 */
	private static double wiggleDeltaSeconds(double gameTicks) {
		double delta = wiggleHasTime ? gameTicks - wiggleGameTicks : -1.0;
		wiggleGameTicks = gameTicks;
		wiggleHasTime = true;
		if (delta < 0.0 || delta > 5.0) {
			return -1.0;
		}
		return delta / 20.0;
	}

	/** A catch-up time in seconds, kept between 0.02 and 1.0 so it is always finite and never zero. */
	private static double clampedSeconds(double seconds) {
		return Math.max(0.02, Math.min(1.0, seconds));
	}

	private static double clamp(double value, double min, double max) {
		return value < min ? min : Math.min(value, max);
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

	/** Where the purple panel hangs from, and how far and high it sits from there. */
	private record PanelPlacement(boolean attachToCamera, double distance, double height) {
	}

	/** The placement for the chosen preset. Waist keeps the original feet-anchored sliders. */
	private static PanelPlacement panelPlacement(SpatialHudConfig cfg) {
		return switch (cfg.horizontalPanelPreset) {
			case FACE -> new PanelPlacement(true, cfg.horizontalPanelFaceDistance, cfg.horizontalPanelFaceHeight);
			case CUSTOM_ONE -> new PanelPlacement(cfg.horizontalPanelCustomOneAttachToCamera,
					cfg.horizontalPanelCustomOneDistance, cfg.horizontalPanelCustomOneHeight);
			case CUSTOM_TWO -> new PanelPlacement(cfg.horizontalPanelCustomTwoAttachToCamera,
					cfg.horizontalPanelCustomTwoDistance, cfg.horizontalPanelCustomTwoHeight);
			default -> new PanelPlacement(false, cfg.horizontalPanelDistance, cfg.horizontalPanelHeight);
		};
	}

	private static PlaneState purplePanelState(LocalPlayer player, SpatialHudConfig cfg, float partialTick,
			int guiWidth, int guiHeight) {
		ExperimentalHudCapture.SourceRect band = ExperimentalHudCapture.purpleSourceRect(cfg, guiWidth, guiHeight);
		PanelPlacement placement = panelPlacement(cfg);
		// Heading: the camera's view for camera-locked presets, otherwise the
		// Method 4 heading setting (body heading by default).
		// Interpolated to the partial tick, like the camera, so it turns smoothly.
		Vec3 cameraPos = Minecraft.getInstance().gameRenderer.mainCamera().position();
		float targetYaw = placement.attachToCamera()
				|| cfg.horizontalPanelAnchor == SpatialHudConfig.HorizontalPanelAnchor.CAMERA_YAW
				? radians(player.getViewYRot(partialTick))
				: radians(net.minecraft.util.Mth.rotLerp(partialTick, player.yBodyRotO, player.yBodyRot));
		// The point the panel hangs from: your feet, or the camera for camera-locked presets.
		Vec3 targetFeet = placement.attachToCamera() ? cameraPos : player.getPosition(partialTick);
		if (cfg.horizontalPanelFollowShoulderCamera && !placement.attachToCamera()) {
			// Shoulder cameras sit beside the eye. Slide the feet anchor by the same
			// sideways amount, in the panel's own right direction, so the body is not
			// in the line of sight. Feeding this through the wiggle keeps it from snapping.
			Vec3 eye = player.getEyePosition(partialTick);
			double sideX = -Math.cos(targetYaw);
			double sideZ = -Math.sin(targetYaw);
			double lateral = (cameraPos.x - eye.x) * sideX + (cameraPos.z - eye.z) * sideZ;
			targetFeet = targetFeet.add(sideX * lateral, 0.0, sideZ * lateral);
		}
		// The wiggle runs on game time, the same clock as the camera's partial tick.
		// Wall-clock time made the lag uneven from frame to frame.
		// Cast before adding: a float would lose precision after a few hours of play.
		double gameTicks = (double) Minecraft.getInstance().level.getGameTime() + partialTick;
		float targetPitch = radians(player.getViewXRot(partialTick));
		updateWiggle(cfg, targetYaw, targetPitch, targetFeet, placement.height(), wiggleDeltaSeconds(gameTicks));
		float bodyYaw = shownYaw;
		// The panel's right is the player's right. Minecraft's forward is (-sin, cos),
		// so the right is (-cos, -sin). The old sign mirrored the picture.
		float rightX = -cos(bodyYaw);
		float rightZ = -sin(bodyYaw);
		float forwardX = -sin(bodyYaw);
		float forwardZ = cos(bodyYaw);

		float width = clamp((float) cfg.horizontalPanelWidth(), 0.10f, 6.0f);
		// The band keeps its aspect, plus any fill strip under it (picture offset).
		float gap = WorldSpaceSolidQuad.bandGap(cfg);
		float height = width * (band.bottom() - band.top()) / (float) (band.right() - band.left())
				* WorldSpaceSolidQuad.panelHeightScale(gap);
		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;
		Point bottomLeft;
		Point bottomRight;
		Point topRight;
		Point topLeft;
		if (placement.attachToCamera()) {
			// View-locked (Face and camera-attached presets). The panel stands square to
			// the view, a fixed distance ahead and height above the view centre, so it
			// keeps its place on screen as you look around. The view direction is built
			// from the lagged yaw and pitch, so with the wiggle on, the panel swings
			// behind your turns and tilts a little, and the anchor lags your movement.
			// Matches Minecraft's view vector, with no roll: forward, then right, then
			// up = right x forward.
			float cosPitch = (float) Math.cos(shownPitch);
			float sinPitch = (float) Math.sin(shownPitch);
			float cosYaw = (float) Math.cos(shownYaw);
			float sinYaw = (float) Math.sin(shownYaw);
			float fx = -sinYaw * cosPitch;
			float fy = -sinPitch;
			float fz = cosYaw * cosPitch;
			float rx = -cosYaw;
			float ry = 0.0f;
			float rz = -sinYaw;
			float ux = ry * fz - rz * fy;
			float uy = rz * fx - rx * fz;
			float uz = rx * fy - ry * fx;
			float lookX = fx;
			float lookY = fy;
			float lookZ = fz;
			float dist = (float) Math.max(0.10, placement.distance());
			float upHeight = (float) shownHeight;
			float cx = (float) shownX + lookX * dist + ux * upHeight;
			float cy = (float) shownY + lookY * dist + uy * upHeight;
			float cz = (float) shownZ + lookZ * dist + uz * upHeight;
			bottomLeft = point(cx - rx * halfWidth - ux * halfHeight,
					cy - ry * halfWidth - uy * halfHeight,
					cz - rz * halfWidth - uz * halfHeight);
			bottomRight = point(cx + rx * halfWidth - ux * halfHeight,
					cy + ry * halfWidth - uy * halfHeight,
					cz + rz * halfWidth - uz * halfHeight);
			topRight = point(cx + rx * halfWidth + ux * halfHeight,
					cy + ry * halfWidth + uy * halfHeight,
					cz + rz * halfWidth + uz * halfHeight);
			topLeft = point(cx - rx * halfWidth + ux * halfHeight,
					cy - ry * halfWidth + uy * halfHeight,
					cz - rz * halfWidth + uz * halfHeight);
		} else {
			// Feet-anchored (Waist and the custom feet presets): a flat sheet at the
			// heading, at a set distance and height, tilted by the angle setting.
			float distance = (float) Math.max(0.10, placement.distance());
			float centerX = (float) shownX + forwardX * distance;
			float centerY = (float) shownY + (float) shownHeight;
			float centerZ = (float) shownZ + forwardZ * distance;
			float lookDown = radians(clamp(cfg.horizontalPanelAngle, 20, 90));
			// Unit vector along the panel's height, lying in the plane that faces
			// the player at the chosen look-down angle. At 90 it is the forward
			// direction, so the panel is exactly horizontal (normal along Y).
			float topX = forwardX * sin(lookDown);
			float topY = cos(lookDown);
			float topZ = forwardZ * sin(lookDown);
			bottomLeft = point(centerX - rightX * halfWidth - topX * halfHeight,
					centerY - topY * halfHeight,
					centerZ - rightZ * halfWidth - topZ * halfHeight);
			bottomRight = point(centerX + rightX * halfWidth - topX * halfHeight,
					centerY - topY * halfHeight,
					centerZ + rightZ * halfWidth - topZ * halfHeight);
			topRight = point(centerX + rightX * halfWidth + topX * halfHeight,
					centerY + topY * halfHeight,
					centerZ + rightZ * halfWidth + topZ * halfHeight);
			topLeft = point(centerX - rightX * halfWidth + topX * halfHeight,
					centerY + topY * halfHeight,
					centerZ - rightZ * halfWidth + topZ * halfHeight);
		}
		// Occlusion is the player's choice. On, blocks and mobs in front of the
		// panel hide it. Body exclusion (both views) lets the player's own body,
		// hand, armour and particles stop hiding it, and blocks and mobs stop too
		// (see SpatialHudConfig).
		boolean occlude = cfg.horizontalPanelOcclusion
				&& !cfg.horizontalPanelThirdPersonException;
		// Occlusion off needs the see-through path. If it cannot run, draw occluded.
		if (!occlude && !WorldSpaceSolidQuad.openPathReady()) {
			occlude = true;
		}
		// An open panel (occlusion off, or body exclusion) is drawn over the hand
		// instead, in drawOverHand, in both views. The world pass applies the head
		// bob and the sprint FOV to its projection, which made the open panel wobble
		// while running. The hand pass projection has neither. In first person this
		// is also what keeps the hand from covering the panel.
		boolean overHand = !occlude;
		// Curve: bend the flat panel around the viewer. The arc keeps the flat width.
		double curveRadians = Math.toRadians(clamp(cfg.horizontalPanelCurveDegrees, 0, 180));
		WorldSpaceSolidQuad.Shape shape = WorldSpaceSolidQuad.Shape.of(
				toVec(bottomLeft), toVec(bottomRight), toVec(topLeft), curveRadians, cameraPos);
		return new PlaneState(toPoint(shape.at(0f, 0f)), toPoint(shape.at(1f, 0f)),
				toPoint(shape.at(1f, 1f)), toPoint(shape.at(0f, 1f)),
				band.left() / (float) guiWidth, band.right() / (float) guiWidth,
				// The capture texture is stored bottom-up, but GUI coordinates run
				// top-down, so the vertical texture axis is flipped here.
				1.0f - band.top() / (float) guiHeight, 1.0f - band.bottom() / (float) guiHeight,
				occlude, shape, overHand);
	}

	/**
	 * The inverse of the head bob vanilla puts on the world projection. Mirrors
	 * GameRenderer.bobHurt and bobView, in the same order. Identity on any failure,
	 * so the panel just bobs as vanilla does.
	 */
	private static Matrix4f inverseHeadBob(CameraRenderState cs) {
		try {
			PoseStack bob = new PoseStack();
			CameraEntityRenderState e = cs.entityRenderState;
			Options options = Minecraft.getInstance().options;
			if (e.isLiving) {
				if (e.isDeadOrDying) {
					float duration = Math.min(e.deathTime, 20.0F);
					bob.rotateDegrees(Axis.ZP, 40.0F - 8000.0F / (duration + 200.0F));
				}
				float hurt = e.hurtTime;
				if (hurt >= 0.0F) {
					hurt /= e.hurtDuration;
					hurt = Mth.sin(hurt * hurt * hurt * hurt * (float) Math.PI);
					float rr = e.hurtDir;
					bob.rotateDegrees(Axis.YP, -rr);
					float tilt = (float) (-hurt * 14.0 * options.damageTiltStrength().get());
					bob.rotateDegrees(Axis.ZP, tilt);
					bob.rotateDegrees(Axis.YP, rr);
				}
			}
			if (options.bobView().get() && e.isPlayer) {
				float walk = e.backwardsInterpolatedWalkDistance;
				float amount = e.bob;
				bob.translate(
						Mth.sin(walk * (float) Math.PI) * amount * 0.5F,
						-Math.abs(Mth.cos(walk * (float) Math.PI) * amount),
						0.0F);
				bob.rotateDegrees(Axis.ZP, Mth.sin(walk * (float) Math.PI) * amount * 3.0F);
				bob.rotateDegrees(Axis.XP, Math.abs(Mth.cos(walk * (float) Math.PI - 0.2F) * amount) * 5.0F);
			}
			Matrix4f inverse = new Matrix4f(bob.last().pose());
			if (!inverse.isFinite()) {
				return new Matrix4f();
			}
			return inverse.invert();
		} catch (Throwable t) {
			return new Matrix4f();
		}
	}

	/**
	 * Scale in eye space that turns the HUD projection (fixed hudFov) into the world
	 * projection, so the hand-pass panel is the same size and place as in the world.
	 * Both projections share the aspect ratio, so one factor, from the vertical
	 * focal length, does both axes. Identity on any failure.
	 */
	private static Matrix4f fovCorrection(CameraRenderState cs) {
		float worldFocal = cs.projectionMatrix.m11();
		double hudFocal = 1.0 / Math.tan(Math.toRadians(cs.hudFov) * 0.5);
		double scale = worldFocal / hudFocal;
		if (!(scale > 0.0) || !Double.isFinite(scale)) {
			scale = 1.0;
		}
		return new Matrix4f().scaling((float) scale, (float) scale, 1.0f);
	}

	/**
	 * Draws the purple panel over the first-person hand, body and particles. Called
	 * from the mixin on the GameRenderer's 3D HUD pass, after the hand is drawn. The
	 * panel is open (no depth test), so nothing in the world hides it. Any failure
	 * is caught, and the panel stops drawing through the normal error path.
	 */
	public static void drawOverHand(RenderTarget target, CameraRenderState cs) {
		PlaneState state = planeState;
		if (state == null || !state.overHand() || !SpatialHudConfig.get().usesPurplePanel()) {
			return;
		}
		// The hand pass has the camera's rotation on the model-view stack, and that
		// is popped before this runs. Without it the panel would stay fixed in the
		// screen and show only when you look straight ahead. Apply it here, the same
		// way the hand pass does.
		Matrix4f viewRotation = cs.viewRotationMatrix;
		// The hand pass projection uses the fixed hudFov. Scale the panel so it
		// matches the world FOV (your FOV option, and the sprint zoom), not 70.
		PoseStack.Pose pose = WorldSpaceSolidQuad.cameraPose(cs.pos, fovCorrection(cs), viewRotation);
		Matrix4fStack modelView = RenderSystem.getModelViewStack();
		modelView.pushMatrix().mul(viewRotation);
		try {
			SpatialHudConfig cfg = SpatialHudConfig.get();
			WorldSpaceSolidQuad.Shape shape = state.shape();
			SubmitNodeStorage storage = new SubmitNodeStorage();
			WorldSpaceSolidQuad.submitPanel(storage, pose,
					cfg.horizontalPanelFill, cfg.horizontalPanelBorder, cfg.horizontalPanelHideEdges,
					true, shape);
			if (ExperimentalHudCapture.worldTextureView() != null) {
				CapturedHudTexture.register();
				WorldSpaceSolidQuad.submitCapturedBand(storage, pose, true,
						WorldSpaceSolidQuad.bandGap(cfg),
						state.uLeft(), state.uRight(), state.vTop(), state.vBottom(), shape);
			}
			try (FeatureRenderDispatcher.PreparedFrame frame = Minecraft.getInstance().gameRenderer
					.featureRenderDispatcher().prepareFrame(storage)) {
				if (!frame.isEmpty()) {
					try (RenderPass renderPass = RenderSystem.getDevice()
							.createCommandEncoder()
							.createRenderPass(
									() -> "Spatial HUD panel over hand", target.getColorTextureView(),
									Optional.empty(), target.getDepthTextureView(), OptionalDouble.empty())) {
						RenderSystem.bindDefaultUniforms(renderPass);
						FeatureRenderDispatcher.renderAllFeatures(renderPass, frame);
					}
				}
			}
		} catch (Throwable t) {
			planeState = null;
			ExperimentalHudCapture.worldTextureFailed(t);
		} finally {
			modelView.popMatrix();
		}
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
						  boolean occludeBehindWorld, WorldSpaceSolidQuad.Shape shape,
						  boolean overHand) {
	}

	private static Point point(float x, float y, float z) {
		return new Point(x, y, z);
	}

	private static Vec3 toVec(Point p) {
		return new Vec3(p.x(), p.y(), p.z());
	}

	private static Point toPoint(Vec3 v) {
		return new Point((float) v.x, (float) v.y, (float) v.z);
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
