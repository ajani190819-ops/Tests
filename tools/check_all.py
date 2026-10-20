#!/usr/bin/env python3
"""Every check in this repo, in both dependency states, one line each.

    python3 tools/check_all.py

Why this exists: verifying a change used to mean running six test scripts and
three sync tools by hand, remembering that test_wave_gcode SKIPS without
shapely (a skip is not a pass) and that test_plugin_runtime has a path that
only runs WITHOUT it. Doing that by hand is slow and easy to get wrong.

Exit code is non-zero if anything failed or skipped, so it can gate a commit
and run directly in GitHub Actions CI.
"""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
NODEPS = pathlib.Path("/tmp/nodeps")
ARENA_LINK = ROOT / "arena-link-windows"

TESTS = ["test_installer.py", "test_plugin_audit.py",
         "test_post_script.py", "test_unlayered_waves.py", "test_wave_gcode.py"]
# test_plugin_runtime.py asserts what the plugins do when numpy and shapely
# are MISSING (the first-run dependency path), so it is only run in the
# no-deps pass below. Running it with the deps installed reports failures
# that mean nothing -- which is exactly the confusion this script removes.
NODEPS_ONLY = ["test_plugin_runtime.py", "test_plugin_audit.py"]
TOOLS = [["tools/sync_engine.py", "--check"],
         ["tools/sync_changelog.py", "--check"],
         ["tools/dump_default_config.py", "--check"],
         ["tools/bump_version.py", "--check-docs"]]


def have_deps():
    try:
        import numpy, shapely  # noqa: F401
        return True
    except ImportError:
        return False


def make_nodeps():
    """A PYTHONPATH that makes numpy and shapely unimportable."""
    NODEPS.mkdir(parents=True, exist_ok=True)
    for name in ("numpy.py", "shapely.py"):
        (NODEPS / name).write_text('raise ImportError("blocked by check_all")\n')
    env = dict(os.environ)
    env["PYTHONPATH"] = str(NODEPS) + os.pathsep + env.get("PYTHONPATH", "")
    return env


def run_cmd(label, cmd, cwd=ROOT, env=None):
    with tempfile.TemporaryDirectory() as tmp_storage:
        run_env = dict(os.environ if env is None else env)
        run_env.setdefault("ORCA_PLUGIN_STORAGE_DIR", tmp_storage)
        result = subprocess.run(cmd, cwd=cwd, env=run_env,
                                capture_output=True, text=True)
    body = (result.stdout or "") + (result.stderr or "")
    failed = result.returncode != 0 or "FAILED" in body
    skipped = body.lstrip().startswith("SKIP")
    mark = "FAIL" if failed else ("SKIP" if skipped else "ok  ")
    last = [ln for ln in body.strip().splitlines() if ln.strip()]
    print(f"  {mark}  {label}")
    if failed:
        for line in last[-12:]:
            print(f"          {line[:160]}")
    return failed, skipped


def run(label, argv, env=None):
    return run_cmd(label, [sys.executable] + argv, cwd=ROOT, env=env)


def main():
    deps = have_deps()
    bad = []
    skipped = []
    print("with numpy + shapely:" if deps else
          "numpy/shapely NOT INSTALLED -- geometry tests will skip:")
    for test in TESTS:
        failed, skip = run(test, [f"tests/{test}"])
        if failed:
            bad.append(test)
        if skip:
            skipped.append(test)

    print("without numpy + shapely (the first-run dependency path):")
    env = make_nodeps()
    for test in NODEPS_ONLY:
        failed, _s = run(test, [f"tests/{test}"], env=env)
        if failed:
            bad.append(test + " (no deps)")

    print("repository consistency:")
    for argv in TOOLS:
        label = argv[0].split("/")[-1] + (" " + " ".join(argv[1:]) if len(argv) > 1 else "")
        failed, _s = run(label, argv)
        if failed:
            bad.append(label)

    if ARENA_LINK.exists() and shutil.which("node"):
        print("arena-link-windows:")
        failed, _s = run_cmd("npm test", ["npm", "test"], cwd=ARENA_LINK)
        if failed:
            bad.append("arena-link-windows (npm test)")

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
