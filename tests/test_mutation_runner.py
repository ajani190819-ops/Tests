#!/usr/bin/env python3
"""Runner safety and protocol tests. Synthetic mutations never touch app files."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import py_compile
import shutil
import signal
import subprocess
import time
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools import mutate


def verdict(status):
    return {"status": status, "tests_run": 1, "seconds": 0.0,
            "failures": ["tests.example.Case.test_behavior"] if status == "detected" else [],
            "errors": [], "skipped": [],
            "infrastructure": ["test infrastructure unavailable"] if status == "error" else []}


class MutationRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mutation-runner-unit-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "subject.py"
        self.original = b"value = 1\r\n"
        self.source.write_bytes(self.original)
        self.entry = {"id": "example-01", "file": "subject.py", "old": "value = 1",
                      "new": "value = 2", "label": "a real behavioral break",
                      "tests": ["tests.example.Case.test_behavior"]}
        self.calls = []

    def campaign(self, outcomes=("pass", "detected", "pass"), entries=None, selected=None, **kwargs):
        queue = list(outcomes)

        def execute(selectors, **options):
            self.calls.append((tuple(selectors), options, self.source.read_bytes()))
            outcome = queue.pop(0)
            if isinstance(outcome, BaseException):
                raise outcome
            return verdict(outcome)

        entries = entries or [self.entry]
        return mutate.run_campaign(entries, selected or entries, root=self.root,
                                   full_tests=("tests.full",), executor=execute,
                                   progress=lambda _line: None, **kwargs)

    def test_targeted_detection_keeps_clean_gates_and_exact_source_bytes(self):
        report = self.campaign()
        self.assertTrue(mutate.successful(report))
        self.assertEqual(self.source.read_bytes(), self.original)
        self.assertEqual([call[0] for call in self.calls],
                         [("tests.full",), tuple(self.entry["tests"]), ("tests.full",)])
        self.assertEqual(self.calls[1][2], b"value = 2\r\n")
        self.assertEqual(self.calls[1][1]["mutation_file"], "subject.py")
        self.assertTrue(self.calls[1][1]["failfast"])
        self.assertNotIn("mutation_file", self.calls[0][1])
        self.assertNotIn("mutation_file", self.calls[-1][1])

    def test_passing_targeted_selection_must_run_full_fallback(self):
        report = self.campaign(("pass", "pass", "detected", "pass"))
        self.assertTrue(mutate.successful(report))
        self.assertEqual([item["scope"] for item in report["results"][0]["attempts"]],
                         ["targeted", "fallback"])
        self.assertEqual(self.calls[2][0], ("tests.full",))

    def test_unmapped_and_explicit_full_modes_cannot_skip_a_mutation(self):
        for full in (False, True):
            with self.subTest(full=full):
                entry = dict(self.entry)
                if not full:
                    del entry["tests"]
                self.calls.clear()
                report = self.campaign(entries=[entry], full_suite=full)
                self.assertTrue(mutate.successful(report))
                self.assertEqual(self.calls[1][0], ("tests.full",))
                self.assertEqual(report["results"][0]["attempts"][0]["scope"], "full")

    def test_full_fallback_pass_is_a_surviving_mutation_not_detection(self):
        report = self.campaign(("pass", "pass", "pass", "pass"))
        self.assertFalse(mutate.successful(report))
        self.assertEqual(report["results"][0]["status"], "survived")
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_bad_target_is_an_error_never_a_fallback_detection(self):
        report = self.campaign(("pass", "error", "pass"))
        self.assertFalse(mutate.successful(report))
        self.assertEqual(report["results"][0]["status"], "error")
        self.assertEqual(len(report["results"][0]["attempts"]), 1)
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_broken_clean_baseline_aborts_before_writing(self):
        report = self.campaign(("detected",))
        self.assertFalse(mutate.successful(report))
        self.assertEqual(report["results"], [])
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_broken_restored_suite_cannot_publish_a_green_campaign(self):
        report = self.campaign(("pass", "detected", "error"))
        self.assertFalse(mutate.successful(report))
        self.assertEqual(report["issues"][0]["id"], "postflight")

    def test_exception_and_interruption_restore_before_postflight(self):
        for exception in (OSError("runner crashed"), KeyboardInterrupt()):
            with self.subTest(exception=type(exception).__name__):
                self.calls.clear()
                report = self.campaign(("pass", exception, "pass"))
                self.assertFalse(mutate.successful(report))
                self.assertEqual(self.source.read_bytes(), self.original)
                self.assertTrue(report["restored"])
                self.assertEqual(self.calls[-1][2], self.original)
                self.assertEqual(report["issues"][-1]["id"], "campaign")

    def test_stale_registry_never_runs_tests_or_writes_sources(self):
        self.entry["old"] = "nonexistent needle"
        report = self.campaign(())
        self.assertFalse(mutate.successful(report))
        self.assertEqual(report["issues"][0]["status"], "stale")
        self.assertEqual(self.calls, [])
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_invalid_registry_paths_ids_routes_and_noops_fail_closed(self):
        invalid = [dict(self.entry, file="../escape.py"), dict(self.entry, file=str(self.source)),
                   dict(self.entry, tests=[]), dict(self.entry, tests=["not_a_test"]),
                   dict(self.entry, new=self.entry["old"])]
        for entry in invalid:
            with self.subTest(entry=entry):
                self.assertTrue(mutate.registry_issues([entry], self.root))
        self.assertTrue(mutate.registry_issues([self.entry, self.entry], self.root))
        self.assertTrue(mutate.registry_issues([], self.root))
        self.assertTrue(mutate.registry_issues([None], self.root))

    def test_two_campaigns_cannot_write_the_same_checkout(self):
        with mutate.campaign_lock(self.root):
            with self.assertRaises(OSError):
                with mutate.campaign_lock(self.root):
                    self.fail("a second writer acquired the checkout")
        with mutate.campaign_lock(self.root):
            pass

    def test_lock_precedes_registry_reads_and_source_snapshots(self):
        with mutate.campaign_lock(self.root), \
                mock.patch.object(mutate, "registry_issues") as validate:
            report = self.campaign(())
        validate.assert_not_called()
        self.assertEqual(self.calls, [])
        self.assertFalse(mutate.successful(report))
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_restoration_write_failure_cannot_report_restored_or_success(self):
        write = Path.write_bytes

        def fail_restore(path, data):
            if path == self.source and data == self.original:
                raise PermissionError("restoration write denied")
            return write(path, data)

        with mock.patch.object(Path, "write_bytes", fail_restore):
            report = self.campaign()
        self.assertFalse(report["restored"])
        self.assertFalse(mutate.successful(report))
        self.assertEqual(report["postflight"]["status"], "pass")
        self.assertIn("restoration", [issue["id"] for issue in report["issues"]])

    def test_report_records_only_selected_mutations_not_registry_size(self):
        second = dict(self.entry, id="example-02")
        report = self.campaign(entries=[self.entry, second], selected=[self.entry])
        self.assertTrue(mutate.successful(report))
        self.assertEqual(report["registry_count"], 2)
        self.assertEqual(report["selected_count"], 1)
        self.assertEqual(len(report["results"]), 1)
        destination = self.root / "reports" / "timings.json"
        mutate.write_report(destination, report)
        self.assertEqual(json.loads(destination.read_text()), report)
        self.assertEqual(list(destination.parent.iterdir()), [destination])

    def test_help_unknown_options_and_unknown_ids_do_not_start_tests(self):
        registry = self.root / "mutations.json"
        registry.write_text(json.dumps([self.entry]))
        with mock.patch.object(mutate, "ROOT", self.root), mock.patch.object(mutate, "REG", registry), \
                mock.patch.object(mutate, "execute_tests") as execute, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            for args, expected in [(["--help"], 0), (["--typo"], 2), (["bad-id"], 2),
                                   (["--timeout", "nan"], 2), (["--timeout", "inf"], 2)]:
                with self.subTest(args=args), self.assertRaises(SystemExit) as error:
                    mutate.main(args)
                self.assertEqual(error.exception.code, expected)
            self.assertEqual(mutate.main(["--check"]), 0)
            execute.assert_not_called()

    def test_sigterm_unwinds_source_restoration_and_reports_failure(self):
        script = r"""
