#!/usr/bin/env python3
"""Real-export regression for Wave's one-pass bridge replacement.

Run with numpy and shapely available (the same dependencies Orca installs):
  PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py
"""
import importlib.util
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))

try:
    import numpy  # noqa: F401
    import shapely  # noqa: F401
except ImportError:
    print("SKIP -- test_wave_gcode needs numpy + shapely")
    raise SystemExit(0)

import fake_orca

orca = fake_orca.install()
path = ROOT / "plugins/wave-overhangs/wave_overhangs_orca.py"
spec = importlib.util.spec_from_file_location("wave_overhangs_orca", path)
wave = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = wave
spec.loader.exec_module(wave)

source = (ROOT / "tests/fixtures/Cube^2_3m53s.gcode").read_text(
    encoding="utf-8")
cfg = dict(wave._DEFAULTS)
cfg["_lh"] = 0.3
out, stats = wave._gcode_wave_rewrite(source, cfg)


def as_absolute_e(text):
    """Convert this captured relative-E fixture to an equivalent M82 file."""
    absolute = 0.0
    converted = []
    number = r"-?(?:\d+(?:\.\d*)?|\.\d+)"
    for line in text.splitlines(keepends=True):
        code = line.split(";", 1)[0]
        if re.match(r"^\s*M83\b", code):
            line = re.sub(r"^(\s*)M83\b", r"\1M82", line, count=1)
        elif re.match(r"^\s*G92\b", code):
            match = re.search(r"\bE(" + number + r")", code)
            if match:
                absolute = float(match.group(1))
        elif re.match(r"^\s*G[01]\b", code):
            match = re.search(r"\bE(" + number + r")", code)
            if match:
                absolute += float(match.group(1))
                line = line[:match.start(1)] + f"{absolute:.5f}" + line[match.end(1):]
        converted.append(line)
    return "".join(converted)


absolute_out, absolute_stats = wave._gcode_wave_rewrite(
    as_absolute_e(source), cfg)
assert absolute_stats["wave_layers"] == 3, absolute_stats
assert absolute_stats["removed_moves"] == stats["removed_moves"]
assert "\nM82\nG92 E" in absolute_out, (
    "absolute-E rewrite must restore both mode and command value")
assert absolute_out.count("G92 E") > as_absolute_e(source).count("G92 E")

assert stats["wave_layers"] == 3, stats
assert stats["replaced_sections"] == 3, stats
assert stats["removed_moves"] == 103, stats
assert stats["kept_fragments"] == 31, (
    "substantial uncovered bridge fragments must remain", stats)
assert stats["tiny_fragments_dropped"] == 111, (
    "sub-nozzle edge remnants should be cleaned up", stats)
assert stats["short_wave_paths_dropped"] == 30, (
    "isolated short wavefronts should be cleaned up", stats)
assert out.startswith("; wave-overhangs v"), "missing build stamp"
assert out.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert out.count("; ==== WAVE OVERHANG END ====") == 3
wave_move_count = sum(
    block.count("\nG1 X") for block in re.findall(
        r"; ==== WAVE OVERHANG BEGIN ====(.*?)"
        r"; ==== WAVE OVERHANG END ====", out, re.DOTALL))
assert wave_move_count == 386, "wavefront cleanup changed unexpectedly"
assert "; wave-overhangs replaced covered bridge move" in out
lines = out.splitlines()
for marker in [i for i, line in enumerate(lines)
               if line.startswith("; wave-overhangs replaced covered bridge move")]:
    first_motion = next(
        (lines[j] for j in range(marker + 1, len(lines))
         if lines[j].startswith(("G0", "G1"))), None)
    assert first_motion is not None and first_motion.startswith("G0"), (
        "every replaced bridge segment must return with non-extruding travel")

# A covered segment can leave the nozzle at the end of a generated Wave path.
# The next original retained bridge move must begin with an explicit G0, never
# a diagonal G1 extrusion from the Wave endpoint.
for end in [i for i, line in enumerate(lines)
            if line == "; ==== WAVE OVERHANG END ===="]:
    for i in range(end + 1, len(lines)):
        if lines[i].startswith(";LAYER_CHANGE"):
            break
        if lines[i].startswith("G1") and " E" in lines[i]:
            previous_motion = next(
                (lines[j] for j in range(i - 1, end, -1)
                 if lines[j].startswith(("G0", "G1"))), None)
            assert previous_motion is not None and previous_motion.startswith("G0"), (
                "first retained extrusion after a Wave block needs a non-extruding travel")
            break

assert ";Z:5.4" in out and "Z5.650" in out, (
    "Wave must use the bridge move's real Z, including Orca's 0.25 mm Z offset")
assert "Z9.850" in out and "Z14.650" in out
assert "Z5.400" not in out.split("; ==== WAVE OVERHANG END ====", 1)[0], (
    "nominal layer Z leaked into the first Wave block")
assert ";TYPE:Bridge" in out, "section labels should remain inspectable"
assert "M106 S" in out, "wave blocks must restore the prior fan setting"

# The expanded pattern setting is real, not just documentation. Monotonic uses
# one endpoint direction for all fronts while retaining the same safe geometry
# bounds and transactional behavior.
monotonic, monotonic_stats = wave._gcode_wave_rewrite(
    source, dict(cfg, pattern="monotonic", start_policy="consistent"))
assert monotonic_stats["wave_layers"] == 3, monotonic_stats
assert monotonic.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert monotonic != out, "pattern setting did not change the ordered toolpaths"

again, second = wave._gcode_wave_rewrite(out, cfg)
assert again == out and second["already_processed"], "second pass must be a no-op"

old = wave.wc.wave_tracks
try:
    def broken(*_args, **_kwargs):
        raise RuntimeError("deliberate generation failure")
    wave.wc.wave_tracks = broken
    failed, failure_stats = wave._gcode_wave_rewrite(source, cfg)
finally:
    wave.wc.wave_tracks = old
assert failed == source, "generation failure must retain the original G-code byte-for-byte"
assert "error" in failure_stats

print("ok -- real Cube^2 export: 3 cleaned wave layers use actual offset Z, "
      "replace covered moves, retain substantial fragments, restore fan state, "
      "fail closed, and are idempotent")
