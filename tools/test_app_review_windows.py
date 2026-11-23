#!/usr/bin/env python3
"""Opt-in native Windows fixtures for App Review, never owner-app automation.

Requires preinstalled Python + ziglang==0.17.1. Compiles only the fixture host
that includes the production capture code, then captures that synthetic host.
No installed suite app, device, game, settings UI or security control is used.
"""
import argparse
import base64
import hashlib
import importlib.metadata
import json
import os
import re
import struct
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools import test_radar_windows as common

ZIG_VERSION = "0.17.1"
BOUNDARY = ("Synthetic native-window capture/timeout/settings-sentinel fixtures only; "
            "not installed-app compatibility, desktop screenshots or owner acceptance")
TRUE_FIELDS = ("native_windows", "synthetic_fixture", "settings_unchanged", "placement_unchanged",
               "timeout_contained", "cancellation_contained", "protected_skipped", "minimized_skipped", "mismatch_skipped",
               "blank_not_passed", "overwrite_refused", "evidence_not_accepted")


def validate_native_result(data, returncode):
    if type(returncode) is not int or returncode != 0 or not isinstance(data, dict):
        raise ValueError("Native fixture process failed; not a pass")
    if data.get("schema") != "tests.app-review-native.v1":
        raise ValueError("Wrong native fixture schema")
    for name in TRUE_FIELDS:
        if data.get(name) is not True:
            raise ValueError(f"Missing or failed native fixture: {name}")
    for name, minimum in (("checks", 25), ("images", 1)):
        value = data.get(name)
        if type(value) is not int or not minimum <= value <= 1000000:
            raise ValueError(f"Invalid native fixture count: {name}")
    if type(data.get("target_input_count")) is not int or data["target_input_count"] != 0:
        raise ValueError("Fixture received target input")
    if data.get("owner_acceptance") is not False:
        raise ValueError("Native fixtures cannot grant owner acceptance")
    return data


def run_command(command, cwd, log, timeout):
    """Bounded one-shot fixture/compiler process, with explicit cwd and log."""
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        # Only the process this runner created and its descendants; never apps.
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           capture_output=True, timeout=10)
        if process.poll() is None:
            process.kill()
        stdout, stderr = process.communicate()
        with log.open("xb") as stream:
            stream.write((stdout or b"") + (stderr or b"") + b"\nTIMEOUT: not a pass\n")
        raise ValueError("Native fixture/compiler timed out; not a pass") from None
    with log.open("xb") as stream:
        stream.write((stdout or b"") + (stderr or b""))
    return process.returncode


def validate_fixture_report(directory):
    """Validate actual native fixture output, never a discovered owner report."""
    paths = {name: directory / name for name in ("fixture-report.json", "fixture-report.html", "window-01.png")}
    for path in paths.values():
        if path.is_symlink() or not path.is_file() or not 24 <= path.stat().st_size <= 8 * 1024 * 1024:
            raise ValueError("Missing/oversized fixture evidence")
    report = json.loads(paths["fixture-report.json"].read_text(encoding="ascii"))
    if (not isinstance(report, dict) or report.get("schema") != "tests.app-review.v1"
            or report.get("synthetic_fixture") is not True or report.get("owner_acceptance") is not False
            or report.get("whole_desktop_capture") is not False or report.get("automatic_upload") is not False
            or report.get("target_input_or_settings_requests") is not False
            or report.get("capture_kind") != "os_composited_window"
            or report.get("visual_acceptance") != "NOT_EVALUATED" or report.get("functional_acceptance") != "NOT_EVALUATED"):
        raise ValueError("Fixture evidence identity/acceptance boundary failed")
    windows = report.get("windows")
    if not isinstance(windows, list) or len(windows) != 1 or not isinstance(windows[0], dict):
        raise ValueError("Fixture report must contain only its one selected synthetic window")
    window = windows[0]
    image = paths["window-01.png"].read_bytes()
    markup = paths["fixture-report.html"].read_text(encoding="ascii")
    embedded = re.findall(r'data:image/png;base64,([A-Za-z0-9+/=]+)', markup)
    if (window.get("status") != "IMAGE_UNVERIFIED" or window.get("class") != "AppReview_NativeFixture"
            or window.get("ping_answered") is not True or window.get("placement_unchanged") is not True
            or window.get("capture_dimensions") != [420, 260]
            or not all(type(value) is int for value in window.get("capture_dimensions", []))
            or window.get("png") != "window-01.png"
            or type(window.get("png_bytes")) is not int or window["png_bytes"] != len(image)
            or not image.startswith(b"\x89PNG\r\n\x1a\n") or struct.unpack(">II", image[16:24]) != (420, 260)
            or len(embedded) != 1 or base64.b64decode(embedded[0], validate=True) != image
            or "<script" in markup.lower() or "default-src 'none'; img-src data:" not in markup
            or "SYNTHETIC NATIVE FIXTURE REPORT" not in markup):
        raise ValueError("Native fixture image/HTML/metadata is incomplete or not inert")
    return {name: {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            for name, path in paths.items()}


def run_review(output):
    if os.name != "nt":
        raise ValueError("Native Windows is required; a cross-build is not a native pass")
    common.validate_windows_path(output)
    if importlib.metadata.version("ziglang") != ZIG_VERSION:
        raise ValueError(f"Preinstall ziglang=={ZIG_VERSION}; this runner does not download dependencies")
    directory = common.create_output_directory(Path(output))
    resource = directory / "review.res"
    executable = directory / "app-review-native-fixture.exe"
    cc = [sys.executable, "-m", "ziglang"]
    commands = [
        (cc + ["rc", "/fo", str(resource), "review.rc"], ROOT / "app-review/src", "resource.log", 120),
        (cc + ["cc", "-target", "x86_64-windows-gnu", "-Os", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-function", "-municode", "-Wl,--subsystem,console",
               str(ROOT / "tests/native/app_review_windows.c"), str(resource),
               "-luser32", "-lgdi32", "-lshell32", "-lole32", "-luuid", "-lcomctl32", "-lversion",
               "-ld3d11", "-ldxguid",
               "-o", str(executable)], ROOT, "compile.log", 600),
        ([str(executable)], directory, "native.log", 90),
    ]
    result_code = 1
    for command, cwd, log, timeout in commands:
        result_code = run_command(command, cwd, directory / log, timeout)
        if result_code != 0:
            raise ValueError(f"Native fixture step failed ({log}); not a pass")
    lines = (directory / "native.log").read_text(encoding="utf-8", errors="replace").splitlines()
    if len(lines) != 1:
        raise ValueError("Expected one complete native fixture JSON result")
    result = validate_native_result(json.loads(lines[0]), result_code)
    artifacts = validate_fixture_report(directory)
    sources = list((ROOT / "app-review/src").rglob("*")) + [ROOT / "tests/native/app_review_windows.c",
        ROOT / "shared/win32/suite_emblem.c", ROOT / "project-hub/hub.ico", Path(__file__).resolve()]
    payload = {"boundary": BOUNDARY, "owner_acceptance": False, "native_fixture": result,
               "fixture_artifacts": artifacts,
               "native_fixture_exe_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
               "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in sorted(sources) if p.is_file()}}
    common.write_json(directory / "native-review.json", payload)
    return payload


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="NEW absolute local output directory outside the checkout")
    args = parser.parse_args(argv)
    try:
        result = run_review(args.output)
    except (OSError, ValueError, importlib.metadata.PackageNotFoundError) as exc:
        print(f"App Review native fixtures: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
