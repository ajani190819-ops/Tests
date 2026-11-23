#!/usr/bin/env python3
"""Portable review-tool/schema tests, NOT Windows/Orca/print acceptance."""
from contextlib import redirect_stderr, redirect_stdout
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools import inspect_orca_export as orca
from tools import test_radar_windows as radar


def fixture_result():
    return dict(mode="fixture", native_windows=True, passed=True, checks=500, clicks=375,
                text_overflows=0, concurrent_builds=2, blocked_http=0, http_requests=0)


class WeatherReviewToolTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="weather-review-tool-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def test_schema_accepts_complete_fixture_and_live_not_desktop_acceptance(self):
        radar.validate_result(fixture_result(), "fixture", 0)
        live = fixture_result()
        live.update(mode="live", clicks=0, concurrent_builds=0, http_requests=25)
        radar.validate_result(live, "live", 0)
        self.assertIn("not installed-EXE", radar.BOUNDARY)
        self.assertIn("Windows location", radar.BOUNDARY)

    def test_schema_refuses_missing_wrong_mode_or_coerced_booleans(self):
        for key, value in (("native_windows", False), ("native_windows", "true"),
                           ("passed", "true"), ("passed", 1), ("mode", "live")):
            data = fixture_result()
            data[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                radar.validate_result(data, "fixture", 0)
        for data in (None, {}, [], fixture_result()):
            with self.subTest(data=data), self.assertRaises(ValueError):
                radar.validate_result(data, "fixture", 1)
        for key in fixture_result():
            data = fixture_result()
            del data[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                radar.validate_result(data, "fixture", 0)

    def test_fixture_cannot_pass_without_click_stress_or_offline_gate(self):
        for key, value in (("checks", 0), ("checks", True), ("clicks", 124),
                           ("concurrent_builds", 0), ("blocked_http", 1), ("http_requests", 1),
                           ("text_overflows", 1), ("clicks", "375"), ("checks", -1)):
            data = fixture_result()
            data[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                radar.validate_result(data, "fixture", 0)
        data = fixture_result()
        data.update(mode="live", http_requests=0)
        with self.assertRaises(ValueError):
            radar.validate_result(data, "live", 0)

    def test_live_city_requires_complete_fresh_telemetry(self):
        valid = dict(city=0, passed=True, forecast=2, days=5, ensemble=1,
                     members=10, frames=8, view=2, base_pixels=589824)
        radar.validate_live_city(valid, 0)
        for key, value in (("city", 1), ("passed", "true"), ("forecast", 1), ("days", 4),
                           ("ensemble", 0), ("members", 1), ("frames", 0), ("view", 0),
                           ("base_pixels", 1000), ("members", True)):
            data = dict(valid, **{key: value})
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                radar.validate_live_city(data, 0)

    def test_output_directory_is_new_external_and_never_overwritten(self):
        output = self.directory / "new-review"
        self.assertEqual(radar.create_output_directory(output), output)
        sentinel = output / "profile.ini"
        sentinel.write_bytes(b"DO NOT TOUCH")
        with self.assertRaises(FileExistsError):
            radar.create_output_directory(output)
        self.assertEqual(sentinel.read_bytes(), b"DO NOT TOUCH")
        with self.assertRaises(ValueError):
            radar.create_output_directory(ROOT / "never-create-review")
        self.assertFalse((ROOT / "never-create-review").exists())
        with self.assertRaises(ValueError):
            radar.create_output_directory(Path("relative-review"))
        with self.assertRaises(FileNotFoundError):
            radar.create_output_directory(self.directory / "missing" / "child")

    def test_symlink_or_reparse_parent_is_not_followed(self):
        link = self.directory / "link"
        link.symlink_to(self.directory, target_is_directory=True)
        with self.assertRaises(ValueError):
            radar.create_output_directory(link / "child")
        self.assertFalse((self.directory / "child").exists())

    def test_windows_output_syntax_refuses_remote_relative_ads_and_parent_paths(self):
        for value in (r"C:\Temp\new-review", "D:/Temp/new-review"):
            radar.validate_windows_path(value)
        for value in (r"\\server\share\review", r"\\?\C:\review", "relative", r"C:relative",
                      r"\rooted", r"C:\file:stream", r"C:\Temp\..\profile", "C:/" + "x" * 180):
            with self.subTest(value=value), self.assertRaises(ValueError):
                radar.validate_windows_path(value)

    def test_non_windows_cannot_compile_execute_or_create_a_result(self):
        output = self.directory / "never-created"
        with mock.patch.object(radar.os, "name", "posix"), mock.patch.object(radar, "run_command") as command:
            with self.assertRaisesRegex(ValueError, "cross-build is not a native pass"):
                radar.run_review(str(output), live=True)
            command.assert_not_called()
        self.assertFalse(output.exists())

    def test_json_writer_refuses_an_existing_file(self):
        path = self.directory / "review.json"
        radar.write_json(path, {"owner_acceptance": False})
        before = path.read_bytes()
        with self.assertRaises(FileExistsError):
            radar.write_json(path, {"owner_acceptance": True})
        self.assertEqual(path.read_bytes(), before)
        self.assertIs(json.loads(before)["owner_acceptance"], False)

    def test_capture_manifest_requires_every_expected_file_and_records_exact_hashes(self):
        # Synthetic signatures test the manifest protocol, not WIC/image rendering.
        image = b"\x89PNG\r\n\x1a\n" + b"synthetic fixture" * 3
        names = [f"fixture-planner-category{i}-dpi96-fit100.png" for i in range(4)]
        names += [f"fixture-planner-rain-dpi{dpi}-fit{fit}.png" for dpi, fit in
                  ((96, 100), (96, 75), (96, 55), (144, 75), (192, 55))]
        names += [f"fixture-radar-city{city}-zoom{zoom}.png" for city in range(2) for zoom in (7, 10)]
        for name in names:
            (self.directory / name).write_bytes(image)
        manifest = radar.capture_manifest(self.directory, "fixture")
        self.assertEqual(len(manifest), 13)
        self.assertEqual({item["sha256"] for item in manifest}, {hashlib.sha256(image).hexdigest()})
        self.assertEqual({item["bytes"] for item in manifest}, {len(image)})
        self.assertTrue(all(item["file"].startswith("fixture/") for item in manifest))
        (self.directory / names[-1]).unlink()
        with self.assertRaises(FileNotFoundError):
            radar.capture_manifest(self.directory, "fixture")
        (self.directory / names[-1]).write_bytes(b"not a PNG" * 5)
        with self.assertRaises(ValueError):
            radar.capture_manifest(self.directory, "fixture")

    def test_process_failure_and_timeout_do_not_turn_into_a_pass(self):
        log = self.directory / "failed.log"
        with mock.patch.object(radar.subprocess, "run", return_value=subprocess.CompletedProcess([], 9, b"out", b"err")):
            self.assertEqual(radar.run_command(["synthetic"], log, 1), 9)
        self.assertEqual(log.read_bytes(), b"outerr")
        with mock.patch.object(radar.subprocess, "run", side_effect=subprocess.TimeoutExpired([], 1, b"partial", b"error")):
            with self.assertRaisesRegex(ValueError, "timed out"):
                radar.run_command(["synthetic"], self.directory / "timeout.log", 1)
        self.assertIn(b"not a pass", (self.directory / "timeout.log").read_bytes())

    def test_harness_keeps_production_include_explicit_live_and_new_state(self):
        source = (ROOT / "tests/native/radar_windows_review.c").read_text()
        self.assertIn('#include "../../weather-radar/src/weather_radar.c"', source)
        self.assertLess(source.index("#define COBJMACROS"), source.index("#include <windows.h>"))
        guard = source[source.index("static HINTERNET WINAPI ReviewHttpOpen(LPCWSTR agent, DWORD access,", source.index("#undef WinHttpOpen")):]
        self.assertLess(guard.index("if (!reviewLive)"), guard.index("return WinHttpOpen("))
        self.assertIn("return NULL;", guard[:guard.index("return WinHttpOpen(")])
        self.assertIn("CHECK(reviewBlockedHttp == 0 && reviewHttpRequests == 0);", source)
        self.assertIn("if (!CreateDirectoryW(outputDir, NULL)) return 2;", source)
        self.assertIn("wcscmp(argv[2], L\"--live\") != 0", source)
        self.assertIn("g_frameN = 0;\n        g_radarHost[0] = L'\\0';", source)
        self.assertIn("g_fc.have == 2", source)
        self.assertIn("g_ens.members >= 2", source)
        self.assertIn("RadarViewBuild(&g_places[city], 1, ticket)", source)
        self.assertIn("WaitForSingleObject(thread, 30000) == WAIT_OBJECT_0", source)
        wrapper = (ROOT / "tools/test_radar_windows.ps1").read_text(encoding="ascii")
        self.assertIn("test_radar_windows.py", wrapper)
        self.assertNotIn("ExecutionPolicy", wrapper)
        self.assertNotIn("CaptureAnnotations", wrapper)
        self.assertNotIn("-Force", wrapper)


class OrcaExportEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="orca-evidence-tool-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.path = self.directory / "private-model-name.gcode"

    def inspect(self, content):
        self.path.write_bytes(content if isinstance(content, bytes) else content.encode("utf-8"))
        return orca.inspect_export(self.path)

    def test_exact_hash_size_and_original_file_are_preserved(self):
        raw = b"\xef\xbb\xbf; generated by OrcaSlicer\r\nG1 X1 E0.1\r\n; CONFIG_BLOCK_START\r\n; layer_height = 0.3\r\n; CONFIG_BLOCK_END\r\n"
        data = self.inspect(raw)
        self.assertEqual(data["sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(data["bytes"], len(raw))
        self.assertEqual(data["lines"], 5)
        self.assertEqual(data["profile_values"]["layer_height"], ["0.3"])
        self.assertEqual(self.path.read_bytes(), raw)
        self.assertEqual(list(self.directory.iterdir()), [self.path])

    def test_archived_clean_named_inputs_are_recognized_as_infill_output(self):
        for relative in ("test-prints/t2-curved-perimeters/clean.gcode",
                         "test-prints/t3-multi-overhang/t3_20m37s.gcode"):
            with self.subTest(file=relative):
                data = orca.inspect_export(ROOT / relative)
                self.assertIs(data["recognized_processing_evidence"], True)
                self.assertEqual(data["processing_markers"][0],
                                 {"plugin": "unlayered-infill", "version": "0.4.8", "line": 1})
                self.assertEqual(data["acceptance"], "NOT_EVALUATED")
                self.assertEqual(data["profile_values"]["nozzle_diameter"], ["0.6"])
        t3 = orca.inspect_export(ROOT / "test-prints/t3-multi-overhang/t3_20m37s.gcode")
        self.assertEqual(t3["selection_metadata"]["slicing_pipeline_plugin"]["recognized"],
                         ["Unlayered Infill", "Wave Overhangs"])
        self.assertEqual(t3["wave_blocks"], {"begin": 0, "end": 0})

    def test_no_marker_is_not_freshness_or_host_execution_acceptance(self):
        data = orca.inspect_export(ROOT / "tests/fixtures/Cube^2_3m53s.gcode")
        self.assertIs(data["recognized_processing_evidence"], False)
        self.assertTrue(any("NOT proof" in warning for warning in data["warnings"]))
        self.assertIn("effective plugin", data["boundary"])
        self.assertEqual(data["acceptance"], "NOT_EVALUATED")

    def test_stamp_versions_wave_blocks_and_bounded_marker_list(self):
        data = self.inspect("; wave-overhangs v0.0.52 (toolpaths)\n" +
                            "; wave-overhangs edge clearance centerline=0.3\n" * 25 +
                            "; ==== WAVE OVERHANG BEGIN ====\nG1 X2\n; ==== WAVE OVERHANG END ====\n")
        self.assertEqual(data["processing_markers"][0]["version"], "0.0.52")
        self.assertEqual(data["processing_marker_count"], 26)
        self.assertEqual(len(data["processing_markers"]), 20)
        self.assertEqual(data["processing_markers_omitted"], 6)
        self.assertEqual(data["wave_blocks"], {"begin": 1, "end": 1})
        self.assertIs(self.inspect("; ==== WAVE OVERHANG BEGIN ====\n")["recognized_processing_evidence"], True)
        self.assertIs(self.inspect("; unlayered-infill vunknown\n")["recognized_processing_evidence"], True)
        self.assertIs(self.inspect("; unlayered-infillX v1\n")["recognized_processing_evidence"], False)

    def test_only_allowlisted_numeric_metadata_inside_config_is_reported(self):
        data = self.inspect("; layer_height = 9\n; CONFIG_BLOCK_START\n; layer_height = 0.3\n"
                            "; nozzle_diameter = 0.4,0.6\n; sparse_infill_density = 15%\n"
                            "; resolution = C:/Users/PRIVATE/file\n; machine_start_gcode = PRIVATE G28\n"
                            "; print_settings_id = PRIVATE_PRESET\n; CONFIG_BLOCK_END\n; layer_height = 8\n")
        self.assertEqual(data["profile_values"], {"layer_height": ["0.3"], "nozzle_diameter": ["0.4,0.6"],
                                                 "sparse_infill_density": ["15%"]})
        self.assertEqual(data["omitted_profile_values"], {"resolution": 1})
        self.assertNotIn("PRIVATE", json.dumps(data))
        self.assertNotIn("machine_start_gcode", json.dumps(data))

    def test_plugin_references_and_selection_are_separate_and_do_not_echo_private_keys(self):
        data = self.inspect('; CONFIG_BLOCK_START\n; plugins = "module;PRIVATE_UUID;Wave Overhangs";"module2;;PRIVATE_PLUGIN"\n'
                            '; slicing_pipeline_plugin = "Unlayered Infill"\n; CONFIG_BLOCK_END\n')
        self.assertEqual(data["selection_metadata"]["plugins"]["recognized"], ["Wave Overhangs"])
        self.assertEqual(data["selection_metadata"]["plugins"]["unrecognized_entries"], 1)
        self.assertEqual(data["selection_metadata"]["slicing_pipeline_plugin"]["recognized"], ["Unlayered Infill"])
        self.assertNotIn("PRIVATE", json.dumps(data))
        self.assertTrue(any("Config/preset override" in warning for warning in data["warnings"]))
        self.assertEqual(orca.selected_capabilities('"Wave Overhangs" trailing junk'), ([], 1))

    def test_conflicts_and_incomplete_blocks_are_not_silently_resolved(self):
        data = self.inspect("; CONFIG_BLOCK_START\n; layer_height = 0.2\n; layer_height = 0.3\n"
                            "; layer_height = 0.2\n; CONFIG_BLOCK_START\n")
        self.assertEqual(data["profile_values"]["layer_height"], ["0.2", "0.3"])
        self.assertEqual(data["conflicting_profile_keys"], ["layer_height"])
        self.assertEqual(data["config_blocks"], {"started": 2, "complete": 0, "malformed": 2})
        self.assertTrue(any("malformed config" in warning for warning in data["warnings"]))

    def test_binary_empty_undecodable_and_overlong_inputs_are_not_partial_successes(self):
        for raw in (b"", b"GCDEbinary", b"PK\x03\x04archive", b"G1 X1\x00\n", b"\xff\n", b"x" * (orca.MAX_LINE + 1)):
            with self.subTest(prefix=raw[:12]), self.assertRaises(ValueError):
                self.inspect(raw)

    def test_cli_rejects_recognized_processing_without_writing_or_leaking_paths(self):
        raw = b"; unlayered-infill v0.4.8 (non-planar sparse infill)\nG1 X1 E0.01\n"
        self.path.write_bytes(raw)
        out = io.StringIO()
        with redirect_stdout(out):
            code = orca.main([str(self.path), "--reject-processed"])
        self.assertEqual(code, 2)
        payload = json.loads(out.getvalue())
        self.assertEqual(payload["files"][0]["input_index"], 1)
        self.assertEqual(payload["errors"], [])
        self.assertNotIn("private-model-name", out.getvalue())
        self.assertNotIn(str(self.directory), out.getvalue())
        self.assertEqual(self.path.read_bytes(), raw)
        self.assertEqual(list(self.directory.iterdir()), [self.path])

    def test_cli_errors_are_explicit_nonzero_and_do_not_leak_missing_paths(self):
        self.path.write_bytes(b"G1 X1 E0.1\n")
        out = io.StringIO()
        with redirect_stdout(out), redirect_stderr(io.StringIO()):
            code = orca.main([str(self.path), str(self.directory / "PRIVATE-missing.gcode")])
        self.assertEqual(code, 1)
        payload = json.loads(out.getvalue())
        self.assertEqual(len(payload["files"]), 1)
        self.assertEqual(payload["errors"][0]["input_index"], 2)
        self.assertNotIn("PRIVATE", out.getvalue())
        self.assertNotIn("private-model-name", out.getvalue())


class OwnerHandoffContractTests(unittest.TestCase):
    def test_checklist_preserves_all_20_ids_and_safety_acceptance_boundary(self):
        import re
        checklist = (ROOT / ".agent/runbooks/owner-verification-checklist.md").read_text()
        ids = set(re.findall(r"\b(?:BT|APP|OPS|WX|KBL|HUD|ORCA|NAME|PLAN)-[0-9]{2}\b", checklist))
        self.assertEqual(ids, {"BT-01", "APP-01", "APP-02", "APP-03", "APP-04", "APP-05", "OPS-01", "WX-01", "WX-02", "WX-03",
                               "KBL-01", "HUD-01", "ORCA-01", "ORCA-02", "ORCA-03", "ORCA-04", "ORCA-05",
                               "NAME-01", "PLAN-01", "PLAN-02"})
        normalized = " ".join(checklist.split())
        self.assertIn("owner-reported good", checklist)
        self.assertIn("not a new request to retest", normalized)
        self.assertIn("do not launch a blocked executable", normalized)
        self.assertIn("Do not send the same screenshot again", normalized)
        self.assertIn("fresh `engine-6.log` received", checklist)
        self.assertNotIn("OPEN / LOG-GATED", checklist)
        self.assertIn("original three-method", checklist)
        self.assertIn("under identical settings", checklist)
        self.assertIn("not untouched controls", normalized)

    def test_current_status_keeps_open_focus_and_monitoring_distinct_from_owner_approval(self):
        import re
        checklist = (ROOT / ".agent/runbooks/owner-verification-checklist.md").read_text()
        rows = dict(re.findall(r"(?m)^\| [^|]+ \| ([A-Z]+-[0-9]{2}) \| ([^|]+) \|$", checklist))
        self.assertEqual(len(rows), 20)
        for ident in ("APP-01", "OPS-01", "KBL-01"):
            self.assertNotIn("Owner-reported good", rows[ident])
        for ident in ("BT-01", "APP-02", "APP-03", "APP-04", "WX-01", "WX-02", "WX-03"):
            self.assertIn("Owner-reported good", rows[ident])
        for ident in ("ORCA-04", "ORCA-05"):
            self.assertIn("PARKED", rows[ident])
        self.assertIn("DEFERRED", rows["PLAN-02"])
        self.assertIn("2/3. kindarandom/ all", checklist)
        self.assertIn("MONITOR", rows["APP-05"])
        self.assertIn("one icon currently working", rows["APP-05"])
        self.assertIn("no lasting startup fix claimed", rows["APP-05"])
        self.assertIn("Bluetooth Guard and Keyboard Lighting", checklist)
        self.assertIn("historical Hub presence", " ".join(checklist.split()))
        self.assertNotIn("Ask which app fails", checklist)

    def test_lighting_followup_retains_log_receipt_and_mock_hardware_boundary(self):
        runbook = (ROOT / ".agent/runbooks/test-lighting-input.md").read_text(encoding="ascii")
        normalized = " ".join(runbook.split())
        for phrase in ("fresh `engine-6.log`", "824", "814675", "does not prove no physical",
                       "not proof", "not an established build identity", "not a request for another engine log",
                       "not what the owner's keyboard emitted", "input-status.txt"):
            self.assertIn(phrase, normalized)
        self.assertIn("No other software was stopped", normalized)
        self.assertIn("No owner-hardware reproduction or successful hold is claimed", normalized)

    def test_open_handoff_respects_no_log_loop_and_source_vs_native_boundary(self):
        checklist = (ROOT / ".agent/runbooks/owner-verification-checklist.md").read_text()
        runbook = (ROOT / ".agent/runbooks/test-hub-open-to-front.md").read_text()
        lighting = (ROOT / ".agent/runbooks/test-lighting-input.md").read_text(encoding="ascii")
        state = (ROOT / ".agent/STATE.md").read_text()
        self.assertIn("No more log collection is the next action", checklist)
        self.assertIn("not proof of the owner's exact intermittent cause", " ".join(runbook.split()))
        self.assertIn("Open never calls it, kills a process or restarts the engine", " ".join(runbook.split()))
        self.assertIn("No additional log request", state)
        self.assertIn("observation request is superseded", " ".join(lighting.split()))
        self.assertNotIn("## Focused owner observation, only after a verified release", lighting)

    def test_continued_silent_lighting_failure_overrides_prior_source_success(self):
        paths = (".agent/STATE.md", ".agent/runbooks/test-hub-open-to-front.md",
                 ".agent/runbooks/owner-verification-checklist.md")
        for path in paths:
            with self.subTest(file=path):
                text = " ".join((ROOT / path).read_text().split())
                self.assertIn("Lighting", text)
                self.assertIn("nothing visible", text)
                self.assertIn("offscreen", text)
                self.assertNotIn("Open currently works", text)
                self.assertNotIn("Currently Open works", text)
        runbook = " ".join((ROOT / paths[1]).read_text().split())
        self.assertIn("APP-01 remains unresolved", runbook)
        self.assertIn("not proof of the owner's exact intermittent cause", runbook)
        self.assertIn("No new Guard outcome", runbook)
        self.assertIn("no new owner test/log request", runbook)
        self.assertIn("maximized placement is unchanged", runbook)

    def test_hud_guidance_exposes_source_mismatch_instead_of_dropping_old_request(self):
        config = (ROOT / "spatial-hud-template-26.3/src/client/java/dev/arena/spatialhud/SpatialHudConfig.java").read_text()
        start = config.index("RenderMethod selectedRenderMethod() {")
        self.assertIn("return RenderMethod.POLYGON_TEST;", config[start:config.index("\n\t}", start)])
        self.assertNotIn("CLASSIC_AFFINE,", config)
        self.assertNotIn("CAPTURED_MESH,", config)
        for file in ("spatial-hud-template-26.3/README.md", ".agent/facts/spatial-hud.md",
                     ".agent/runbooks/test-spatial-hud-f5w.md"):
            with self.subTest(file=file):
                text = (ROOT / file).read_text()
                self.assertIn("POLYGON_TEST", text)
                self.assertIn("Classic Affine", text)
                self.assertIn("Captured Mesh", text)
                self.assertIn("three-method", text)
                self.assertIn("scope reconciliation", text)
        runbook = (ROOT / ".agent/runbooks/test-spatial-hud-f5w.md").read_text()
        self.assertIn("unfulfilled original three-method test", runbook)
        self.assertIn("No hidden JSON edits", runbook)


if __name__ == "__main__":
    unittest.main()
