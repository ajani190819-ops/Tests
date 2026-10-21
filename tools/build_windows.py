#!/usr/bin/env python3
"""Parallel zero-discovery builder for ProjectHub.exe and BluetoothGuard.exe.

Usage:
    python3 tools/build_windows.py

Automatically uses /home/user/.venv (bootstrapping ziglang outside the repo if
needed) and caches compiler artifacts in /home/user/.cache/zig so workspace
snapshots stay lean and rebuilds complete in ~1.5 seconds.
"""
import concurrent.futures
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
VENV_DIR = pathlib.Path("/home/user/.venv")
VENV_PY = VENV_DIR / "bin" / "python"


def ensure_zig_python() -> str:
    for cand in (str(VENV_PY), sys.executable):
        if pathlib.Path(cand).exists():
            rc = subprocess.run([cand, "-c", "import ziglang"], capture_output=True)
            if rc.returncode == 0:
                return cand
    if not VENV_PY.exists():
        subprocess.run([sys.executable, "-m", "venv", str(VENV_DIR)], check=True)
    subprocess.run(
        [str(VENV_PY), "-m", "pip", "install", "--quiet",
         "-r", str(ROOT / "requirements-dev.txt"), "ziglang", "Pillow"],
        check=True,
    )
    return str(VENV_PY)


def build_target(zig_py: str, env: dict, name: str, rc_dir: pathlib.Path,
                 rc_file: str, c_file: pathlib.Path, out_exe: pathlib.Path,
                 libs: list[str]) -> str:
    tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix=f"winbuild-{name}-"))
    try:
        res_path = tmp_dir / f"{name}.res"
        subprocess.run(
            [zig_py, "-m", "ziglang", "rc", "/fo", str(res_path), rc_file],
            cwd=rc_dir, env=env, check=True,
        )
        subprocess.run(
            [zig_py, "-m", "ziglang", "cc", "-target", "x86_64-windows-gnu",
             "-Os", "-s", "-Wall", "-Wextra", "-Werror",
             "-Wl,--subsystem,windows", "-municode",
             str(c_file), str(res_path), *libs, "-o", str(out_exe)],
            cwd=ROOT, env=env, check=True,
        )
        size_kb = out_exe.stat().st_size // 1024
        return f"  ok    {out_exe.relative_to(ROOT)} ({size_kb} KB)"
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


def main() -> int:
    zig_py = ensure_zig_python()
    env = dict(os.environ)
    env["ZIG_GLOBAL_CACHE_DIR"] = "/home/user/.cache/zig"
    env["ZIG_LOCAL_CACHE_DIR"] = "/home/user/.cache/zig-local"

    targets = [
        (
            "hub",
            ROOT / "project-hub" / "src",
            "hub.rc",
            ROOT / "project-hub" / "src" / "project_hub.c",
            ROOT / "project-hub" / "ProjectHub.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-lole32", "-luuid",
             "-lcomctl32", "-lwinhttp", "-lshlwapi"],
        ),
        (
            "btguard",
            ROOT / "bluetooth-guard" / "src",
            "btguard.rc",
            ROOT / "bluetooth-guard" / "src" / "bluetooth_guard.c",
            ROOT / "bluetooth-guard" / "BluetoothGuard.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-lole32", "-luuid",
             "-ladvapi32", "-lcomctl32"],
        ),
    ]

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futs = [pool.submit(build_target, zig_py, env, *t) for t in targets]
        for fut in futs:
            print(fut.result())
    return 0


if __name__ == "__main__":
    sys.exit(main())
