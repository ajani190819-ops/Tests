#!/usr/bin/env python3
"""Static regression contracts for the Spatial HUD Fabric mod
(spatial-hud-template-26.3/).

The mod cannot be compiled or run in this repository's verification environment
(no JDK, no Minecraft), so the two defects that reached a real client are pinned
at the source level instead, the same way the Windows apps' COM vtable slots are:

  1. Occlusion appeared to do nothing. The body-exclusion option was ANDed out
     of the occlusion decision in every view, and it defaults to on, so with
     Occlusion on the panel still took the see-through path over the world.
  2. The occluded panel frame ignored world depth. Fill and border drew through
     a debug render type while only the captured picture was depth-tested, so
     blocks hid the hotbar picture but not the purple frame around it.
  3. Capture-wrapper drift left the vanilla strip drawing at screen positions
     while the panel showed its captured copy (the bars flickering between the
     two), because the wrappers were re-applied exactly once per session.

Each test states the consequence in its failure message, so a regression reads
as the user-visible symptom rather than as a missing substring.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
MOD = ROOT / "spatial-hud-template-26.3" / "src" / "client" / "java" / "dev" / "arena" / "spatialhud"


def _read(name: str) -> str:
    return (MOD / name).read_text(encoding="utf-8")


class SpatialHudOcclusionTests(unittest.TestCase):
    def test_occlusion_is_only_overridden_in_third_person(self):
        src = _read("WorldSpaceHudRenderer.java")

        # The exception must be gated on the actual camera type...
        guard = src.find("options.getCameraType().isFirstPerson()")
        self.assertNotEqual(
            guard, -1,
            "the body-exclusion exception is no longer gated on the camera type, "
            "so it applies in first person too and silently cancels Occlusion")

        # ...and combined so that first person + Occlusion on => occluded.
        pattern = re.compile(
            r"boolean\s+occlude\s*=\s*cfg\.horizontalPanelOcclusion\s*"
            r"&&\s*!\s*\(\s*cfg\.horizontalPanelThirdPersonException\s*&&\s*thirdPerson\s*\)")
        self.assertIsNotNone(
            pattern.search(src),
            "occlude is not computed as occlusion AND NOT (exception AND third person); "
            "with the exception defaulting to on, any other combination makes the "
            "Occlusion toggle appear broken in first person")

        # The original defect, verbatim: exception applied in every view.
        broken = src.find("&& !cfg.horizontalPanelThirdPersonException;")
        self.assertEqual(
            broken, -1,
            "the occlusion decision again ANDs out the body exception in all views, "
            "which leaves the panel un-occluded at the default settings")

    def test_occluded_fill_and_border_are_depth_tested(self):
        src = _read("WorldSpaceSolidQuad.java")

        # The debug type was the actual defect: it is not part of the normal
        # depth-tested world, so the frame showed through blocks. Checked first,
        # because it is the specific regression this test exists for.
        self.assertEqual(
            src.find("debugFilledBox"), -1,
            "the panel frame draws through a debug render type again; that type does "
            "not depth-test against the world, so blocks hide the HUD picture but not "
            "the purple frame around it")
        self.assertNotEqual(
            src.find("RECT_OCCLUDED"), -1,
            "the occluded fill/border no longer have a depth-tested mode, so world "
            "geometry cannot hide the panel frame")
        self.assertNotEqual(
            src.find("RenderTypes.entityTranslucent(SolidColorTexture.ID, false)"), -1,
            "the occluded solid rectangles are not drawn with the depth-tested entity "
            "translucent type over the white texture")

        submitted = re.search(
            r"SubmitRenderPhases\.TRANSLUCENT_CUSTOM_GEOMETRY\s*,\s*"
            r"new TexturedSubmit\(Mode\.RECT_OCCLUDED", src)
        self.assertIsNotNone(
            submitted,
            "occluded rectangles are not submitted in the translucent phase with the "
            "captured band, so they would not sort after the opaque world")


class SpatialHudCaptureWrapperTests(unittest.TestCase):
    def test_displaced_capture_wrappers_are_reapplied(self):
        capture = _read("ExperimentalHudCapture.java")
        hud = _read("SpatialHud.java")

        self.assertNotEqual(
            capture.find("hotbarMissFrames"), -1,
            "the capture no longer counts frames where the hotbar never arrived, so "
            "displaced capture wrappers can never be detected")
        self.assertNotEqual(
            capture.find("hotbarMissFrames = 0"), -1,
            "the miss counter is never reset, so a single missed frame would look like "
            "permanent drift")
        self.assertNotEqual(
            capture.find("static int consecutiveHotbarMisses()"), -1,
            "the miss counter is not exposed, so nothing can act on wrapper drift")

        self.assertNotEqual(
            hud.find("ExperimentalHudCapture.consecutiveHotbarMisses()"), -1,
            "the client tick no longer watches for capture-wrapper drift; a mod that "
            "re-registers a strip root after our one-shot pass leaves the vanilla bars "
            "on screen while the panel shows its captured copy")

        # Presence of the call is not enough: a disabled branch would still contain
        # it. Pin the gated condition itself, so deadening the branch fails here.
        drift = re.search(
            r"else if \(wrapperReinforcements < MAX_WRAPPER_PASSES\s*"
            r"&& ExperimentalHudCapture\.consecutiveHotbarMisses\(\) >= MISSES_BEFORE_REWRAP\)",
            hud)
        self.assertIsNotNone(
            drift,
            "the drift repair is no longer a live branch gated on the miss counter and "
            "the pass cap, so displaced capture wrappers are never re-applied and the "
            "bars keep flickering between screen and panel")
        self.assertNotEqual(
            hud.find("MAX_WRAPPER_PASSES"), -1,
            "drift repair is unbounded; each pass nests another wrapper, so a registry "
            "that keeps drifting would grow wrappers without limit")


if __name__ == "__main__":
    unittest.main(verbosity=2)
