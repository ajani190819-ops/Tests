#!/usr/bin/env python3
"""Run every registered deliberate break, with measured, fail-closed detection.

Default: explicit per-mutation tests, fail-fast, then the full suite if an
unmapped/targeted mutation is not detected. Clean full suites run before/after.
--full-suite bypasses routing; --no-failfast supports a reference audit.
--check and --list do not mutate; --help/unknown options never start a sweep.
Reports live outside Git by default. No detection verdict is cached.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
REG = ROOT / "tools" / "mutations.json"
FULL_TESTS = ("tests.test_windows_apps", "tests.test_agent_notes", "tests.test_mutation_runner",
              "tests.test_review_tools", "tests.test_app_review", "tests.test_orca_settings_ui")
CACHE = Path.home() / ".cache" / "tests_mutate"


def registry_issues(entries, root):
    issues = []
    seen = set()
    if not isinstance(entries, list) or not entries:
        return [{"status": "error", "id": "registry", "reason": "Expected a nonempty registry list"}]
    for entry in entries:
        ident = entry.get("id", "?") if isinstance(entry, dict) else "?"
        try:
            if not all(isinstance(entry[key], str) and entry[key]
                       for key in ("id", "file", "old", "label")):
                raise ValueError("Missing/non-string registry field")
            if not isinstance(entry["new"], str) or entry["old"] == entry["new"]:
                raise ValueError("Mutation must change its source")
            if ident in seen:
                raise ValueError("Duplicate mutation id")
            seen.add(ident)
            relative = Path(entry["file"])
            path = (root / relative).resolve()
            if relative.is_absolute() or ".." in relative.parts or not path.is_relative_to(root.resolve()):
                raise ValueError("Mutation path escapes the source root")
            if "tests" in entry and (not isinstance(entry["tests"], list) or not entry["tests"]
                                      or not all(isinstance(test, str) and test.startswith("tests.")
                                                 for test in entry["tests"])):
                raise ValueError("tests must be a nonempty list of explicit unittest selectors")
            if entry["old"] not in path.read_bytes().decode("utf-8"):
                issues.append({"status": "stale", "id": ident, "reason": "Needle no longer matches"})
        except (KeyError, TypeError, ValueError, OSError) as exc:
            issues.append({"status": "error", "id": ident, "reason": str(exc)})
    return issues


def validate_result(payload, returncode):
    """A crash, missing selector, skip or missing JSON is never a mutation kill."""
    if not isinstance(payload, dict):
        raise ValueError("Worker result is not an object")
    status = payload.get("status")
    if status not in ("pass", "detected", "error"):
        raise ValueError("Unknown worker status")
    if returncode != {"pass": 0, "detected": 1, "error": 2}[status]:
        raise ValueError("Worker exit code disagrees with its result")
    count = payload.get("tests_run")
    if type(count) is not int or count < 0:
        raise ValueError("Invalid test count")
    for key in ("failures", "errors", "infrastructure", "skipped"):
        if not isinstance(payload.get(key), list):
            raise ValueError("Missing worker result field: " + key)
    if status != "error" and (payload["infrastructure"] or payload["skipped"]):
        raise ValueError("Infrastructure failure/skip is not a valid test verdict")
    if status == "pass" and (not count or payload["failures"] or payload["errors"]):
        raise ValueError("An empty/failing suite cannot pass")
    if status == "detected" and not (payload["failures"] or payload["errors"]):
        raise ValueError("Detection requires an actual failing test")
    return payload


def execute_tests(selectors, *, root, mutation_file=None, failfast=False, timeout=120):
    started = time.perf_counter()
    try:
        with tempfile.TemporaryDirectory(prefix="mutation-tests-") as name:
            folder = Path(name)
            result_path = folder / "result.json"
            command = [sys.executable, "-B", str(root / "tools" / "mutation_worker.py"),
                       "--result", str(result_path)]
            if mutation_file is not None:
                command += ["--mutation-file", mutation_file]
            if failfast:
                command.append("--failfast")
            command.extend(selectors)
            # -B prevents writes, not reads. A fresh prefix also prevents stale
            # timestamp/size .pyc hits after an equal-length Python mutation.
            env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1",
                       PYTHONPYCACHEPREFIX=str(folder / "pycache"))
            process = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True,
                                       start_new_session=os.name == "posix")
            try:
                stdout, stderr = process.communicate(timeout=timeout)
            except BaseException:
                try:
                    if os.name == "posix":
                        try:
                            os.killpg(process.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass  # Worker and its process group already exited.
                    else:
                        # Python's kill() alone leaves compiler/grandchild jobs.
                        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                                       capture_output=True, timeout=10)
                finally:
                    if process.poll() is None:
                        process.kill()
                    process.communicate()
                raise
            payload = validate_result(json.loads(result_path.read_text()), process.returncode)
            payload["returncode"] = process.returncode
            payload["diagnostic"] = (stdout + stderr)[-4000:] if payload["status"] == "error" else ""
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        payload = {"status": "error", "tests_run": 0, "failures": [], "errors": [],
                   "infrastructure": [f"{type(exc).__name__}: {exc}"], "skipped": []}
    payload["wall_seconds"] = time.perf_counter() - started
    return payload


@contextmanager
def campaign_lock(root):
    """Serialize writers to one checkout; different reference copies are safe."""
    CACHE.mkdir(parents=True, exist_ok=True)
    key = hashlib.sha256(str(root.resolve()).encode()).hexdigest()[:20]
    with (CACHE / (key + ".lock")).open("a+b") as lock:
        if os.name == "posix":
            import fcntl
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            try:
                yield
            finally:
                fcntl.flock(lock, fcntl.LOCK_UN)
        else:
            import msvcrt
            lock.write(b" "); lock.flush(); lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
            try:
                yield
            finally:
                lock.seek(0); msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


@contextmanager
def termination_guard():
    """Let a normal SIGTERM unwind source-restoration finally blocks."""
    previous = signal.getsignal(signal.SIGTERM)

    def terminate(_signum, _frame):
        raise KeyboardInterrupt("termination requested")

    signal.signal(signal.SIGTERM, terminate)
    try:
        yield
    finally:
        signal.signal(signal.SIGTERM, previous)


def run_campaign(entries, selected, *, root=ROOT, full_tests=FULL_TESTS,
                 full_suite=False, failfast=True, timeout=120, executor=None, progress=print):
    execute = executor or execute_tests
    started = time.perf_counter()
    report = {"schema": 1, "registry_count": len(entries), "selected_count": len(selected),
              "mode": "full" if full_suite else "routed", "failfast": failfast,
              "results": [], "issues": [], "baseline": None, "postflight": None,
              "restored": True, "seconds": 0.0, "python": sys.version, "workers": 1,
              "verdict_cache": False, "artifact_cache": "disabled; harnesses compile normally"}
    try:
        with campaign_lock(root):
            # Validate and snapshot under the same lock as writes. A contender
            # must never save another campaign's temporarily mutated bytes.
            report["issues"] = registry_issues(entries, root)
            if report["issues"]:
                return report
            originals = {entry["file"]: (root / entry["file"]).read_bytes() for entry in entries}
            fingerprint = hashlib.sha256()
            for relative, data in sorted(originals.items()):
                fingerprint.update(relative.encode() + b"\0" + data + b"\0")
            report["mutated_sources_sha256"] = fingerprint.hexdigest()
            report["registry_sha256"] = hashlib.sha256(json.dumps(entries, sort_keys=True).encode()).hexdigest()
            report["baseline"] = execute(full_tests, root=root, timeout=timeout)
            if report["baseline"]["status"] != "pass":
                report["issues"].append({"status": "error", "id": "baseline", "reason": "Clean full suite failed"})
                return report
            try:
                for entry in selected:
                    tick = time.perf_counter()
                    path = root / entry["file"]
                    original = originals[entry["file"]]
                    if path.read_bytes() != original:
                        raise RuntimeError("Source changed during campaign: " + entry["file"])
                    item = {"id": entry["id"], "attempts": [], "status": "error", "seconds": 0.0}
                    report["results"].append(item)
                    report["restored"] = False
                    try:
                        path.write_bytes(original.decode("utf-8").replace(entry["old"], entry["new"], 1).encode("utf-8"))
                        targets = None if full_suite else entry.get("tests")
                        if targets:
                            outcome = execute(targets, root=root, mutation_file=entry["file"],
                                              failfast=failfast, timeout=timeout)
                            item["attempts"].append(dict(scope="targeted", selectors=targets, **outcome))
                        if not targets or outcome["status"] == "pass":
                            outcome = execute(full_tests, root=root, mutation_file=entry["file"],
                                              failfast=failfast, timeout=timeout)
                            item["attempts"].append(dict(scope="fallback" if targets else "full",
                                                         selectors=list(full_tests), **outcome))
                        item["status"] = {"pass": "survived", "detected": "detected", "error": "error"}[outcome["status"]]
                    finally:
                        try:
                            path.write_bytes(original)
                        finally:
                            item["seconds"] = time.perf_counter() - tick
                    progress(f"{item['status'].upper():8} {entry['id']:<22} {item['seconds']:.3f}s {entry['label']}")
                    if item["status"] == "error":
                        break
            finally:
                # Even restoration/read failures must not leave restored=True.
                report["restored"] = False
                try:
                    report["restored"] = all((root / rel).read_bytes() == data for rel, data in originals.items())
                except OSError as exc:
                    report["issues"].append({"status": "error", "id": "restoration", "reason": str(exc)})
                if not report["restored"]:
                    report["issues"].append({"status": "error", "id": "restoration", "reason": "Source bytes not restored"})
                report["postflight"] = execute(full_tests, root=root, timeout=timeout)
                if report["postflight"]["status"] != "pass":
                    report["issues"].append({"status": "error", "id": "postflight", "reason": "Restored full suite failed"})
    except (Exception, KeyboardInterrupt) as exc:
        report["issues"].append({"status": "error", "id": "campaign", "reason": f"{type(exc).__name__}: {exc}"})
    finally:
        report["seconds"] = time.perf_counter() - started
        report["counts"] = {status: sum(item["status"] == status for item in report["results"])
                            for status in ("detected", "survived", "error")}
        report["counts"].update(run=len(report["results"]),
                                stale=sum(issue["status"] == "stale" for issue in report["issues"]),
                                gate_errors=sum(issue["status"] == "error" for issue in report["issues"]))
    return report


def successful(report):
    return (not report["issues"] and report["restored"]
            and len(report["results"]) == report["selected_count"]
            and all(item["status"] == "detected" for item in report["results"])
            and all(report[key] and report[key]["status"] == "pass" for key in ("baseline", "postflight")))


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, prefix=path.name + ".", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            json.dump(report, stream, indent=2)
            stream.write("\n")
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="verify registry applicability without mutating")
    mode.add_argument("--list", action="store_true", help="list mutations without running tests")
    parser.add_argument("--full-suite", action="store_true", help="bypass per-mutation test routing")
    parser.add_argument("--no-failfast", action="store_true", help="reference audit: finish each broken suite")
    parser.add_argument("--timeout", type=float, default=120, help="seconds per test invocation")
    parser.add_argument("--report", type=Path, help="JSON timings/result path (default: external cache)")
    parser.add_argument("ids", nargs="*", help="optional exact mutation ids")
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be finite and positive")
    try:
        entries = json.loads(REG.read_text(encoding="utf-8"))
        if not isinstance(entries, list) or not entries:
            raise ValueError("Expected a nonempty registry list")
        unknown = set(args.ids) - {entry["id"] for entry in entries}
        if unknown:
            parser.error("unknown mutation ids: " + ", ".join(sorted(unknown)))
        selected = [entry for entry in entries if not args.ids or entry["id"] in args.ids]
        if args.list:
            for entry in selected:
                print(f"{entry['id']:<22} {entry['file']:<46} {entry['label']}")
            return 0
        if args.check:
            issues = registry_issues(entries, ROOT)
            for issue in issues:
                print(f"{issue['status'].upper()} {issue['id']}: {issue['reason']}")
            print(f"{len(entries)} entries, {sum(x['status'] == 'stale' for x in issues)} stale, "
                  f"{sum(x['status'] == 'error' for x in issues)} errors")
            return int(bool(issues))
        with termination_guard():
            report = run_campaign(entries, selected, root=ROOT, full_suite=args.full_suite,
                                  failfast=not args.no_failfast, timeout=args.timeout)
        key = hashlib.sha256(str(ROOT.resolve()).encode()).hexdigest()[:20]
        destination = args.report or CACHE / (key + "-latest.json")
        write_report(destination, report)
        counts = report["counts"]
        print(f"FINAL {len(report['results'])}/{len(selected)} run, {counts['detected']} detected, "
              f"{counts['survived']} SURVIVED, {counts['stale']} STALE, "
              f"{counts['error']} mutation errors, {counts['gate_errors']} gate/config errors; "
              f"restored={report['restored']}; clean suites="
              f"{all(report[k] and report[k]['status'] == 'pass' for k in ('baseline', 'postflight'))}; "
              f"{report['seconds']:.3f}s")
        for issue in report["issues"]:
            print(f"ERROR {issue['id']}: {issue['reason']}")
        print("JSON report:", destination)
        return 0 if successful(report) else 1
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"ERROR: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
