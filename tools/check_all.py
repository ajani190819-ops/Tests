#!/usr/bin/env python3
"""Every check in this repo, executed concurrently across CPU cores.

    python3 tools/check_all.py              # concurrent + SHA-256 Merkle cache for unchanged geometry suite
    python3 tools/check_all.py --no-cache   # force full uncached execution of every suite

Exit code is non-zero if anything failed or skipped, so it can gate a commit
and run directly in GitHub Actions CI (where CI=true always runs cold).
"""
import concurrent.futures
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
NODEPS = pathlib.Path("/tmp/nodeps")
EXTERNAL_VENV_PY = pathlib.Path("/home/user/.venv/bin/python")
CACHE_PATH = pathlib.Path("/home/user/.cache/tests_check_merkle.json")

TESTS = ["test_installer.py", "test_plugin_audit.py",
         "test_post_script.py", "test_unlayered_waves.py",
         "test_wave_gcode.py", "test_windows_apps.py"]
NODEPS_ONLY = ["test_plugin_runtime.py", "test_plugin_audit.py"]
TOOLS = [["tools/sync_engine.py", "--check"],
         ["tools/sync_changelog.py", "--check"],
         ["tools/dump_default_config.py", "--check"],
         ["tools/bump_version.py", "--check-docs"],
         ["tools/agent_map.py", "--check"]]


def have_deps():
    try:
        import numpy, shapely  # noqa: F401
        return True
    except ImportError:
        return False


def wave_gcode_merkle_key() -> str:
    """Exact cryptographic SHA-256 digest of every file & package version test_wave_gcode.py depends on."""
    import numpy
    import shapely
    h = hashlib.sha256()
    h.update(f"py={sys.version}|np={numpy.__version__}|shp={shapely.__version__}\n".encode())
    watched = sorted(
        list((ROOT / "plugins" / "wave-overhangs").rglob("*"))
        + list((ROOT / "tests" / "fixtures").rglob("*"))
        + [
            ROOT / "tests" / "test_wave_gcode.py",
            ROOT / "tests" / "wave_cases.py",
            ROOT / "tests" / "fake_orca.py",
        ]
    )
    for p in watched:
        if p.is_file() and "__pycache__" not in p.parts:
            h.update(str(p.relative_to(ROOT)).encode() + b"\0")
            h.update(p.read_bytes() + b"\0")
    return h.hexdigest()


def load_cache() -> dict:
    try:
        return json.loads(CACHE_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {}


def save_cache(data: dict) -> None:
    try:
        CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        CACHE_PATH.write_text(json.dumps(data), encoding="utf-8")
    except Exception:
        pass


def make_nodeps():
    """A PYTHONPATH that makes numpy and shapely unimportable."""
    NODEPS.mkdir(parents=True, exist_ok=True)
    for name in ("numpy.py", "shapely.py"):
        (NODEPS / name).write_text('raise ImportError("blocked by check_all")\n')
    env = dict(os.environ)
    env["PYTHONPATH"] = str(NODEPS) + os.pathsep + env.get("PYTHONPATH", "")
    return env


def exec_task(label, cmd, cwd=ROOT, env=None):
    with tempfile.TemporaryDirectory() as tmp_storage:
        run_env = dict(os.environ if env is None else env)
        run_env.setdefault("ORCA_PLUGIN_STORAGE_DIR", tmp_storage)
        result = subprocess.run(cmd, cwd=cwd, env=run_env,
                                capture_output=True, text=True)
    body = (result.stdout or "") + (result.stderr or "")
    # Both harnesses exit non-zero on failure (report_and_exit -> sys.exit(1),
    # unittest -> exit 1), so the return code is the real signal. The text
    # match is a safety net for a harness that forgets to exit non-zero; it is
    # anchored to a line START because a bare "FAILED" substring also matches
    # a passing test that merely quotes a failure message - which these suites
    # do, since several assertions print the word in their own diagnostics.
    failed = (result.returncode != 0
              or re.search(r"(?m)^FAILED\b", body) is not None)
    skipped = body.lstrip().startswith("SKIP")
    mark = "FAIL" if failed else ("SKIP" if skipped else "ok  ")
    last = [ln for ln in body.strip().splitlines() if ln.strip()]
    lines = [f"  {mark}  {label}"]
    if failed:
        for line in last[-12:]:
            lines.append(f"          {line[:160]}")
    return failed, skipped, lines


def main():
    if not have_deps() and EXTERNAL_VENV_PY.exists() and pathlib.Path(sys.executable) != EXTERNAL_VENV_PY:
        os.execv(str(EXTERNAL_VENV_PY), [str(EXTERNAL_VENV_PY), __file__] + sys.argv[1:])

    use_cache = ("--no-cache" not in sys.argv) and (os.environ.get("CI", "").lower() != "true")
    deps = have_deps()
    cache = load_cache() if (use_cache and deps) else {}
    wave_key = wave_gcode_merkle_key() if deps else ""

    nodeps_env = make_nodeps()

    max_workers = max(4, min(16, (os.cpu_count() or 4) * 2))
    with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as pool:
        dep_futs = []
        for test in TESTS:
            if test == "test_wave_gcode.py" and wave_key and cache.get("wave_gcode") == wave_key:
                dep_futs.append((test, None))
            else:
                dep_futs.append(
                    (test, pool.submit(exec_task, test, [sys.executable, f"tests/{test}"], ROOT, None))
                )
        nodep_futs = [
            (test, pool.submit(exec_task, test, [sys.executable, f"tests/{test}"], ROOT, nodeps_env))
            for test in NODEPS_ONLY
        ]
        tool_futs = []
        for argv in TOOLS:
            label = argv[0].split("/")[-1] + (" " + " ".join(argv[1:]) if len(argv) > 1 else "")
            tool_futs.append(
                (label, pool.submit(exec_task, label, [sys.executable] + argv, ROOT, None))
            )
        bad = []
        skipped = []

        print("with numpy + shapely:" if deps else
              "numpy/shapely NOT INSTALLED -- geometry tests will skip:")
        for test, fut in dep_futs:
            if fut is None:
                print(f"  ok    {test} (SHA-256 unchanged)")
                continue
            failed, skip, lines = fut.result()
            for ln in lines:
                print(ln)
            if failed:
                bad.append(test)
            elif skip:
                skipped.append(test)
            elif test == "test_wave_gcode.py" and wave_key and use_cache:
                cache["wave_gcode"] = wave_key
                save_cache(cache)

        print("without numpy + shapely (the first-run dependency path):")
        for test, fut in nodep_futs:
            failed, _s, lines = fut.result()
            for ln in lines:
                print(ln)
            if failed:
                bad.append(test + " (no deps)")

        print("repository consistency:")
        for label, fut in tool_futs:
            failed, _s, lines = fut.result()
            for ln in lines:
                print(ln)
            if failed:
                bad.append(label)

    print()
    if bad:
        print(f"FAILED: {', '.join(bad)}")
        return 1
    if skipped:
        print(f"ok, but SKIPPED (a skip is not a pass): {', '.join(skipped)}")
        print("  pip install -r requirements-dev.txt")
        return 1
    print("ok -- everything passes in both dependency states")
    return 0


if __name__ == "__main__":
    sys.exit(main())
