"""Focused Wave print-order / part-fan regressions, not hardware validation."""
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock

from shapely.geometry import LineString, Polygon, box

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests"))
import fake_orca


def load_wave():
    orca = fake_orca.install()
    name = "wave_print_safeguards_test"
    spec = importlib.util.spec_from_file_location(
        name, ROOT / "plugins/wave-overhangs/wave_overhangs_orca.py")
    wave = importlib.util.module_from_spec(spec)
    sys.modules[name] = wave
    spec.loader.exec_module(wave)
    return wave, orca


def small_overhang_print():
    """A 12 mm tab on a large body: exposed wall is under 25% of its loop."""
    text = ["G90", "M83", ";Z:0.3", "G1 Z0.3 F9000",
            ";TYPE:Outer wall", ";WIDTH:0.50", "G0 X20.25 Y20.25"]
    for x, y in [(99.75, 20.25), (99.75, 59.75),
                 (20.25, 59.75), (20.25, 20.25)]:
        text.append(f"G1 X{x} Y{y} E0.4 F1800")
    text += [";TYPE:Internal solid infill", ";WIDTH:0.50"]
    for n in range(87):
        y = 20.65 + n * 0.45
        text += [f"G0 X20.65 Y{y:.3f}", f"G1 X99.35 Y{y:.3f} E2.75 F2400"]
    text += [";Z:0.6", "G1 Z0.6 F9000 ; layer Z must stay here",
             ";TYPE:Outer wall", ";WIDTH:0.50", "G0 X20.25 Y20.25"]
    for x, y in [(99.75, 20.25), (99.75, 35.0), (111.75, 35.0),
                 (111.75, 45.0), (99.75, 45.0), (99.75, 59.75),
                 (20.25, 59.75), (20.25, 20.25)]:
        text.append(f"G1 X{x} Y{y} E0.4 F1800 ; upper boundary")
    text += [";TYPE:Bridge", ";WIDTH:0.50", "M106 S255"]
    for n in range(18):
        y = 35.5 + n * 0.5
        text += [f"G0 X99.65 Y{y:.3f}",
                 f"G1 X111.15 Y{y:.3f} E0.4 F1200"]
    text += [";TYPE:Sparse infill", "G0 X25 Y25 F6000", "G1 X26 Y25 E0.1 F2400"]
    return "\n".join(text) + "\n"


def variable_feed_overhang_print():
    """Synthetic speed-segmented wall with balanced retract/wipe plumbing.

    This reproduces the command structure, not an owner file or machine preset.
    """
    source = small_overhang_print().replace(
        ";Z:0.6", "G1 E-1.25 F1500\n;Z:0.6", 1).replace(
        "G1 X111.15 Y35.500 E0.4 F1200",
        "G1 E1.25 F2700\nG1 X111.15 Y35.500 E0.4 F1200", 1)
    output = []
    n = 0
    for line in source.splitlines():
        if "; upper boundary" in line:
            if n == 0:
                output.append("G1 E1.25 F2700")
            output.append(f"G1 F{(1800, 1500, 600, 900)[n % 4]}")
            if n in (2, 4):
                output.append(";TYPE:Overhang wall" if n == 2 else ";TYPE:Outer wall")
            output.append(line.replace(" F1800", ""))
            n += 1
        else:
            if line == ";TYPE:Bridge":
                output += ["G1 E-1.25 F1500", ";WIPE_START", "G1 F600",
                           "G1 X20.85 Y20.25", ";WIPE_END"]
            output.append(line)
    return "\n".join(output) + "\n"


def relative_extrusion_state(wave, text):
    """Deposition geometry/Z/E/feed/debt, final debt and surplus priming."""
    x = y = 0.0
    z = feed = None
    debt = surplus = 0.0
    events = []
    for line in text.splitlines():
        if not wave._MOVE_CODE.match(line):
            continue
        words = wave._gwords(line)
        feed = words.get("F", feed)
        z = words.get("Z", z)
        nx, ny = words.get("X", x), words.get("Y", y)
        e = words.get("E", 0)
        if e and (nx, ny) != (x, y):
            events.append((x, y, nx, ny, z, e, feed, debt))
        elif e > 0:
            surplus += max(0, e - debt)
        debt = max(0, debt - e)
        x, y = nx, ny
    return sorted(events), debt, surplus


class WavePrintSafeguardsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wave, cls.orca = load_wave()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.storage = mock.patch.dict(os.environ, {"ORCA_PLUGIN_STORAGE_DIR": self.temp.name})
        self.storage.start()
        self.addCleanup(self.storage.stop)

    def wall_layer(self, relative=True):
        pts = [(0, 0), (100, 0), (100, 45), (104, 45), (104, 55),
               (100, 55), (100, 100), (0, 100), (0, 0)]
        lines = ["M83\n" if relative else "M82\n", ";Z:0.6\n",
                 ";TYPE:Outer wall\n", "G0 X0 Y0\n"]
        lines += [f"G1 X{x} Y{y} E{0.4 if relative else n * 0.4}\n"
                  for n, (x, y) in enumerate(pts[1:], 1)]
        return lines, self.wave._parse_gcode_geometry(lines)[0]

    def test_safeguards_are_in_normal_and_full_mutation_gates(self):
        from tools import check_all, mutate
        self.assertIn("test_wave_print_safeguards.py", check_all.TESTS)
        self.assertIn("tests.test_wave_print_safeguards", mutate.FULL_TESTS)

    def test_small_exposed_portion_moves_the_whole_loop(self):
        lines, layer = self.wall_layer()
        support = box(-0.25, -0.25, 100.25, 100.25)
        total = sum(seg["geom"].length for seg in layer["walls"])
        exposed = sum(seg["geom"].difference(support.buffer(0.05)).length
                      for seg in layer["walls"])
        self.assertGreater(exposed, 10)
        self.assertLess(exposed, total * 0.25)
        runs = self.wave._overhanging_wall_runs(layer, support, 0.05, lines)
        self.assertEqual(runs, [layer["walls"]])

    def test_supported_or_within_tolerance_is_not_reordered(self):
        lines, layer = self.wall_layer()
        for support in (box(-1, -1, 105, 101), box(-1, -1, 103.98, 101)):
            self.assertEqual(self.wave._overhanging_wall_runs(layer, support, 0.05, lines), [])

    def test_absolute_e_and_missing_support_stay_excluded(self):
        lines, layer = self.wall_layer(False)
        self.assertEqual(self.wave._overhanging_wall_runs(layer, box(-1, -1, 100, 101), 0.05, lines), [])
        lines, layer = self.wall_layer()
        for support in (None, Polygon()):
            self.assertEqual(self.wave._overhanging_wall_runs(layer, support, 0.05, lines), [])

    def test_real_rewrite_puts_boundary_after_all_waves_once(self):
        source = small_overhang_print()
        cfg = {**self.wave._DEFAULTS, "_lh": 0.3, "straight_bridge_span": 0}
        ordered, stats = self.wave._gcode_wave_rewrite(source, cfg)
        unchanged_order, other = self.wave._gcode_wave_rewrite(source, {**cfg, "wall_last": False})
        self.assertGreater(stats.get("wave_layers", 0), 0, stats)
        self.assertGreater(stats.get("walls_moved", 0), 0, stats)
        last_wave = ordered.rindex("; ==== WAVE OVERHANG END ====")
        for line in source.splitlines():
            if "; upper boundary" in line:
                self.assertEqual(ordered.count(line), 1)
                self.assertGreater(ordered.index(line), last_wave)
        self.assertNotIn(self.wave.WALL_LAST_BEGIN, unchanged_order)
        for key in ("removed_moves", "kept_fragments", "replaced_sections", "wave_layers"):
            self.assertEqual(stats[key], other[key], key)
        total_e = lambda text: sum(float(s) for s in re.findall(r"(?m)^G[123][^;\n]*\bE(-?[\d.]+)", text))
        self.assertAlmostEqual(total_e(ordered), total_e(unchanged_order), places=8)
        self.assertEqual(self.wave._gcode_wave_rewrite(ordered, cfg)[0], ordered)
        z_line = "G1 Z0.6 F9000 ; layer Z must stay here"
        self.assertLess(ordered.index(z_line), ordered.index("WAVE OVERHANG BEGIN"))

    def test_speed_only_moves_keep_the_whole_wall_feature_together(self):
        lines = [";Z:0.6\n", ";TYPE:Outer wall\n", "G1 F1800\n",
                 "G1 X2 Y0 E0.1\n", "G1 F600\n", ";TYPE:Overhang wall\n",
                 "G1 X3 Y0 E0.1\n", "G1 F900\n", ";TYPE:Outer wall\n",
                 "G1 X2 Y1 E0.1\n"]
        def wall(index, a, b):
            return {"line": index, "a": a, "b": b, "geom": LineString([a, b])}

        def select(walls, text):
            return self.wave._overhanging_wall_runs(
                {"walls": walls}, box(-10, -10, -5, -5), 0.05, text)

        walls = [wall(3, (1, 0), (2, 0)), wall(6, (2, 0), (3, 0)),
                 wall(9, (3, 0), (2, 1))]
        runs = select(walls, lines)
        self.assertEqual([[m["line"] for m in run] for run in runs], [[3, 6, 9]])
        for command in ("", "; harmless comment", ";WIDTH:0.6", "G0 F2700",
                        "G1F600 ; E-1.25 X9 is only a comment", "M205 X7.5 Y7.5",
                        "M73 P22 R7"):
            with self.subTest(inert=command):
                inert = list(lines)
                inert[4] = command + "\n"
                self.assertEqual(len(select(walls, inert)), 1)
        for command in ("G1 X3 Y1", "G0 X3 Y1", "G1 E-1.25", "G1 Z0.9",
                        "G1 X2.5 Y0 E0.1", "G92 E0", "M104 S205",
                        ";TYPE:Bridge", ";WIPE_START", ";Z:0.9",
                        ";LAYER_CHANGE", ";LAYER_HEIGHT:0.3"):
            with self.subTest(command=command):
                boundaries = list(lines)
                boundaries[4] = command + "\n"
                self.assertEqual(len(select(walls, boundaries)), 2)
        adjacent = ["G1 X1 Y0 E0.1\n", ";LAYER_CHANGE\n", "G1 X2 Y0 E0.1\n"]
        self.assertEqual(len(select([wall(0, (0, 0), (1, 0)),
                                     wall(2, (1, 0), (2, 0))], adjacent)), 2)

    def test_real_rewrite_keeps_speed_segmented_wall_and_retract_handoff(self):
        source = variable_feed_overhang_print()
        cfg = {**self.wave._DEFAULTS, "_lh": 0.3, "straight_bridge_span": 0}
        result, stats = self.wave._gcode_wave_rewrite(source, cfg)
        baseline, _ = self.wave._gcode_wave_rewrite(source, {**cfg, "wall_last": False})
        self.assertGreater(stats.get("walls_moved", 0), 0, stats)
        self.assertEqual(result.count(self.wave.WALL_LAST_BEGIN), 1)
        self.assertLess(result.rindex("; ==== WAVE OVERHANG END ===="),
                        result.index(self.wave.WALL_LAST_BEGIN))
        start = result.index(self.wave.WALL_LAST_BEGIN)
        end = result.index(self.wave.WALL_LAST_END, start)
        moved = result[start:end]
        for line in source.splitlines():
            if "; upper boundary" in line:
                self.assertEqual(result.count(line), 1)
                self.assertIn(line, moved)
        self.assertIn(";WIPE_START\nG1 F600\nG1 X20.85 Y20.25\n;WIPE_END", moved)
        # The old seam must not strand a prime/retract/wipe cycle. The next
        # bridge's own unretract stays in place; the wall's plumbing moves.
        old_seam = result[result.index("; wave-overhangs moved this overhanging wall"):
                          result.index("; ==== WAVE OVERHANG BEGIN ====")]
        self.assertEqual(old_seam.count("E1.25"), 1)
        self.assertNotIn("E-1.25", old_seam)
        self.assertNotIn("WIPE_START", old_seam)
        self.assertEqual(self.wave._gcode_wave_rewrite(result, cfg)[0], result)
        for marker in (";Z:0.6", "G1 Z0.6 F9000 ; layer Z must stay here"):
            self.assertLess(result.index(marker), result.index("WAVE OVERHANG BEGIN"))
        # Every depositing move retains its geometry/Z/E/feed/retract state:
        # no deposition while retracted, duplicate prime, or stranded retract.
        self.assertEqual(relative_extrusion_state(self.wave, result),
                         relative_extrusion_state(self.wave, baseline))
        total_e = lambda text: sum(float(s) for s in re.findall(r"(?m)^G[123][^;\n]*\bE(-?[\d.]+)", text))
        self.assertAlmostEqual(total_e(result), total_e(baseline), places=8)

    def test_wall_scaffolding_does_not_absorb_layer_z(self):
        lines = [";Z:0.6\n", "G1 Z0.6 F9000\n", ";TYPE:Outer wall\n",
                 "G0 X0 Y0\n", "G1 X1 Y0 E0.1 F1800\n", ";TYPE:Bridge\n"]
        start, end = self.wave._scaffolding_span(lines, 4, 4)
        self.assertGreater(start, 1)
        self.assertEqual(end, 4)
        for marker in (";Z:0.6\n", ";LAYER_CHANGE\n"):
            lines = [marker, ";TYPE:Outer wall\n", "G0 X0 Y0\n", "G1 X1 Y0 E0.1\n"]
            self.assertEqual(self.wave._scaffolding_span(lines, 3, 3), (1, 3))

    def test_wall_scaffolding_rejects_unbalanced_retract(self):
        lines = ["G1 E1.75 F3600\n", "G1 X1 Y0 E0.1\n", ";TYPE:Bridge\n"]
        self.assertEqual(self.wave._scaffolding_span(lines, 1, 1), (1, 1))
        run = [{"line": 1, "a": (0, 0), "b": (1, 0), "feed": 3600}]
        replacements = {}
        moved, count = self.wave._relocate_wall_runs(lines, [run], replacements, 6000, 10, (5, 5))
        self.assertEqual((moved, count, replacements), ([], 0, {}))

    def test_relocation_restores_feed_at_both_handoffs(self):
        lines = [";TYPE:Outer wall\n", "G0 X0 Y0 F6000\n",
                 "G1 X1 Y0 E0.1 F1800\n", ";TYPE:Bridge\n"]
        run = [{"line": 2, "a": (0, 0), "b": (1, 0), "feed": 1800}]
        replacements = {}
        moved, count = self.wave._relocate_wall_runs(
            lines, [run], replacements, 6000, 10, (5, 5), resume_feed=1200)
        self.assertGreater(count, 0)
        self.assertEqual(replacements[0][-1].strip(), "G1 F1800")
        self.assertEqual(moved[-1].strip(), "G1 F1200")

    def test_wall_scaffolding_never_absorbs_next_layer_marker_or_z_hop(self):
        for marker in (";Z:0.9\n", ";LAYER_CHANGE\n", "G0 Z0.9\n"):
            lines = ["G0 X0 Y0\n", "G1 X1 Y0 E0.1 F1800\n", marker, "G0 Z0.9\n"]
            with self.subTest(marker=marker):
                self.assertEqual(self.wave._scaffolding_span(lines, 1, 1), (0, 1))
        lines = ["G0 X0 Y0 Z0.6\n", ";TYPE:Outer wall\n", "G1 X1 Y0 E0.1\n"]
        self.assertEqual(self.wave._scaffolding_span(lines, 2, 2), (1, 2))

    def test_relocation_preserves_final_wipe_xy_and_overlap_guards(self):
        lines = ["G1 E-1.75 F1800\n", "G1 E1.75 F3600\n",
                 "G1 X1 Y0 E0.1 F1800\n", "G1 E-1.75 F1800\n",
                 ";WIPE_START\n", "G0 X9 Y2\n", ";WIPE_END\n",
                 ";TYPE:Bridge\n", "G1 E1.75 F3600\n", "G1 X5 Y5 E0.1 F1200\n"]
        run = [{"line": 2, "a": (0, 0), "b": (1, 0), "feed": 1800}]
        replacements = {}
        moved, count = self.wave._relocate_wall_runs(lines, [run], replacements, 6000, 9, (5, 5))
        self.assertGreater(count, 0)
        self.assertIn("G0 F6000 X9.000 Y2.000\n", replacements[1])
        for line in lines[1:7]:
            self.assertEqual(moved.count(line), 1)
        for after_line, pending in ((1, {}), (9, {5: ["already rewritten\n"]})):
            before = dict(pending)
            self.assertEqual(self.wave._relocate_wall_runs(
                lines, [run], pending, 6000, after_line, (5, 5)), ([], 0))
            self.assertEqual(pending, before)

    def test_handoff_state_scan_tracks_resets_modes_and_unknown_retractions(self):
        lines = ["M83", "G1 F1200 E-1.5", "G92 E0", "G1 E1.5", "M82",
                 "G92 E10", "G1 E9", "G1 E10", "M83", "G1 E-1.5",
                 "G1 X1 Y0 E0.5", "G1 E1"]
        states = self.wave._wall_handoff_states(lines, set(range(len(lines) + 1)))
        for index, mode, debt in ((2, True, 1.5), (3, True, 1.5), (4, True, 0),
                                  (6, False, 0), (7, False, 1), (8, False, 0),
                                  (10, True, 1.5), (11, True, 1), (12, True, 0)):
            self.assertEqual(states[index], (mode, debt, 1200))
        for command in ("G10", "G11", "T1"):
            with self.subTest(command=command):
                self.assertIsNone(self.wave._wall_handoff_states(
                    lines + [command], {13})[13][1])

    def test_relocation_proves_retract_state_and_preserves_source_entry_feed(self):
        lines = ["M83\n", "G1 E-1.75 F1500\n", ";TYPE:Outer wall\n",
                 "G0 X0 Y0\n", "G1 E1.75\n", "G1 X1 Y0 E0.1 F1800\n",
                 "G1 E-1.75 F1400\n", ";WIPE_START\n", "G0 X9 Y2\n",
                 ";WIPE_END\n", ";TYPE:Bridge\n", "G1 E1.75 F2600\n",
                 "G1 X5 Y5 E0.1 F1200\n"]
        run = [{"line": 5, "a": (0, 0), "b": (1, 0), "feed": 1800}]
        replacements = {}
        moved, count = self.wave._relocate_wall_runs(
            lines, [run], replacements, 6000, 12, (5, 5), resume_feed=1200)
        self.assertGreater(count, 0)
        self.assertEqual(self.wave._last_feedrate(moved[:moved.index(lines[4])]), 1500)
        result = "".join(s for i, line in enumerate(lines)
                         for s in replacements.get(i, [line])) + "".join(moved)
        self.assertEqual(relative_extrusion_state(self.wave, result),
                         relative_extrusion_state(self.wave, "".join(lines)))
        for index, command in ((0, "G10"), (0, "G11"), (0, "T1"),
                               (1, "G1 E-1.0 F1500"), (1, "G1 E-1.75"),
                               (4, "G1 E1.5"), (6, "G1 X1.1 E-1.75 F1400"),
                               (10, "M82")):
            with self.subTest(index=index, command=command):
                unsafe = list(lines)
                unsafe[index] = command + "\n"
                pending = {99: ["unrelated replacement\n"]}
                self.assertEqual(self.wave._relocate_wall_runs(
                    unsafe, [run], pending, 6000, 12, (5, 5)), ([], 0))
                self.assertEqual(pending, {99: ["unrelated replacement\n"]})
        for state in ((True, float("inf"), 1500), (True, None, 1500),
                      (False, 1.75, 1500)):
            self.assertIsNone(self.wave._wall_retract_plan(lines, 2, 9, 5, 5, state))

    def test_retract_plan_refuses_unsafe_feeds_and_nonfinite_amounts(self):
        lines = ["G1 E1.75 F2600", "G1 X1 Y0 E0.1 F1800", "G1 E-1.75 F1400"]
        self.assertEqual(self.wave._wall_retract_plan(lines, 0, 2, 1, 1, (True, 1.75, 1500)),
                         (1.75, 1400, 2600))
        for index, command in ((0, "G1 E1.75 F0"), (2, "G1 E-1.75 F-2"),
                               (0, "G1 E1.75 F" + "9" * 400),
                               (0, "G1 E" + "9" * 400 + " F2600"),
                               (2, "G1 E-0.5 F1400"), (2, "G1 E1.75 F1400"),
                               (0, "G1 E-1.75 F2600"), (0, "G1 E0 F2600")):
            with self.subTest(index=index, command=command[:30]):
                unsafe = list(lines)
                unsafe[index] = command
                self.assertIsNone(self.wave._wall_retract_plan(
                    unsafe, 0, 2, 1, 1, (True, 1.75, 1500)))
        lines[0] = "G1 E1.75"
        self.assertIsNone(self.wave._wall_retract_plan(lines, 0, 2, 1, 1, (True, 1.75, None)))

    def test_retraction_handoffs_carry_between_whole_wall_features(self):
        lines = ["M83\n", ";Z:0.6\n", "G1 Z0.6 F6500\n", "G0 X0 Y0\n"]
        runs = []
        for x, retract in ((90, 0.75), (60, 1.25), (30, 0)):
            if retract:
                lines.append(f"G1 E{-retract} F1400\n")
            lines += [f"G0 X{x} Y0 Z0.6 F6500\n", ";TYPE:Outer wall\n"]
            if retract:
                lines.append(f"G1 E{retract} F2600\n")
            runs.append([{"line": len(lines), "a": (x, 0), "b": (x + 2, 0), "feed": 1800}])
            lines.append(f"G1 X{x + 2} Y0 E0.1 F1800\n")
            if retract:
                lines.append(f"G1 E{-retract} F1400\n")
            lines += [";WIPE_START\n", f"G0 X{x + 1} Y0.2 F6500\n",
                      ";WIPE_END\n", ";TYPE:Bridge\n"]
            if retract:
                lines.append(f"G1 E{retract} F2600\n")
            lines.append(f"G1 X{x} Y5 E0.15 F1200\n")
        replacements = {}
        moved, count = self.wave._relocate_wall_runs(
            lines, runs, replacements, 6500, len(lines) - 1, (30, 5), resume_feed=1200)
        self.assertGreater(count, 0)
        self.assertEqual(moved.count(self.wave.WALL_LAST_BEGIN + "\n"), 3)
        joined = "".join(moved)
        self.assertLess(joined.index("G1 X32 Y0 E0.1"), joined.index("G1 X62 Y0 E0.1"))
        self.assertLess(joined.index("G1 X62 Y0 E0.1"), joined.index("G1 X92 Y0 E0.1"))
        self.assertIn("E-1.25 ; wave-overhangs wall handoff retract", joined)
        self.assertIn("E0.5 ; wave-overhangs wall handoff prime", joined)
        self.assertIn("E0.75 ; wave-overhangs wall handoff prime", joined)
        result = "".join(s for i, line in enumerate(lines)
                         for s in replacements.get(i, [line])) + joined
        self.assertEqual(relative_extrusion_state(self.wave, result),
                         relative_extrusion_state(self.wave, "".join(lines)))
        # The final prime happens at the resume point, before F is returned.
        self.assertTrue(joined.endswith(
            "G0 F6500 X30.000 Y5.000\nG1 F2600 E0.75 ; wave-overhangs wall handoff prime\n"
            "G1 F1200\n"))

    def test_collapse_preserves_travel_and_following_modal_feed(self):
        lines = ["; wave-overhangs replaced covered bridge move 1\n",
                 "G0 X1 Y1 F6000\n", "G1 F1800\n", "G0 X2 Y2\n",
                 "G1 F1200\n", "G1 X3 Y2 E0.1\n"]
        collapsed = self.wave._collapse_wave_travels(lines)
        travels = [line for line in collapsed if line.startswith("G0")]
        self.assertEqual(len(travels), 1)
        self.assertEqual(self.wave._gwords(travels[0]), {"G": 0, "X": 2, "Y": 2, "F": 1800})
        self.assertEqual(self.wave._last_feedrate(collapsed), 1200)
        self.assertIn("G1 X3 Y2 E0.1\n", collapsed)
        # An identical final travel must use its LAST occurrence's state.
        repeated = lines[:2] + ["G1 F1700\n", "G0 X1 Y1 F6000\n", "G1 F1100\n"]
        self.assertEqual(self.wave._last_feedrate(self.wave._collapse_wave_travels(repeated)), 1100)
        self.assertEqual(self.wave._collapse_wave_travels(lines[1:]), lines[1:])

    def test_primary_fan_restore_tracks_off_and_ignores_other_channels(self):
        for commands, expected in ((["M106 S51", "M106 P1 S255"], 51),
                                   (["M106 S51", "M107"], 0),
                                   (["M106 S51", "M107 P1"], 51),
                                   (["m106s+51", "M106 P2 S255"], 51)):
            lines = [";Z:0.6", ";TYPE:Bridge"] + commands + ["G0 X0 Y0", "G1 X1 Y0 E0.1"]
            layer = self.wave._parse_gcode_geometry(lines)[0]
            with self.subTest(commands=commands):
                self.assertEqual(layer["sections"][0]["fan"], expected)

    def test_auto_and_explicit_requests_are_limited_without_editing_config(self):
        for profile in ({}, {"bridge_fan_speed": "100"}, {"bridge_fan_speed": "80"}):
            cfg = {**self.wave._DEFAULTS}
            result = self.wave._resolve_autos(cfg, 0.4, profile)
            self.assertEqual(result["fan"], 0.5)
            self.assertIn("50%", result["_auto_notes"]["fan"])
            self.assertEqual(cfg["fan"], "auto")
        for requested, expected in [(0, 0), (0.2, 0.2), (0.5, 0.5), (1.0, 0.5)]:
            cfg = {**self.wave._DEFAULTS, "fan": requested}
            self.assertEqual(self.wave._resolve_autos(cfg, 0.4, {})["fan"], expected)
            self.assertEqual(cfg["fan"], requested)

    def test_emitter_limits_both_wave_and_restored_fan(self):
        cfg = {**self.wave._DEFAULTS}
        swcfg = self.wave._wave_config(cfg, 0.2)
        swcfg.fan = 1.0
        block = self.wave._emit_wave_gcode([[(0, 0), (2, 0)]], 0.6, swcfg, cfg, restore_fan=255)
        fans = [int(line.split("S", 1)[1]) for line in block if line.startswith("M106 S")]
        self.assertEqual(fans, [127, 127])
        swcfg.fan = 0.2
        block = self.wave._emit_wave_gcode([[(0, 0), (2, 0)]], 0.6, swcfg, cfg, restore_fan=0)
        self.assertIn("M106 S51", block)
        self.assertIn("M106 S0", block)

    def test_pwm_rounding_cannot_cross_half_scale(self):
        for value in (0.5, 0.50001, 1, 10, 1e308):
            self.assertEqual(self.wave._part_fan_pwm(value), 127)
        self.assertEqual(self.wave._part_fan_pwm(0), 0)
        self.assertLessEqual(self.wave._part_fan_pwm(-1), 127)
        for value in (float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                self.wave._part_fan_pwm(value)

    def test_file_limiter_handles_implicit_compact_and_primary_channel(self):
        source = "M106\nM106S255\n m106 p0 s200 ; primary\nM106 S127.5 P0\nM106 P0\n"
        limited, count = self.wave._limit_part_fan_gcode(source)
        self.assertEqual(count, 5)
        self.assertEqual(limited, "M106 S127\nM106S127\n m106 p0 s127 ; primary\nM106 S127 P0\nM106 S127 P0\n")
        self.assertEqual(self.wave._limit_part_fan_gcode(limited), (limited, 0))

    def test_file_limiter_preserves_lower_values_comments_and_other_fans(self):
        source = "; M106 S255\nM106 S100 ; S255 is a comment\nM106 P1 S255\nM106 P2 S255\nM107\nG1 X1 E0.2\n"
        self.assertEqual(self.wave._limit_part_fan_gcode(source), (source, 0))
        self.assertEqual(self.wave._limit_part_fan_gcode("M106 S-1\n"), ("M106 S0\n", 1))

    def test_ambiguous_primary_fan_commands_cannot_bypass_limit(self):
        for command in ("M106 Snan\n", "M106 S255 S10\n", "M106 S255*42\n",
                        "N9 M106 S255\n", "M106 S1e3\n", "M106*42\n",
                        "M106 P0.1 S255\n", "M106 P-1 S255\n", "M106.1 S255\n"):
            with self.subTest(command=command), self.assertRaises(ValueError):
                self.wave._limit_part_fan_gcode(command)

    def test_file_limiter_preserves_line_endings_and_handles_zero_padding(self):
        for source in ("M0106 S255\r\n", "\ufeffM106 S255\r\n", "M106 S255 ; comment"):
            limited, count = self.wave._limit_part_fan_gcode(source)
            self.assertEqual(count, 1)
            self.assertEqual(limited, source.replace("255", "127"))
        self.assertEqual(self.wave._limit_part_fan_gcode("M1060 S255\n"), ("M1060 S255\n", 0))

    def test_splice_caps_original_commands_even_without_waves(self):
        path = Path(self.temp.name) / "plain.gcode"
        path.write_text("M83\nM106 S255\nG1 X1 E0.2\nM106 S200\n")
        log = {}
        self.assertEqual(self.wave._splice_gcode(str(path), dict(self.wave._DEFAULTS), log), 0)
        expected = "M83\nM106 S127\nG1 X1 E0.2\nM106 S127\n"
        self.assertEqual(path.read_text(), expected)
        self.assertEqual(log["fan_commands_limited"], 2)
        self.assertNotIn(self.wave.WAVE_STAMP, expected)
        self.wave._splice_gcode(str(path), dict(self.wave._DEFAULTS), {})
        self.assertEqual(path.read_text(), expected)

    def test_splice_caps_already_waved_and_geometry_timeout_outputs(self):
        for source, timeout in [(self.wave.WAVE_STAMP + "M106 S255\n", False), ("M106 S255\n", True)]:
            path = Path(self.temp.name) / "existing.gcode"
            path.write_text(source)
            log = {}
            if timeout:
                callback = lambda text, cfg: (text, {"wave_layers": 0, "timed_out": True})
                with mock.patch.object(self.wave, "_gcode_wave_rewrite", side_effect=callback):
                    self.wave._splice_gcode(str(path), dict(self.wave._DEFAULTS), log)
                self.assertTrue(log["timed_out"])
            else:
                self.wave._splice_gcode(str(path), dict(self.wave._DEFAULTS), log)
            self.assertIn("M106 S127", path.read_text())
            self.assertNotIn("M106 S255", path.read_text())

    def capability(self, cfg):
        cap = self.wave.WaveOverhangsSlicing()
        cap.get_config = lambda: json.dumps(cfg)
        cap.save_config = lambda raw: True
        return cap

    def test_export_error_is_not_reported_as_successful_uncapped_skip(self):
        cap = self.capability(self.wave._DEFAULTS)
        path = Path(self.temp.name) / "broken.gcode"
        source = "M106 S255 S10\n"
        path.write_text(source)
        with mock.patch.object(self.wave, "_migrate_once"), mock.patch.object(self.wave, "_write_log"):
            result = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(path)))
        self.assertFalse(result.ok, result)
        self.assertEqual(result.kind, self.orca.PluginResult.RecoverableError)
        self.assertIn("stopped", result.message.lower())
        self.assertEqual(path.read_text(), source)

    def test_enabled_export_refuses_missing_dependencies_and_write_failure(self):
        path = Path(self.temp.name) / "no-write.gcode"
        source = "M106 S255\n"
        path.write_text(source)
        cap = self.capability(self.wave._DEFAULTS)
        ctx = fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(path))
        with mock.patch.object(self.wave, "_migrate_once"), mock.patch.object(self.wave, "np", None):
            result = cap.execute(ctx)
        self.assertFalse(result.ok)
        self.assertEqual(path.read_text(), source)
        original_open = open

        def denied_write(file, mode="r", **kwargs):
            if "w" in mode:
                raise PermissionError("test: writing refused")
            return original_open(file, mode, **kwargs)

        with mock.patch.object(self.wave, "_migrate_once"), mock.patch.object(self.wave, "_write_log"), \
                mock.patch.object(self.wave, "open", side_effect=denied_write, create=True):
            result = cap.execute(ctx)
        self.assertFalse(result.ok)
        self.assertIn("stopped", result.message)
        self.assertEqual(path.read_text(), source)

    def test_real_splice_limits_all_primary_commands_and_is_idempotent(self):
        path = Path(self.temp.name) / "waved.gcode"
        path.write_text(small_overhang_print())
        cfg = {**self.wave._DEFAULTS, "straight_bridge_span": 0}
        log = {}
        self.assertGreater(self.wave._splice_gcode(str(path), cfg, log), 0)
        output = path.read_text()
        self.assertIn("WAVE OVERHANG BEGIN", output)
        self.assertIn(self.wave.WALL_LAST_BEGIN, output)
        self.assertNotIn("M106 S255", output)
        self.assertEqual(self.wave._limit_part_fan_gcode(output), (output, 0))
        self.assertGreater(log["fan_commands_limited"], 0)
        self.wave._splice_gcode(str(path), cfg, {})
        self.assertEqual(path.read_text(), output)

    def test_disabled_plugin_does_not_claim_or_apply_a_fan_limit(self):
        cap = self.capability({**self.wave._DEFAULTS, "enabled": False})
        path = Path(self.temp.name) / "disabled.gcode"
        source = "M106 S255\n"
        path.write_text(source)
        with mock.patch.object(self.wave, "_migrate_once"):
            result = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(path)))
        self.assertTrue(result.ok)
        self.assertIn("disabled", result.message)
        self.assertEqual(path.read_text(), source)


if __name__ == "__main__":
    unittest.main()
