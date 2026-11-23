#!/usr/bin/env python3
"""Portable App Review policy/safety tests, NOT native/owner acceptance."""
from contextlib import redirect_stdout
import html
import base64
import io
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools import build_windows as builder
from tools import test_app_review_windows as native


class AppReviewPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="app-review-policy-")
        cls.executable = Path(cls.temporary.name) / "policy"
        subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/native/app_review_policy.c"), "-o", str(cls.executable)],
                       capture_output=True, text=True, check=True, timeout=30)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def policy(self, name):
        result = subprocess.run([str(self.executable), name], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def test_every_gate_is_fail_closed(self):
        self.policy("gates")

    def test_dimensions_bound_memory_and_reject_overflow(self):
        self.policy("dimensions")

    def test_worker_numbers_reject_overflow_and_non_decimal_tokens(self):
        self.policy("numbers")

    def test_known_image_names_are_exact_case_insensitive_categories_only(self):
        self.policy("apps")

    def test_uniform_or_unpainted_pixels_cannot_be_evidence(self):
        self.policy("pixels")

    def test_no_image_status_grants_a_pass(self):
        self.policy("statuses")

    def test_json_escapes_selected_caption_including_unicode_and_controls(self):
        text = self.policy("json")
        self.assertEqual(json.loads(text), "Chosen <script>alert('x')</script> & \"quote\" \\ tab\t newline\n café 🐟")
        self.assertNotIn("<script>", text)
        self.assertTrue(text.isascii())

    def test_html_caption_is_inert_and_round_trips_unicode(self):
        text = self.policy("html")
        self.assertNotIn("<", text)
        self.assertEqual(html.unescape(text), "Chosen <script>alert('x')</script> & \"quote\" \\ café 🐟")
        self.assertIn("&quot;", text)


class AppReviewSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = (ROOT / "app-review/src/app_review.c").read_text(encoding="ascii")
        cls.capture = (ROOT / "app-review/src/parts/capture.c").read_text(encoding="ascii")
        cls.wgc = (ROOT / "app-review/src/parts/wgc.c").read_text(encoding="ascii")
        cls.ui = (ROOT / "app-review/src/parts/ui.c").read_text(encoding="ascii")
        cls.report = (ROOT / "app-review/src/parts/report.c").read_text(encoding="ascii")

    def test_capture_has_no_target_mutation_or_desktop_fallback(self):
        for name in ("SendInput", "keybd_event", "mouse_event", "SetWindowPos", "MoveWindow", "ShowWindow",
                     "SetForegroundWindow", "SetWindowPlacement", "AttachThreadInput", "SetWindowText",
                     "SetWindowDisplayAffinity", "SetThreadExecutionState", "WM_COMMAND", "BM_CLICK",
                     "WM_KEYDOWN", "WM_LBUTTONDOWN", "GetDC(NULL)", "GetWindowDC", "BitBlt", "GetDesktopWindow",
                     "OpenClipboard", "ShellExecute", "WriteProcessMemory", "RegSet", "BluetoothSet", "ExecutionPolicy"):
            with self.subTest(api=name):
                self.assertNotIn(name, self.capture)
        messages = re.findall(r"SendMessageTimeoutW\(window,\s*(\w+)", self.capture)
        self.assertEqual(messages, ["WM_NULL"])
        self.assertIn("ReviewWindowPixels(window, &pixels, &size)", self.capture)
        for forbidden in ("PrintWindow(", "BitBlt(", "CreateForMonitor(", "IsBorderRequired", "RequestAccessAsync"):
            self.assertNotIn(forbidden, self.capture + self.wgc)
        for path in ("config.ini", "panel.json", "theme.json", "presets.ini", "engine.log", "rules.txt"):
            self.assertNotIn(path, self.root + self.capture + self.ui)

    def test_checks_precede_capture_and_revalidate_before_saving(self):
        function = self.capture[self.capture.index("static void ReviewCaptureWindow"):self.capture.index("static int ReviewReadFixed")]
        self.assertLess(function.index("ReviewSnapshot("), function.index("SendMessageTimeoutW("))
        self.assertLess(function.index("SendMessageTimeoutW("), function.index("ReviewWindowPixels("))
        after = function[function.index("ReviewWindowPixels("):]
        for text in ("ReviewSnapshot(window, expected", "before.showCmd == after.showCmd",
                     "before.flags == after.flags", "before.rcNormalPosition", "result->dpi == ReviewDpi(window)",
                     "if (after_status != REVIEW_IMAGE || !result->placement_unchanged)",
                     "else if (!ReviewPixelsUsable(pixels, (size_t)size.width * size.height)) result->status = REVIEW_BLANK;"):
            self.assertIn(text, after)
        self.assertLess(after.index("ReviewPixelsUsable"), after.index("ReviewSavePng"))

    def test_os_capture_keeps_protection_and_finishes_its_own_session(self):
        for text in ("->IsSupported(supported, &is_supported)", "!is_supported) hr = E_NOTIMPL",
                     "->CreateForWindow(interop, window, &ReviewIID_Item", "->CreateFreeThreaded(pool_statics",
                     "->StartCapture(session)", "GetTickCount64() - started < 3000",
                     "ReviewCloseObject(frame); ReviewCloseObject(session); ReviewCloseObject(pool);",
                     "ReviewIID_Direct3DDevice, (void **)&winrt_device", "D3D11_USAGE_STAGING", "D3D11_CPU_ACCESS_READ",
                     "size->width != item_size.width || size->height != item_size.height"):
            self.assertIn(text, self.wgc)
        self.assertIn('LOAD_LIBRARY_SEARCH_SYSTEM32', self.wgc)
        self.assertIn('c.minimized = IsIconic(window);', self.capture)
        self.assertIn('c.visible = IsWindowVisible(window);', self.capture)
        self.assertIn('if (!ReviewCloaked(window, &cloaked)) return REVIEW_QUERY_FAILED;', self.capture)
        self.assertIn('BOOL known = GetWindowDisplayAffinity(window, &affinity);', self.capture)
        self.assertIn('if (known) c.affinity = affinity;', self.capture)
        self.assertIn('SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 750', self.capture)

    def test_single_instance_ui_reveals_only_the_utility(self):
        self.assertIn('CreateMutexW(NULL, FALSE, L"Local\\\\Tests_AppReview_UI_v1")', self.ui)
        self.assertIn('GetLastError() == ERROR_ALREADY_EXISTS', self.ui)
        self.assertIn('FindWindowW(L"Tests_AppReview_LocalOnly_v1", NULL)', self.ui)
        self.assertIn('return ReviewLaunchUi(show);', self.root)

    def test_worker_is_owned_suspended_and_bounded_not_the_target(self):
        src = self.capture[self.capture.index("static void ReviewRunWorker"):]
        self.assertIn("CreateProcessW(exe, command", src)
        self.assertIn("CREATE_SUSPENDED | CREATE_NO_WINDOW", src)
        self.assertIn("JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE", src)
        self.assertLess(src.index("AssignProcessToJobObject"), src.index("ResumeThread"))
        self.assertIn("GetTickCount64() - started >= timeout_ms", src)
        self.assertIn("result->status = InterlockedCompareExchange(&g_cancel, 0, 0) ? REVIEW_CANCELLED : REVIEW_TIMEOUT;", src)
        self.assertIn("TerminateJobObject(job, 5)", src)
        self.assertEqual(re.findall(r"TerminateProcess\(([^,]+)", self.capture), ["child.hProcess"])
        self.assertNotIn("PROCESS_TERMINATE", self.root + self.capture)

    def test_selection_identity_is_transported_not_rebuilt_from_a_reused_handle(self):
        self.assertIn("ReviewWriteNew(request, window, sizeof(*window))", self.capture)
        self.assertIn("ReviewReadFixed(request, &expected, sizeof(expected))", self.capture)
        self.assertIn("expected.hwnd != hwnd || expected.pid != pid || expected.created != created", self.capture)
        self.assertIn("ReviewDescribe(window, current) && ReviewSameIdentity(expected, current)", self.capture)
        self.assertIn("a->hwnd == b->hwnd && a->pid == b->pid && a->created == b->created", self.root)
        self.assertIn("wcscmp(a->class_name, b->class_name) == 0", self.root)
        self.assertIn("ReviewSameIdentity(window, &captured.window)", self.capture)
        self.assertIn("result->bytes == sizeof(*result) && result->status < REVIEW_STATUS_COUNT", self.capture)

    def test_output_is_exclusive_local_and_never_an_app_profile(self):
        self.assertIn('L"TestsSuiteReview"', self.root)
        self.assertIn("&FOLDERID_LocalAppData", self.root)
        self.assertIn("!= DRIVE_FIXED", self.root)
        self.assertIn("FILE_ATTRIBUTE_REPARSE_POINT", self.root)
        self.assertIn("if (!CreateDirectoryW(out, NULL)) return 0;", self.root)
        self.assertIn("return ReviewLocalDirectory(directory);", self.root)
        self.assertIn("GENERIC_WRITE, 0, NULL, CREATE_NEW", self.root)
        self.assertIn("GENERIC_WRITE, 0, NULL, CREATE_NEW", self.report)
        self.assertNotIn("CREATE_ALWAYS", self.root + self.capture + self.report)
        self.assertNotIn("OPEN_ALWAYS", self.root + self.capture + self.report)
        self.assertEqual(re.findall(r"DeleteFileW\((\w+)\)", self.capture),
                         ["request", "request", "request", "binary", "request"])
        self.assertNotIn("DeleteFileW", self.ui + self.report + self.root)

    def test_only_explicitly_checked_windows_enter_reports(self):
        self.assertIn("ListView_SetCheckState(g_list, row, FALSE)", self.ui)
        self.assertIn("if (ListView_GetCheckState(g_list, row)) g_run.selected[g_run.count++] = g_windows[row];", self.ui)
        self.assertIn("Button_GetCheck(g_other) != BST_CHECKED", self.ui)
        self.assertIn("pid == GetCurrentProcessId()", self.ui)
        self.assertNotIn("g_windows", self.report)
        self.assertIn("i < run->count", self.report)
        self.assertIn("ReviewEnable(1)", self.ui)
        self.assertIn("if (g_busy) return;", self.ui)

    def test_reports_are_inert_local_and_never_acceptance(self):
        for literal in ('\\"owner_acceptance\\": false', '\\"whole_desktop_capture\\": false',
                        '\\"visual_acceptance\\": \\"NOT_EVALUATED\\"',
                        '\\"functional_acceptance\\": \\"NOT_EVALUATED\\"',
                        '\\"target_input_or_settings_requests\\": false', '\\"automatic_upload\\": false'):
            self.assertIn(literal, self.report)
        self.assertIn("ReviewHtmlText(out, r->window.title)", self.report)
        self.assertIn("ReviewJsonText(out, r->window.title)", self.report)
        self.assertIn("default-src 'none'; img-src data:", self.report)
        for forbidden in ("<script", "<iframe", "http://", "https://", "fetch(", "XMLHttpRequest"):
            self.assertNotIn(forbidden, self.report)
        self.assertIn("r->status == REVIEW_IMAGE", self.report)
        self.assertIn("ReviewPngValid(png, &g_run.results[i])", self.report)
        self.assertIn("g_run.results[i].status = REVIEW_IO_ERROR;", self.report)
        self.assertIn("No desktop/clipboard fallback", self.report)

    def test_failed_or_unscheduled_attempts_do_not_fabricate_time_or_affinity(self):
        worker = self.capture[self.capture.index("static void ReviewRunWorker("):]
        self.assertLess(worker.index("GetSystemTime(&result->utc)"), worker.index("CreateProcessW"))
        self.assertIn("if (affinity_known) *affinity_known = known != FALSE;", self.capture)
        self.assertIn("if (affinity_value) *affinity_value = known ? affinity : 0;", self.capture)
        self.assertIn("&result->affinity_query_known, &result->affinity", self.capture)
        self.assertIn('capture_attempt_utc', self.report)
        self.assertIn('if (r->utc.wYear)', self.report)
        self.assertIn('else fputs("null", out); /* Cancelled before scheduling', self.report)
        self.assertNotIn('capture_started_utc', self.report)

    def test_png_encoding_uses_opaque_bgra_and_memory_before_exclusive_write(self):
        self.assertIn("GUID_WICPixelFormat32bppBGRA", self.capture)
        self.assertIn("pixels[i] |= 0xFF000000u", self.capture)
        self.assertIn("CreateStreamOnHGlobal(NULL, TRUE, &stream)", self.capture)
        self.assertIn("stat.cbSize.QuadPart > REVIEW_MAX_PNG", self.capture)
        self.assertIn("ReviewWriteNew(path, data, stat.cbSize.LowPart)", self.capture)
        self.assertIn("size.QuadPart == result->png_bytes", self.report)
        self.assertIn('memcmp(header + 12, "IHDR", 4)', self.report)

    def test_manifest_and_emblem_preserve_suite_and_no_elevation(self):
        manifest = ET.parse(ROOT / "app-review/src/manifest.xml")
        privilege = manifest.find(".//{urn:schemas-microsoft-com:asm.v3}requestedExecutionLevel")
        self.assertEqual(privilege.attrib, {"level": "asInvoker", "uiAccess": "false"})
        self.assertIn("PerMonitorV2", (ROOT / "app-review/src/manifest.xml").read_text())
        self.assertIn('#include "../../shared/win32/suite_emblem.c"', self.root)
        self.assertIn("SuiteEmblemDibPixel(RGB(24, 27, 36))", self.ui)
        self.assertIn("SuiteEmblemRelease(&g_emblem)", self.ui)
        self.assertIn(ROOT / "shared/win32/suite_emblem.c", builder.TARGET_SHARED_SOURCES["appreview"])
        self.assertIn(ROOT / "project-hub/hub.ico", builder.TARGET_SHARED_SOURCES["appreview"])

    def test_builder_returns_nonzero_for_any_failed_target(self):
        for failure in (None, "appreview", "hub"):
            with self.subTest(failure=failure), mock.patch.object(builder, "ensure_zig_python", return_value=sys.executable), \
                    mock.patch.object(builder, "zig_version", return_value="fixture"), \
                    mock.patch.object(sys, "argv", ["build_windows.py"]), redirect_stdout(io.StringIO()):
                def build(_python, _env, name, *_args):
                    return "FAIL fixture" if name == failure else "ok fixture"
                with mock.patch.object(builder, "build_target", side_effect=build) as called:
                    self.assertEqual(builder.main(), int(failure is not None))
                self.assertEqual(called.call_count, 8)

    def test_header_resource_and_source_invalidate_helper_cache(self):
        with tempfile.TemporaryDirectory(prefix="app-review-key-") as temp:
            directory = Path(temp)
            header = directory / "policy.h"
            header.write_bytes(b"read only")
            with mock.patch.object(builder, "TARGET_SHARED_SOURCES", {}):
                before = builder.target_key("appreview", directory, [], "fixture")
                header.write_bytes(b"changed safety")
                self.assertNotEqual(before, builder.target_key("appreview", directory, [], "fixture"))


class AppReviewNativeProtocolTests(unittest.TestCase):
    def valid_result(self):
        return dict(schema="tests.app-review-native.v1", checks=50, images=1, target_input_count=0,
                    owner_acceptance=False, **{name: True for name in native.TRUE_FIELDS})

    def test_native_schema_accepts_only_complete_synthetic_evidence(self):
        native.validate_native_result(self.valid_result(), 0)
        for key in native.TRUE_FIELDS:
            for value in (False, 1, "true", None):
                data = self.valid_result(); data[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    native.validate_native_result(data, 0)
        self.assertIn("not installed-app", native.BOUNDARY)

    def test_missing_counts_nonzero_input_or_exit_cannot_pass(self):
        for key, value in (("checks", 24), ("checks", True), ("images", 0), ("images", "1"),
                           ("target_input_count", 1), ("target_input_count", False),
                           ("owner_acceptance", True), ("owner_acceptance", 0), ("schema", "other")):
            data = self.valid_result(); data[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                native.validate_native_result(data, 0)
        for key in self.valid_result():
            data = self.valid_result(); del data[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                native.validate_native_result(data, 0)
        for data, code in (({}, 0), (None, 0), (self.valid_result(), 1)):
            with self.assertRaises(ValueError): native.validate_native_result(data, code)

    def test_non_windows_never_compiles_executes_or_creates_output(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "never-created"
            with mock.patch.object(native.os, "name", "posix"), mock.patch.object(native, "run_command") as run:
                with self.assertRaisesRegex(ValueError, "cross-build is not a native pass"):
                    native.run_review(str(directory))
            run.assert_not_called()
            self.assertFalse(directory.exists())

    def test_fixture_process_failure_and_timeout_are_logged_never_passed(self):
        with tempfile.TemporaryDirectory(prefix="app-review-runner-") as temp:
            directory = Path(temp)
            process = mock.Mock(returncode=9)
            process.communicate.return_value = (b"out", b"err")
            with mock.patch.object(native.subprocess, "Popen", return_value=process) as start:
                self.assertEqual(native.run_command(["fixture"], directory, directory / "failed.log", 2), 9)
                self.assertEqual(start.call_args.kwargs["cwd"], directory)
            self.assertEqual((directory / "failed.log").read_bytes(), b"outerr")
            process = mock.Mock()
            process.communicate.side_effect = [subprocess.TimeoutExpired(["fixture"], 2), (b"partial", b"error")]
            process.poll.return_value = None
            with mock.patch.object(native.subprocess, "Popen", return_value=process), mock.patch.object(native.os, "name", "posix"):
                with self.assertRaisesRegex(ValueError, "timed out; not a pass"):
                    native.run_command(["fixture"], directory, directory / "timeout.log", 2)
            process.kill.assert_called_once()
            self.assertIn(b"TIMEOUT: not a pass", (directory / "timeout.log").read_bytes())

    def fixture_report(self, directory):
        # Schema-only synthetic bytes; the Windows C fixture independently WIC-decodes its real PNG.
        image = b"\x89PNG\r\n\x1a\n" + b"\0\0\0\rIHDR" + struct.pack(">II", 420, 260)
        window = dict(status="IMAGE_UNVERIFIED", **{"class": "AppReview_NativeFixture"},
                      ping_answered=True, placement_unchanged=True, capture_dimensions=[420, 260],
                      png="window-01.png", png_bytes=len(image))
        report = dict(schema="tests.app-review.v1", synthetic_fixture=True, owner_acceptance=False,
                      whole_desktop_capture=False, automatic_upload=False, target_input_or_settings_requests=False,
                      capture_kind="os_composited_window", visual_acceptance="NOT_EVALUATED",
                      functional_acceptance="NOT_EVALUATED", windows=[window])
        (directory / "window-01.png").write_bytes(image)
        (directory / "fixture-report.json").write_text(json.dumps(report))
        (directory / "fixture-report.html").write_text("default-src 'none'; img-src data: SYNTHETIC NATIVE FIXTURE REPORT " +
            "data:image/png;base64," + base64.b64encode(image).decode())
        return report

    def test_actual_native_report_protocol_requires_selected_fixture_and_embedded_image(self):
        with tempfile.TemporaryDirectory(prefix="app-review-manifest-") as temp:
            directory = Path(temp)
            report = self.fixture_report(directory)
            manifest = native.validate_fixture_report(directory)
            self.assertEqual(set(manifest), {"fixture-report.json", "fixture-report.html", "window-01.png"})
            self.assertEqual(manifest["window-01.png"]["bytes"], 24)
            for key, value in (("owner_acceptance", True), ("synthetic_fixture", False),
                               ("automatic_upload", True), ("whole_desktop_capture", True),
                               ("functional_acceptance", "PASS")):
                data = dict(report, **{key: value})
                (directory / "fixture-report.json").write_text(json.dumps(data))
                with self.subTest(key=key), self.assertRaises(ValueError): native.validate_fixture_report(directory)
            (directory / "fixture-report.json").write_text(json.dumps(report))
            (directory / "fixture-report.html").write_text("<script>wrong report</script>" * 3)
            with self.assertRaises(ValueError): native.validate_fixture_report(directory)

    def test_missing_extra_or_mismatched_native_artifacts_never_count_as_images(self):
        with tempfile.TemporaryDirectory(prefix="app-review-artifacts-") as temp:
            directory = Path(temp)
            with self.assertRaises(ValueError): native.validate_fixture_report(directory)
            report = self.fixture_report(directory)
            report["windows"].append(dict(report["windows"][0]))
            (directory / "fixture-report.json").write_text(json.dumps(report))
            with self.assertRaises(ValueError): native.validate_fixture_report(directory)
            self.fixture_report(directory)
            (directory / "window-01.png").write_bytes(b"not the embedded PNG" * 3)
            with self.assertRaises(ValueError): native.validate_fixture_report(directory)

    def test_native_harness_tests_real_capture_and_settings_sentinel_not_owner_apps(self):
        source = (ROOT / "tests/native/app_review_windows.c").read_text()
        self.assertIn('#include "../../app-review/src/app_review.c"', source)
        for text in ("FixtureDecodePng(png)", "result.status == REVIEW_TIMEOUT", "result.status == REVIEW_PROTECTED",
                     "result.status == REVIEW_MINIMIZED", "result.status == REVIEW_BLANK", "result.status == REVIEW_IDENTITY",
                     "InterlockedCompareExchange(&fixture_setting, 0, 0) == 73", "memcmp(unchanged, intended",
                     "!ReviewJsonReport(&run) && !ReviewHtmlReport(&run)", "inputs_before", "ReviewWorkerMain(argc, argv)"):
            self.assertIn(text, source)
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        # The Windows-native kit stays an explicit manual command. The
        # permission-refused CI addition is not rerouted through another step.
        self.assertNotIn("test_app_review_windows.py", workflow)
        self.assertNotIn("AppReview-native-", workflow)
        self.assertEqual(workflow, (ROOT / "docs/ci/ci.yml").read_text())
        runner = (ROOT / "tools/test_app_review_windows.py").read_text()
        self.assertIn('if os.name != "nt":', runner)


if __name__ == "__main__":
    unittest.main()
