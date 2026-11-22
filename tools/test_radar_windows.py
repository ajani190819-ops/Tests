#!/usr/bin/env python3
"""Opt-in Windows Weather Report component review, not installed-app acceptance.

Requires Python + ziglang==0.17.1 already installed. Default fixtures refuse
HTTP before session creation. --live explicitly enables public weather/tile
requests for two fixed cities. Writes only into one NEW local output directory
outside the checkout; no uploads, app installs, profile discovery or policy edits.
"""
import argparse
import ctypes
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path, PureWindowsPath
import stat
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
ZIG_VERSION = "0.17.1"
BOUNDARY = ("Production components and click handlers only; not installed-EXE "
            "startup, monitor-DPI transitions, Windows location or owner desktop acceptance")


def validate_windows_path(value):
    """Reject remote/device/relative/ADS paths before any Windows filesystem I/O."""
    path = PureWindowsPath(value)
    if (not path.is_absolute() or len(path.drive) != 2 or path.drive[1] != ":"
            or not path.drive[0].isascii() or not path.drive[0].isalpha()
            or ":" in str(path)[2:] or ".." in path.parts or len(str(path)) > 180):
        raise ValueError("Use a short absolute local drive path, without ADS or parent traversal")
    return path


def create_output_directory(path):
    """Exclusive new directory; existing files/profiles and reparse parents are refused."""
    path = Path(path)
    if not path.is_absolute() or ".." in path.parts:
        raise ValueError("Output directory must be absolute without parent traversal")
    # Do not resolve symlinks before inspecting them. Existing parents are
    # required, and mkdir(exist_ok=False) refuses an already-used destination.
    for parent in path.parents:
        info = parent.lstat()
        if (stat.S_ISLNK(info.st_mode)
                or getattr(info, "st_file_attributes", 0) & 0x400):  # Windows reparse point
            raise ValueError("Reparse/symlink parents are not followed")
    if path.is_relative_to(ROOT):
        raise ValueError("Review artifacts must stay outside the repository")
    path.mkdir(exist_ok=False)
    return path


def write_json(path, payload):
    with Path(path).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(payload, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")


def validate_result(data, mode, returncode):
    """Fail closed on crashes, partial/missing results and type-coerced booleans.

    This is a portable schema check, NOT proof of native execution by itself.
    Only run_review invokes the component EXE after its Windows platform gate.
    """
    if returncode != 0 or not isinstance(data, dict):
        raise ValueError("Native process failed or did not return a result object")
    if (data.get("native_windows") is not True or data.get("passed") is not True
            or data.get("mode") != mode or mode not in ("fixture", "live")):
        raise ValueError("Missing, failed or wrong-mode native result")
    for key in ("checks", "clicks", "text_overflows", "concurrent_builds", "blocked_http", "http_requests"):
        if type(data.get(key)) is not int or data[key] < 0:
            raise ValueError("Invalid native counter: " + key)
    if not data["checks"] or data["text_overflows"] or data["blocked_http"]:
        raise ValueError("Incomplete review, text overflow or refused fixture network request")
    if mode == "fixture" and (data["clicks"] < 125 or not data["concurrent_builds"] or data["http_requests"]):
        raise ValueError("Fixture click/stress/offline gate did not complete")
    if mode == "live" and not data["http_requests"]:
        raise ValueError("Live review did not send any HTTP requests")


def validate_live_city(data, city):
    if not isinstance(data, dict) or data.get("city") != city or data.get("passed") is not True:
        raise ValueError("Missing or failed live-city telemetry")
    for key in ("forecast", "days", "ensemble", "members", "frames", "view", "base_pixels"):
        if type(data.get(key)) is not int:
            raise ValueError("Invalid live-city counter: " + key)
    if (data["forecast"] != 2 or data["days"] != 5 or data["ensemble"] != 1
            or data["members"] < 2 or data["frames"] < 1 or data["view"] not in (1, 2)
            or data["base_pixels"] <= 1000):
        raise ValueError("Live provider data is missing or incomplete")


def capture_manifest(directory, mode):
    """Require every expected capture. Do not copy cache tiles or owner files."""
    if mode == "fixture":
        expected = {f"fixture-planner-category{category}-dpi96-fit100.png" for category in range(4)}
        expected.update(f"fixture-planner-rain-dpi{dpi}-fit{fit}.png"
                        for dpi, fit in ((96, 100), (96, 75), (96, 55), (144, 75), (192, 55)))
        expected.update(f"fixture-radar-city{city}-zoom{zoom}.png" for city in range(2) for zoom in (7, 10))
    elif mode == "live":
        expected = {f"live-city{city}-planner.png" for city in range(2)}
        expected.update(f"live-city{city}-radar-zoom{zoom}.png" for city in range(2) for zoom in (7, 10))
    else:
        raise ValueError("Unknown review mode")
    manifest = []
    for name in sorted(expected):
        image = (directory / name).read_bytes()
        if not image.startswith(b"\x89PNG\r\n\x1a\n") or len(image) < 33:
            raise ValueError("Missing/invalid PNG capture: " + name)
        manifest.append({"file": f"{mode}/{name}", "bytes": len(image),
                         "sha256": hashlib.sha256(image).hexdigest()})
    return manifest


def run_command(command, log, timeout, env=None):
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=timeout, env=env)
        body = (result.stdout or b"") + (result.stderr or b"")
    except subprocess.TimeoutExpired as exc:
        body = (exc.stdout or b"") + (exc.stderr or b"") + b"\nREVIEW TIMEOUT: not a pass\n"
        with log.open("xb") as stream:
            stream.write(body)
        raise ValueError("Component review timed out; inspect its new output directory") from None
    with log.open("xb") as stream:
        stream.write(body)
    return result.returncode


