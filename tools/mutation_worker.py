#!/usr/bin/env python3
"""One isolated unittest invocation with a machine-readable, fail-closed result.

Only the mutation runner sets the active source on the applicability check.
Clean pre/post runs still check every registry needle; a deliberately replaced
needle must never itself be counted as evidence that a behavior test caught it.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))


def test_name(test):
    name = test.id()
    match = re.fullmatch(r"(?:setUpClass|tearDownClass) \(([\w.]+)\)", name)
    return match.group(1) if match else name


class MeasuredResult(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.failure_ids = []
        self.error_ids = []
        self.infrastructure = []
        self.timings = []
        self.started = 0.0

    def startTest(self, test):
        self.started = time.perf_counter()
        super().startTest(test)

    def stopTest(self, test):
        self.timings.append({"test": test_name(test),
                             "seconds": time.perf_counter() - self.started})
        super().stopTest(test)

    def addFailure(self, test, err):
        self.failure_ids.append(test_name(test))
        super().addFailure(test, err)

    def record_error(self, test, err):
        self.error_ids.append(test_name(test))
        if issubclass(err[0], (ImportError, OSError, MemoryError, subprocess.SubprocessError)):
            self.infrastructure.append(test_name(test))

    def addError(self, test, err):
        self.record_error(test, err)
        super().addError(test, err)

    def addSubTest(self, test, subtest, err):
        # unittest does not call addFailure/addError for a failing subTest.
        # Keep the loadable parent selector, not an id containing parameters.
        if err is not None:
            if issubclass(err[0], test.failureException):
                self.failure_ids.append(test_name(test))
            else:
                self.record_error(test, err)
        super().addSubTest(test, subtest, err)


def run_tests(selectors, *, mutation_file=None, failfast=False):
    started = time.perf_counter()
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromNames(selectors)
    if loader.errors or suite.countTestCases() == 0:
        return {"status": "error", "tests_run": 0, "failures": [], "errors": [],
                "infrastructure": loader.errors or ["No tests selected"],
                "skipped": [], "timings": [], "seconds": time.perf_counter() - started}
    windows = sys.modules.get("tests.test_windows_apps")
    if windows is not None:
        windows.MUTATION_FILE = mutation_file
    result = unittest.TextTestRunner(stream=sys.stderr, verbosity=1, failfast=failfast,
                                     resultclass=MeasuredResult).run(suite)
    infrastructure = list(result.infrastructure)
    if result.expectedFailures or result.unexpectedSuccesses:
        infrastructure.append("Expected-failure/unexpected-success tests are not mutation evidence")
    if result.skipped:
        infrastructure += ["Skipped: " + test_name(test) + ": " + why
                           for test, why in result.skipped]
    status = "error" if infrastructure else ("pass" if result.wasSuccessful() else "detected")
    return {"status": status, "tests_run": result.testsRun,
            "failures": result.failure_ids, "errors": result.error_ids,
            "infrastructure": infrastructure,
            "skipped": [test_name(test) for test, _why in result.skipped],
            "timings": result.timings, "seconds": time.perf_counter() - started}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--mutation-file")
    parser.add_argument("--failfast", action="store_true")
    parser.add_argument("tests", nargs="+")
    args = parser.parse_args(argv)
    try:
        payload = run_tests(args.tests, mutation_file=args.mutation_file, failfast=args.failfast)
    except Exception as exc:
        payload = {"status": "error", "tests_run": 0, "failures": [], "errors": [],
                   "infrastructure": [f"{type(exc).__name__}: {exc}"], "skipped": [],
                   "timings": [], "seconds": 0.0}
    args.result.write_text(json.dumps(payload), encoding="utf-8")
    return {"pass": 0, "detected": 1, "error": 2}[payload["status"]]


if __name__ == "__main__":
    sys.exit(main())
