package dev.arena.spatialhud;

import com.mojang.blaze3d.systems.RenderSystem;
import dev.arena.spatialhud.mixin.InGameHudAccessor;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderContext;
import net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.render.Camera;
import net.minecraft.client.render.RenderTickCounter;
import net.minecraft.client.render.Tessellator;
import net.minecraft.client.render.VertexConsumerProvider;
import net.minecraft.client.util.math.MatrixStack;
import net.minecraft.util.math.Vec3d;
import org.joml.Matrix4f;
import org.joml.Quaternionf;

/**
 * The heart of the mod: draws the vanilla bottom-HUD strip onto a tilted
 * 3D plane floating in front of the camera.
 *
 * Technique ("projection swap"):
 *  1. Vanilla draws its HUD through a 2D orthographic projection. We cancel
 *     that (see InGameHudMixin).
 *  2. At the end of the world render pass we install our own projection:
 *        perspective (borrowed from the running frame)  x  plane transform  x  GUI ortho
 *     and set the model-view to identity. To the HUD code, nothing has
 *     changed — it still "draws at pixel coordinates" — but those pixels now
 *     land on a quad floating in the world, with real perspective.
 *  3. We call vanilla's own render methods via accessors, so hearts/hunger/
 *     hotbar logic (including effects, low-health flash, selected slot) all
 *     just works, unmodified.
 *
 * Cost: one extra draw of a few hundred 2D quads per frame. No framebuffers,
 * no texture copies, no per-pixel work — cheaper than an Exordium-style
 * capture by construction.
 */
public final class HudPlaneRenderer {

    private HudPlaneRenderer() {
    }

    /** Smoothed camera rotation for the sway effect. */
    private static float swayYaw, swayPitch;

    public static void register() {
        WorldRenderEvents.LAST.register(HudPlaneRenderer::render);
    }

    private static void render(WorldRenderContext ctx) {
        MinecraftClient client = MinecraftClient.getInstance();
        SpatialHudConfig cfg = SpatialHudConfig.get();

        if (!cfg.enabled || client.player == null || client.world == null) return;
        if (client.options.hudHidden || client.currentScreen != null) return;

        Camera cam = ctx.camera();
        if (cam == null) return;

        int sw = client.getWindow().getScaledWidth();
        int sh = client.getWindow().getScaledHeight();

        // ---- 1. sway: the plane lags a touch behind fast turns -------------
        float yaw = cam.getYaw();
        float pitch = cam.getPitch();
        swayYaw += (yaw - swayYaw) * 0.15f;
        swayPitch += (pitch - swayPitch) * 0.15f;
        float yawOff = (float) Math.toRadians(yaw - swayYaw) * 0.35f * (float) cfg.sway;
        float pitchOff = (float) Math.toRadians(pitch - swayPitch) * 0.35f * (float) cfg.sway;

        // ---- 2. the plane transform (world space) --------------------------
        float s = (float) cfg.planeWidth / (float) sw;   // gui pixels -> blocks
        float planeHalfH = s * (float) sh * 0.5f;

        Vec3d camPos = cam.getPos();
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
        MatrixStack mv = RenderSystem.getModelViewStack();   // VERIFY POINT 3
        mv.pushMatrix();
        mv.loadIdentity();
        RenderSystem.applyModelViewMatrix();
        RenderSystem.setProjectionMatrix(proj, VertexSorterHolder.BY_DISTANCE);
        RenderSystem.disableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();

        // ---- 4. draw the HUD onto the plane ----------------------------------
        VertexConsumerProvider.Immediate immediate =
                VertexConsumerProvider.immediate(Tessellator.getInstance().getBuffer());
        DrawContext dc = new DrawContext(client, immediate); // VERIFY POINT 5

        if (cfg.showPanel) {
            int x0 = sw / 2 - 95, x1 = sw / 2 + 95;
            int y0 = sh - 48, y1 = sh - 2;
            dc.fill(x0 - 4, y0 - 4, x1 + 4, y1 + 4, 0x90000000); // body
            dc.fill(x0 - 4, y0 - 4, x1 + 4, y0 - 2, 0x70404040); // top edge
        }

        InGameHudAccessor hud = (InGameHudAccessor) client.inGameHud;
        RenderTickCounter tc = tickCounter(client, ctx);

        if (cfg.showHotbar) {
            hud.invokeRenderHotbar(dc, tc);
        }

        boolean survivalBars = client.interactionManager != null
                && client.interactionManager.hasStatusBars();

        if (cfg.showMountBars && client.player.getJumpingMount() != null) {   // VERIFY POINT 6
            hud.invokeRenderMountJumpBar(dc, sw / 2 - 91);
        } else if (survivalBars) {
            if (cfg.showBars) hud.invokeRenderStatusBars(dc);
            if (cfg.showXp) hud.invokeRenderExperienceBar(dc, sw / 2 - 91);
        } else if (cfg.showMountBars && client.player.hasVehicle()) {
            hud.invokeRenderMountHealth(dc);
        }

        if (cfg.showHeldItemName) {
            hud.invokeRenderHeldItemTooltip(dc);
        }

        immediate.draw(); // flush all the above to the GPU

        // ---- 5. restore ------------------------------------------------------
        RenderSystem.enableDepthTest();
        mv.popMatrix();
        RenderSystem.applyModelViewMatrix();
        RenderSystem.setProjectionMatrix(oldProj, VertexSorterHolder.BY_DISTANCE);
    }

    private static RenderTickCounter tickCounter(MinecraftClient client, WorldRenderContext ctx) {
        try {
            return ctx.tickCounter();          // VERIFY POINT 2 (older API: ctx.tickDelta())
        } catch (NoSuchMethodError ignored) {
            return client.getRenderTickCounter();
        }
    }
}
