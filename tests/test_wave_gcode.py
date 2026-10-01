#!/usr/bin/env python3
"""Real-export regression for Wave's one-pass bridge replacement.

Run with numpy and shapely available (the same dependencies Orca installs):
  PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py
"""
import importlib.util
import pathlib
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

source = (ROOT / "Cube^2_3m53s.gcode").read_text(encoding="utf-8")
cfg = dict(wave._DEFAULTS)
cfg["_lh"] = 0.3
out, stats = wave._gcode_wave_rewrite(source, cfg)

assert stats["wave_layers"] == 3, stats
assert stats["replaced_sections"] == 3, stats
assert stats["removed_moves"] == 112, stats
assert stats["kept_fragments"] == 158, (
    "partial coverage must retain the fixture's uncovered fragments", stats)
assert out.startswith("; wave-overhangs v"), "missing build stamp"
assert out.count("; ==== WAVE OVERHANG BEGIN ====") == 3
assert out.count("; ==== WAVE OVERHANG END ====") == 3
assert "; wave-overhangs replaced covered bridge move" in out
assert ";Z:5.4" in out and "Z5.400" in out
assert ";TYPE:Bridge" in out, "section labels should remain inspectable"
assert "M106 S" in out, "wave blocks must restore the prior fan setting"

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

print("ok -- real Cube^2 export: 3 wave layers replace covered bridge moves, "
      "retain uncovered fragments, restore fan state, fail closed, and are idempotent")
