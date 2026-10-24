package dev.arena.spatialhud;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

class SpatialHudUnitTest {
	@Test
	void renderModeSelectionSynchronizesSliderAndCaptureFlags() {
		SpatialHudConfig cfg = new SpatialHudConfig();

		cfg.selectRenderMethod(SpatialHudConfig.RenderMethod.CLASSIC_AFFINE);
		assertEquals(1, cfg.renderModePicker);
		assertEquals(SpatialHudConfig.RenderMethod.CLASSIC_AFFINE, cfg.selectedRenderMethod());
		assertTrue(cfg.capturesTexture());
		assertEquals(0.80f, cfg.meshTopEdgeWidthMultiplier(), 1e-6f);

		cfg.selectRenderMethod(SpatialHudConfig.RenderMethod.CAPTURED_MESH);
		assertEquals(2, cfg.renderModePicker);
		assertTrue(cfg.capturesTexture());
		assertEquals(0.46f, cfg.meshTopEdgeWidthMultiplier(), 1e-6f);

		cfg.selectRenderMethod(SpatialHudConfig.RenderMethod.WORLD_SPACE_TEXTURE);
		assertEquals(3, cfg.renderModePicker);
		assertTrue(cfg.usesWorldSpaceTexture());

		cfg.selectRenderMethod(SpatialHudConfig.RenderMethod.POLYGON_TEST);
		assertEquals(4, cfg.renderModePicker);
		assertTrue(cfg.usesPolygonTest());
		assertFalse(cfg.capturesTexture());
	}

	@Test
	void virtualHudPlaneProjectsNarrowerFarEdgeAndCullsWhenLookingStraightUp() {
		SpatialHudConfig cfg = new SpatialHudConfig();

		// Looking down at the waist-height lectern keeps all corners in front of the camera.
		VirtualHudPlane lookingDown = VirtualHudPlane.forGui(cfg, 640, 360, 70.0, 45.0f);
		assertTrue(lookingDown.intersectsViewport());

		VirtualHudPlane.Point topLeft = lookingDown.projectWarped(
				lookingDown.sourceLeft(), lookingDown.sourceTop(),
				cfg.meshTopEdgeWidthMultiplier(), cfg.meshBottomEdgeWidthMultiplier());
		VirtualHudPlane.Point topRight = lookingDown.projectWarped(
				lookingDown.sourceRight(), lookingDown.sourceTop(),
				cfg.meshTopEdgeWidthMultiplier(), cfg.meshBottomEdgeWidthMultiplier());
		VirtualHudPlane.Point bottomLeft = lookingDown.projectWarped(
				lookingDown.sourceLeft(), lookingDown.sourceBottom(),
				cfg.meshTopEdgeWidthMultiplier(), cfg.meshBottomEdgeWidthMultiplier());
		VirtualHudPlane.Point bottomRight = lookingDown.projectWarped(
				lookingDown.sourceRight(), lookingDown.sourceBottom(),
				cfg.meshTopEdgeWidthMultiplier(), cfg.meshBottomEdgeWidthMultiplier());

		float topSpan = topRight.x() - topLeft.x();
		float bottomSpan = bottomRight.x() - bottomLeft.x();
		assertTrue(topSpan > 0.0f && bottomSpan > topSpan,
				"Far/top edge must be narrower than near/bottom edge for trapezoid projection");

		// Looking steeply upward moves the waist-height lectern out of the viewport.
		VirtualHudPlane lookingUp = VirtualHudPlane.forGui(cfg, 640, 360, 70.0, -75.0f);
		assertFalse(lookingUp.intersectsViewport());
	}
}
