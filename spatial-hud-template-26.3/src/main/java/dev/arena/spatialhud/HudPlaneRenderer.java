package dev.arena.spatialhud;

import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.Tesselator;
import dev.arena.spatialhud.mixin.GuiAccessor;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderContext;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.player.PlayerRideableJumping;
import net.minecraft.client.renderer.Camera;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.world.phys.Vec3;
import org.joml.Matrix4f;
import org.joml.Quaternionf;

/**
 * The heart of the mod: draws the vanilla bottom-HUD strip onto a tilted 3D
 * plane floating in front of the camera. (Mojang mappings edition.)
 *
 * Technique ("projection swap"):
 *  1. Vanilla draws its HUD through a 2D orthographic projection. We cancel
 *     that (see GuiMixin).
 *  2. At the end of the world render pass we install our own projection:
 *        perspective (borrowed from the running frame) x plane x GUI ortho
 *     and set the model-view to identity. To the HUD code nothing changed —
 *     it still "draws at pixel coordinates" — but those pixels now land on a
 *     quad floating in the world, with real perspective.
 *  3. We call vanilla's own render methods via accessors, so hearts/hunger/
 *     hotbar logic (effects, low-health flash, selected slot) just works.
 *
 * Cost: one extra draw of a few hundred 2D quads per frame. No framebuffers,
 * no texture copies, no per-pixel work.
 */
public final class HudPlaneRenderer {

    private HudPlaneRenderer() {
    }

    /** Smoothed camera rotation for the sway effect. */
    private static float swayYaw, swayPitch;

    /** After any unexpected rendering error: log once, self-disable, never crash the game. */
    private static boolean hardFailed = false;

    public static void register() {
        WorldRenderEvents.LAST.register(HudPlaneRenderer::render);
    }

    private static void render(WorldRenderContext ctx) {
        if (hardFailed) return;
        try {
            renderInner(ctx);
        } catch (Throwable t) {
            hardFailed = true;
            SpatialHudConfig cfg = SpatialHudConfig.get();
            cfg.enabled = false;
            SpatialHudConfig.save();
            SpatialHud.LOGGER.error(
                    "Spatial HUD hit an error and disabled itself (flat HUD is restored). Report this:", t);
        }
    }

    private static void renderInner(WorldRenderContext ctx) {
        Minecraft minecraft = Minecraft.getInstance();
        SpatialHudConfig cfg = SpatialHudConfig.get();

        if (!cfg.enabled || minecraft.player == null || minecraft.level == null) return;
        if (minecraft.options.hideGui || minecraft.screen != null) return;

        Camera cam = ctx.camera();
        if (cam == null) return;

        int sw = minecraft.getWindow().getGuiScaledWidth();
        int sh = minecraft.getWindow().getGuiScaledHeight();

        // ---- 1. sway: the plane lags a touch behind fast turns -------------
        float yaw = cam.getYRot();
        float pitch = cam.getXRot();
        swayYaw += (yaw - swayYaw) * 0.15f;
        swayPitch += (pitch - swayPitch) * 0.15f;
        float yawOff = (float) Math.toRadians(yaw - swayYaw) * 0.35f * (float) cfg.sway;
        float pitchOff = (float) Math.toRadians(pitch - swayPitch) * 0.35f * (float) cfg.sway;

        // ---- 2. the plane transform (world space) --------------------------
        float s = (float) cfg.planeWidth / (float) sw;   // gui pixels -> blocks
        float planeHalfH = s * (float) sh * 0.5f;

        Vec3 camPos = cam.getPosition();
        Quaternionf camRot = cam.getRotation();

        Matrix4f plane = new Matrix4f();
        plane.translate((float) camPos.x,
                        (float) camPos.y - (float) cfg.height + planeHalfH,
                        (float) camPos.z);
        plane.rotate(camRot);                                   // faces the camera
        plane.rotateY(yawOff);                                  // sway lag
        plane.rotateX(pitchOff);
        plane.rotateX((float) Math.toRadians(cfg.tiltDegrees)); // lean back
        plane.scale(s, -s, s);                                  // gui y grows downward

        // GUI ortho: (0..sw, 0..sh) onto the plane's local NDC space
        Matrix4f gui = new Matrix4f().ortho(0.0f, (float) sw, (float) sh, 0.0f, -3000.0f, 3000.0f);

        // final = perspective * plane * guiOrtho
        Matrix4f proj = new Matrix4f(RenderSystem.getProjectionMatrix())
                .mul(plane)
                .mul(gui);

        // ---- 3. swap projection + model-view --------------------------------
        Matrix4f oldProj = new Matrix4f(RenderSystem.getProjectionMatrix());
        PoseStack mv = RenderSystem.getModelViewStack();   // VERIFY: pushPose/popPose/setIdentity
        mv.pushPose();
        mv.setIdentity();
        RenderSystem.applyModelViewMatrix();
        RenderSystem.setProjectionMatrix(proj, VertexSortingHolder.BY_DISTANCE);
        RenderSystem.disableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();

        // ---- 4. draw the HUD onto the plane ----------------------------------
        MultiBufferSource.BufferSource immediate =
                MultiBufferSource.immediate(Tesselator.getInstance().getBuilder());
        GuiGraphics gg = new GuiGraphics(minecraft, immediate); // VERIFY: ctor (Minecraft, MultiBufferSource)

        if (cfg.showPanel) {
            int x0 = sw / 2 - 95, x1 = sw / 2 + 95;
            int y0 = sh - 48, y1 = sh - 2;
            gg.fill(x0 - 4, y0 - 4, x1 + 4, y1 + 4, 0x90000000); // body
            gg.fill(x0 - 4, y0 - 4, x1 + 4, y0 - 2, 0x70404040); // top edge
        }

        GuiAccessor hud = (GuiAccessor) minecraft.gui;
        DeltaTracker dt = tickCounter(minecraft, ctx);

        if (cfg.showHotbar) {
            hud.invokeRenderHotbar(gg, dt);
        }

        boolean survivalBars = minecraft.gameMode != null
                && minecraft.gameMode.hasStatusBars();              // VERIFY: hasStatusBars

        PlayerRideableJumping rideable = minecraft.player.jumpableVehicle(); // VERIFY: jumpableVehicle
        if (cfg.showMountBars && rideable != null) {
            hud.invokeRenderJumpMeter(rideable, gg, sw / 2 - 91);
        } else if (survivalBars) {
            if (cfg.showBars) hud.invokeRenderPlayerHealth(gg);
            if (cfg.showXp) hud.invokeRenderExperienceBar(gg, sw / 2 - 91);
        } else if (cfg.showMountBars && minecraft.player.isPassenger()) {
            hud.invokeRenderVehicleHealth(gg);
        }

        if (cfg.showHeldItemName) {
            hud.invokeRenderSelectedItemName(gg);
        }

        immediate.endBatch();

        // ---- 5. restore ------------------------------------------------------
        RenderSystem.enableDepthTest();
        mv.popPose();
        RenderSystem.applyModelViewMatrix();
        RenderSystem.setProjectionMatrix(oldProj, VertexSortingHolder.BY_DISTANCE);
    }

    private static DeltaTracker tickCounter(Minecraft minecraft, WorldRenderContext ctx) {
        try {
            return ctx.tickCounter();          // VERIFY: older Fabric API had tickDelta()
        } catch (NoSuchMethodError ignored) {
            return minecraft.getTimer();       // VERIFY: getTimer
        }
    }
}