def source_manifest():
    inputs = [ROOT / "tests/native/radar_windows_review.c"]
    for directory in (ROOT / "weather-radar/src", ROOT / "shared/win32"):
        inputs += [p for p in directory.rglob("*") if p.suffix in (".c", ".h")]
    return {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(inputs)}


def run_review(output, live=False):
    if os.name != "nt":
        raise ValueError("Must execute on Windows. A cross-build is not a native pass")
    validate_windows_path(output)
    get_drive_type = ctypes.windll.kernel32.GetDriveTypeW
    get_drive_type.argtypes = [ctypes.c_wchar_p]
    get_drive_type.restype = ctypes.c_uint
    if get_drive_type(str(Path(output).anchor)) not in (2, 3):
        raise ValueError("Only local fixed/removable drives are permitted")
    if importlib.metadata.version("ziglang") != ZIG_VERSION:
        raise ValueError("This review requires preinstalled ziglang==" + ZIG_VERSION)
    directory = create_output_directory(Path(output))
    source_hashes = source_manifest()
    write_json(directory / "build.json", {"ziglang": ZIG_VERSION, "sources": source_hashes,
                                          "boundary": BOUNDARY, "owner_acceptance": False})
    exe = directory / "RadarNativeReview.exe"
    command = [sys.executable, "-m", "ziglang", "cc", "-target", "x86_64-windows-gnu",
               "-O1", "-Wall", "-Wextra", "-Werror", "-municode",
               str(ROOT / "tests/native/radar_windows_review.c"),
               "-luser32", "-lgdi32", "-lshell32", "-ladvapi32", "-lcomctl32",
               "-lwinhttp", "-lole32", "-luuid", "-lshlwapi", "-o", str(exe)]
    compile_env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(directory / "compiler-cache"),
                       ZIG_LOCAL_CACHE_DIR=str(directory / "compiler-local-cache"),
                       PYTHONDONTWRITEBYTECODE="1")
    if run_command(command, directory / "build.log", 300, env=compile_env):
        raise ValueError("Harness compilation failed; no native execution pass")
    modes = ["fixture", "live"] if live else ["fixture"]
    manifests, results = [], {}
    for mode in modes:
        destination = directory / mode  # harness, not runner, creates this NEW directory
        arguments = [str(exe), str(destination)] + (["--live"] if mode == "live" else [])
        code = run_command(arguments, directory / (mode + ".log"), 600 if mode == "live" else 180)
        data = json.loads((destination / "result.json").read_text(encoding="utf-8"))
        validate_result(data, mode, code)
        if mode == "live":
            for city in range(2):
                city_data = json.loads((destination / f"live-city{city}-result.json").read_text(encoding="utf-8"))
                validate_live_city(city_data, city)
        manifests.extend(capture_manifest(destination, mode))
        results[mode] = data
    if source_manifest() != source_hashes:
        raise ValueError("Review sources changed during build/execution; rerun in a new directory")
    write_json(directory / "captures.json", manifests)
    write_json(directory / "review.json", {"native_component_pass": True, "owner_acceptance": False,
                                           "exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
                                           "boundary": BOUNDARY, "results": results})
    return directory


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-directory", required=True, help="NEW absolute local path outside the checkout")
    parser.add_argument("--live", action="store_true", help="opt in to public weather/tile HTTP after fixtures")
    args = parser.parse_args(argv)
    try:
        directory = run_review(args.output_directory, args.live)
    except (OSError, ValueError, importlib.metadata.PackageNotFoundError) as exc:
        print("INCOMPLETE Weather component review: " + str(exc), file=sys.stderr)
        return 1
    print("Native component review passed: " + str(directory))
    print(BOUNDARY + "; owner acceptance remains OPEN.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