import json, sys, time
from pathlib import Path
from tools import mutate
root = Path(sys.argv[1])
mutate.ROOT = root
mutate.REG = root / "registry.json"
def execute(selectors, **options):
    if options.get("mutation_file"):
        (root / "active").write_text("ready")
        time.sleep(30)
    return {"status":"pass", "tests_run":1, "failures":[], "errors":[],
            "infrastructure":[], "skipped":[]}
mutate.execute_tests = execute
sys.exit(mutate.main(["--report", str(root / "report.json")]))
"""
        (self.root / "registry.json").write_text(json.dumps([self.entry]))
        process = subprocess.Popen([sys.executable, "-c", script, str(self.root)],
                                   cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while not (self.root / "active").exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue((self.root / "active").exists(), "fixture never reached the active mutation")
            process.send_signal(signal.SIGTERM)
            stdout, stderr = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 1, stdout + stderr)
            self.assertEqual(self.source.read_bytes(), self.original)
            self.assertFalse(mutate.successful(json.loads((self.root / "report.json").read_text())))
        finally:
            if process.poll() is None:
                process.kill()
            process.communicate()

    def test_protocol_rejects_crashes_empty_passes_and_fake_detections(self):
        for payload, code in [(verdict("detected"), 0), (dict(verdict("pass"), tests_run=0), 0),
                              (dict(verdict("detected"), failures=[]), 1),
                              (dict(verdict("detected"), skipped=["a skipped test"]), 1),
                              (dict(verdict("detected"), infrastructure=["no compiler"]), 1),
                              (dict(verdict("pass"), tests_run=-1), 0),
                              (dict(verdict("pass"), tests_run="1"), 0),
                              ({"status": "pass"}, 0), ([], 1)]:
            with self.subTest(payload=payload):
                with self.assertRaises(ValueError):
                    mutate.validate_result(payload, code)

    def test_registered_test_owners_resolve_to_nonempty_suites(self):
        entries = json.loads((ROOT / "tools/mutations.json").read_text())
        loader = unittest.TestLoader()
        for selector in sorted({test for entry in entries for test in entry.get("tests", [])}):
            with self.subTest(selector=selector):
                self.assertGreater(loader.loadTestsFromName(selector).countTestCases(), 0)
                self.assertEqual(loader.errors, [])

    def test_applicability_check_cannot_be_a_false_behavioral_kill(self):
        from tests import test_windows_apps as windows
        entries = [{"id": str(i), "file": f"fixture{i}.txt", "old": "needle"} for i in range(19)]
        build = ("/home/user/.cache/winbuild --force rglob TARGET_SHARED_SOURCES "
                 "target_key(name, rc_dir, libs, zig_ver) out_exe.exists()")
        files = {"tools/build_windows.py": build, "tools/mutate.py": "SURVIVED STALE --list",
                 "tools/mutations.json": json.dumps(entries),
                 "project-hub/SuiteReport.bat": b"report\r\n",
                 "project-hub/SuiteReport.ps1": "SuiteReport.txt .agent/RELEASED engine.log Get-Process -Name"}
        files.update({entry["file"]: "needle" for entry in entries})
        files["fixture0.txt"] = "deliberate replacement"

        class FakePath:
            def __init__(self, name=""):
                self.name = name

            def __truediv__(self, relative):
                return FakePath((self.name + "/" if self.name else "") + str(relative))

            def read_text(self, **_kwargs):
                return files[self.name]

            def read_bytes(self):
                return files[self.name]

            def exists(self):
                return self.name in files

        case = windows.SuiteFolderInstallTests("test_tooling_cache_registry_and_report")
        with mock.patch.object(windows, "ROOT", FakePath()):
            with mock.patch.object(windows, "MUTATION_FILE", None):
                with self.assertRaisesRegex(AssertionError, "drifted"):
                    case.test_tooling_cache_registry_and_report()
            with mock.patch.object(windows, "MUTATION_FILE", "fixture0.txt"):
                case.test_tooling_cache_registry_and_report()
                files["fixture1.txt"] = "an unrelated broken file"
                with self.assertRaisesRegex(AssertionError, "drifted"):
                    case.test_tooling_cache_registry_and_report()


class MutationWorkerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mutation-worker-unit-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools").mkdir()
        shutil.copyfile(ROOT / "tools" / "mutation_worker.py", self.root / "tools" / "mutation_worker.py")
        (self.root / "tests").mkdir()
        (self.root / "tests" / "__init__.py").write_text("")
        self.fixture = self.root / "tests" / "fixture.py"
        self.fixture.write_text('''import time, unittest, subprocess, sys
from pathlib import Path
VALUE = 1
class Cases(unittest.TestCase):
    def test_pass(self): self.assertEqual(VALUE, 1)
    def test_fail(self): self.fail("behavioral regression")
    def test_skip(self): self.skipTest("compiler unavailable")
    def test_infra(self): raise FileNotFoundError("compiler disappeared")
    def test_component_error(self): raise ValueError("invalid component state")
    def test_inner_timeout(self): raise subprocess.TimeoutExpired("compiler", 1)
    def test_compiler_error(self): raise subprocess.CalledProcessError(2, "compiler")
    def test_subtest_failure(self):
        with self.subTest(value=1): self.fail("subtest behavior")
    def test_subtest_component_error(self):
        with self.subTest(value=1): raise ValueError("subtest state")
    def test_subtest_infra(self):
        with self.subTest(value=1): raise FileNotFoundError("missing tool")
    def test_descendant_timeout(self):
        subprocess.Popen([sys.executable, "-c", "import time; time.sleep(5)"])
        Path(__file__).with_suffix(".child").write_text("started")
        time.sleep(5)
    def test_timeout(self): time.sleep(5)
''')

    def execute(self, *names, **kwargs):
        return mutate.execute_tests(["tests.fixture.Cases." + name for name in names],
                                    root=self.root, **kwargs)

    def test_real_worker_pass_failure_and_component_error(self):
        self.assertEqual(self.execute("test_pass")["status"], "pass")
        for name in ("test_fail", "test_component_error", "test_subtest_failure", "test_subtest_component_error"):
            with self.subTest(name=name):
                result = self.execute(name)
                self.assertEqual(result["status"], "detected")
                self.assertEqual(result["failures"] + result["errors"], ["tests.fixture.Cases." + name])

    def test_missing_tests_skips_and_environment_errors_are_not_detections(self):
        for name in ("nonexistent", "test_skip", "test_infra", "test_inner_timeout",
                     "test_compiler_error", "test_subtest_infra"):
            with self.subTest(name=name):
                self.assertEqual(self.execute(name)["status"], "error")
        (self.root / "tests" / "empty.py").write_text("")
        result = mutate.execute_tests(["tests.empty"], root=self.root)
        self.assertEqual(result["status"], "error")

    def test_failfast_stops_after_a_real_test_failure(self):
        fast = self.execute("test_fail", "test_pass", failfast=True)
        full = self.execute("test_fail", "test_pass", failfast=False)
        self.assertEqual(fast["tests_run"], 1)
        self.assertEqual(full["tests_run"], 2)

    def test_stale_equal_size_same_timestamp_bytecode_is_never_reused(self):
        fixed_time = 1700000000
        os.utime(self.fixture, (fixed_time, fixed_time))
        py_compile.compile(str(self.fixture), doraise=True)
        self.fixture.write_text(self.fixture.read_text().replace("VALUE = 1", "VALUE = 2"))
        os.utime(self.fixture, (fixed_time, fixed_time))
        self.assertEqual(self.execute("test_pass")["status"], "detected")

    def test_timed_out_worker_is_an_error_not_detection(self):
        result = self.execute("test_timeout", timeout=0.2)
        self.assertEqual(result["status"], "error")
        self.assertIn("TimeoutExpired", str(result["infrastructure"]))

    def test_timeout_terminates_descendants_not_only_the_test_worker(self):
        result = self.execute("test_descendant_timeout", timeout=0.5)
        self.assertTrue(self.fixture.with_suffix(".child").exists(), "worker did not start its child")
        self.assertEqual(result["status"], "error")
        self.assertLess(result["wall_seconds"], 2, "descendant retained the output pipe after timeout")

    def test_worker_crash_without_a_result_is_not_detection(self):
        (self.root / "tools" / "mutation_worker.py").write_text("raise SystemExit(1)\n")
        self.assertEqual(self.execute("test_pass")["status"], "error")


if __name__ == "__main__":
    unittest.main()
