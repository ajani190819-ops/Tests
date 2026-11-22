#!/usr/bin/env python3
"""Read-only evidence summary for explicitly supplied plain-text Orca G-code.

Never runs G-code, imports plugins, rewrites an export, discovers host profiles
or uploads anything. Output goes to stdout; paths, model names, machine G-code
and arbitrary comments/config values are not copied into the summary.
Marker/selection metadata is untrusted evidence, not proof of plugin execution,
freshness, correct geometry, effective plugin settings or print safety.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys

MAX_LINE = 1024 * 1024
PROFILE_KEYS = frozenset({
    "layer_height", "initial_layer_print_height", "nozzle_diameter", "filament_diameter",
    "resolution", "enable_arc_fitting", "wall_loops", "outer_wall_speed", "inner_wall_speed",
    "bridge_speed", "bridge_flow", "bridge_fan_speed", "overhang_fan_speed", "travel_speed",
    "travel_speed_z", "fill_angle", "sparse_infill_density",
})
CAPABILITIES = frozenset({"Wave Overhangs", "Unlayered Infill"})
SELECTION_KEYS = frozenset({"plugins", "slicing_pipeline_plugin", "post_process_plugin"})
NUMBER = r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?%?"
NUMERIC_LIST = re.compile(rf"{NUMBER}(?:\s*[,;]\s*{NUMBER})*")
STAMP = re.compile(r"^;\s*(wave-overhangs|unlayered-infill)(?:\s|$)")
VERSION = re.compile(r"^\s*v([0-9]+(?:\.[0-9]+)*(?:[-+][A-Za-z0-9.-]+)?)(?:\s|$)")
FIELD = re.compile(r"^;\s*([a-z][a-z0-9_]*)\s*=\s*(.*?)\s*$")
BOUNDARY = ("Comment metadata only: not proof of freshness, real-host execution, effective plugin "
            "Config/preset overrides, geometry correctness, collision clearance or a safe physical print")


def selected_capabilities(value, references=False):
    """Parse Orca's quoted semicolon list; never echo cloud keys or unknown names."""
    tokens = re.findall(r'"([^"\\\r\n]*)"', value)
    if re.sub(r'"[^"\\\r\n]*"', "", value).strip("; \t"):
        return [], 1
    known, unknown = set(), 0
    for token in tokens:
        name = token.rsplit(";", 1)[-1] if references else token
        if name in CAPABILITIES:
            known.add(name)
        elif name:
            unknown += 1
    return sorted(known), unknown


def inspect_export(path):
    """Hash the exact bytes read, preserving conflicts instead of guessing a value."""
    digest = hashlib.sha256()
    byte_count = line_count = blocks_started = blocks_complete = malformed = 0
    marker_count = wave_begin = wave_end = 0
    inside = False
    profile, selections, rejected = {}, {}, {}
    markers = []
    with Path(path).open("rb") as stream:
        before = os.fstat(stream.fileno())
        while True:
            raw = stream.readline(MAX_LINE + 1)
            if not raw:
                break
            if len(raw) > MAX_LINE:
                raise ValueError("Line exceeds the supported one-MiB limit; no partial summary")
            digest.update(raw)
            byte_count += len(raw)
            line_count += 1
            if b"\0" in raw or (line_count == 1 and raw.startswith((b"GCDE", b"PK\x03\x04"))):
                raise ValueError("Only plain-text G-code is supported, not binary G-code/archives")
            line = raw.decode("utf-8").rstrip("\r\n")
            if line_count == 1:
                line = line.removeprefix("\ufeff")
            stamp = STAMP.match(line)
            if stamp:
                marker_count += 1
                version = VERSION.match(line[stamp.end():])
                if len(markers) < 20:
                    markers.append({"plugin": stamp[1], "version": version[1] if version else None,
                                    "line": line_count})
            wave_begin += line.strip() == "; ==== WAVE OVERHANG BEGIN ===="
            wave_end += line.strip() == "; ==== WAVE OVERHANG END ===="
            if line == "; CONFIG_BLOCK_START":
                blocks_started += 1
                malformed += int(inside)
                inside = True
                continue
            if line == "; CONFIG_BLOCK_END":
                blocks_complete += int(inside)
                malformed += int(not inside)
                inside = False
                continue
            field = FIELD.match(line) if inside else None
            if not field:
                continue
            key, value = field.groups()
            if key in PROFILE_KEYS:
                if len(value) > 128 or NUMERIC_LIST.fullmatch(value) is None:
                    rejected[key] = rejected.get(key, 0) + 1
                    continue
                values = profile.setdefault(key, [])
                if value not in values:
                    if len(values) >= 8:
                        raise ValueError("Too many conflicting profile values; no partial summary")
                    values.append(value)
            elif key in SELECTION_KEYS:
                names, unknown = selected_capabilities(value, references=key == "plugins")
                entry = selections.setdefault(key, {"recognized": [], "unrecognized_entries": 0, "records": 0})
                entry["recognized"] = sorted(set(entry["recognized"]) | set(names))
                entry["unrecognized_entries"] += unknown
                entry["records"] += 1
        after = os.fstat(stream.fileno())
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError("Input changed during inspection; no stable evidence summary")
    if not byte_count:
        raise ValueError("Empty input is not G-code evidence")
    malformed += int(inside)
    processed = marker_count > 0 or wave_begin > 0 or wave_end > 0
    warnings = []
    if processed:
        warnings.append("Recognized prior rewrite evidence; do not use this as an untouched slicer control")
    else:
        warnings.append("No recognized rewrite markers found; absence is NOT proof of an untouched or fresh export")
    if blocks_started != 1 or blocks_complete != 1 or malformed:
        warnings.append("Missing, multiple or malformed config blocks; do not infer one complete profile")
    conflicts = sorted(key for key, values in profile.items() if len(values) > 1)
    if conflicts:
        warnings.append("Conflicting profile metadata retained; no last-value/default guess was made")
    if rejected:
        warnings.append("Unsupported allowlisted values were omitted, not guessed")
    warnings.append("Export selections do not contain the effective plugin Config/preset override JSON; collect it in Orca")
    return {"bytes": byte_count, "sha256": digest.hexdigest(), "lines": line_count,
            "recognized_processing_evidence": processed, "processing_markers": markers,
            "processing_marker_count": marker_count, "processing_markers_omitted": max(0, marker_count - len(markers)),
            "wave_blocks": {"begin": wave_begin, "end": wave_end},
            "config_blocks": {"started": blocks_started, "complete": blocks_complete, "malformed": malformed},
            "profile_values": profile, "conflicting_profile_keys": conflicts,
            "omitted_profile_values": rejected, "selection_metadata": selections,
            "acceptance": "NOT_EVALUATED", "boundary": BOUNDARY, "warnings": warnings}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gcode", nargs="+", help="explicit plain-text .gcode file(s), in report order")
    parser.add_argument("--reject-processed", action="store_true",
                        help="exit 2 when recognized prior rewrite evidence is found; never certifies pristine input")
    args = parser.parse_args(argv)
    files, errors = [], []
    for index, path in enumerate(args.gcode, 1):
        try:
            files.append({"input_index": index, **inspect_export(path)})
        except (OSError, ValueError) as exc:
            errors.append({"input_index": index, "error": type(exc).__name__,
                           "status": "UNAVAILABLE; no evidence/acceptance inferred"})
    print(json.dumps({"schema": 1, "files": files, "errors": errors}, indent=2, sort_keys=True, allow_nan=False))
    if errors:
        return 1
    if args.reject_processed and any(item["recognized_processing_evidence"] for item in files):
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
