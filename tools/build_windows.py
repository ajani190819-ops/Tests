#!/usr/bin/env python3
"""Parallel zero-discovery builder for ProjectHub.exe, BluetoothGuard.exe,
KeyboardLighting.exe, and DnsPilot.exe.

Usage:
    python3 tools/build_windows.py

Automatically uses /home/user/.venv (bootstrapping ziglang outside the repo if
needed) and caches compiler artifacts in /home/user/.cache/zig so workspace
snapshots stay lean and rebuilds complete in ~1.5 seconds.

Targets whose src dir, libraries, flags and compiler version are byte-identical
to the last successful build are skipped entirely (cache in
/home/user/.cache/winbuild, outside the repo). Pass --force to rebuild
everything regardless - do that before committing an EXE you actually changed
if you suspect the cache, though a source edit always changes the key.
"""
import concurrent.futures
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
VENV_DIR = pathlib.Path("/home/user/.venv")
VENV_PY = VENV_DIR / "bin" / "python"
# Outside the repo on purpose: workspace snapshots stay lean, and a cache hit
# must never be mistaken for repo content.
CACHE_DIR = pathlib.Path("/home/user/.cache/winbuild")

# The exact flags every target compiles with. Part of the cache key: changing
# them must invalidate every cached EXE.
CC_FLAGS = ["-target", "x86_64-windows-gnu", "-Os", "-s", "-Wall", "-Wextra",
            "-Werror", "-Wl,--subsystem,windows", "-municode"]


def zig_version(zig_py: str) -> str:
    r = subprocess.run([zig_py, "-m", "ziglang", "version"], capture_output=True)
    return r.stdout.decode().strip() or "unknown"


def target_key(src_dir: pathlib.Path, libs: list[str], zig_ver: str) -> str:
    """Hash everything that can change the output: every file under the
    target's src dir (sources, inlined parts, .rc, manifest, icons), the link
    libraries, the flags and the compiler version. Whole-dir on purpose -
    parsing the #include chain would miss a resource or an icon swap."""
    h = hashlib.sha256()
    h.update(zig_ver.encode())
    h.update("\0".join(sorted(libs)).encode())
    h.update("\0".join(CC_FLAGS).encode())
    for p in sorted(src_dir.rglob("*")):
        if p.is_file():
            h.update(str(p.relative_to(src_dir)).encode())
            h.update(b"\0")
            h.update(hashlib.sha256(p.read_bytes()).digest())
    return h.hexdigest()


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
                 libs: list[str], zig_ver: str = "", force: bool = False) -> str:
    key = target_key(rc_dir, libs, zig_ver)
    cache = CACHE_DIR / f"{name}.sha"
    if not force and out_exe.exists() and cache.exists() \
            and cache.read_text().strip() == key:
        size_kb = out_exe.stat().st_size // 1024
        return f"  skip  {out_exe.relative_to(ROOT)} (unchanged, {size_kb} KB)"
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
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        cache.write_text(key)
        return f"  ok    {out_exe.relative_to(ROOT)} ({size_kb} KB)"
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


def main() -> int:
    force = "--force" in sys.argv[1:]
    zig_py = ensure_zig_python()
    zig_ver = zig_version(zig_py)
    env = dict(os.environ)
    env["ZIG_GLOBAL_CACHE_DIR"] = "/home/user/.cache/zig"
    env["ZIG_LOCAL_CACHE_DIR"] = "/home/user/.cache/zig-local"

    targets = [
        (
            "audioswitcher",
            ROOT / "audio-switcher" / "src",
            "audio-switcher.rc",
            ROOT / "audio-switcher" / "src" / "audio_switcher.c",
            ROOT / "audio-switcher" / "AudioSwitcher.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-lole32", "-luuid",
             "-lcomctl32", "-lshlwapi", "-ladvapi32"],
        ),
        (
            "hub",
            ROOT / "project-hub" / "src",
            "hub.rc",
            ROOT / "project-hub" / "src" / "project_hub.c",
            ROOT / "project-hub" / "ProjectHub.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-lole32", "-luuid",
             "-lcomctl32", "-lwinhttp", "-lshlwapi", "-lbcrypt"],
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
        (
            "kbl",
            ROOT / "keyboard-lighting" / "src",
            "kbl.rc",
            ROOT / "keyboard-lighting" / "src" / "keyboard_lighting.c",
            ROOT / "keyboard-lighting" / "KeyboardLighting.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-ladvapi32",
             "-lcomctl32", "-lcomdlg32", "-lwinhttp"],
        ),
        (
            "dnspilot",
            ROOT / "dns-pilot" / "src",
            "dns-pilot.rc",
            ROOT / "dns-pilot" / "src" / "dns_pilot.c",
            ROOT / "dns-pilot" / "DnsPilot.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-ladvapi32",
             "-lcomctl32", "-lws2_32", "-liphlpapi", "-lwinhttp"],
        ),
        ("agentnotes",
            ROOT / "agent-notes" / "src",
            "notes.rc",
            ROOT / "agent-notes" / "src" / "agent_notes.c",
            ROOT / "agent-notes" / "AgentNotes.exe",
            ["-luser32", "-lgdi32", "-lshell32", "-ladvapi32",
             "-lcomctl32", "-lwinhttp"],
        ),
    ]

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futs = [pool.submit(build_target, zig_py, env, *t, zig_ver, force)
                for t in targets]
        for fut in futs:
            print(fut.result())
    return 0


if __name__ == "__main__":
    sys.exit(main())
