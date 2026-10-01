#!/usr/bin/env python3
"""Actually run the Unlayered Infill *plugin* — not just its engine.

    python3 tests/test_plugin_runtime.py

Until now nothing in `plugins/` had ever been executed: `test_installer.py`
checks packaging and `test_post_script.py` checks the standalone tool. The
plugin wrapper — config handling, the step filter, the error paths, the log —
was unproven code.

This loads the plugin against `fake_orca` (a minimal stand-in built from the
documented API in docs/ORCA-PLUGIN-FACTS.md) and drives it the way Orca
would. It cannot prove the plugin works *inside OrcaSlicer* — only a real
slice can do that — but it does prove the plugin's own logic is sound and
that it writes the log the owner asked for.
"""
import importlib.util
import math
import os
import pathlib
import re
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
PLUGIN = REPO / "plugins" / "unlayered-infill" / "unlayered_infill_orca.py"
WAVE_PLUGIN = REPO / "plugins" / "wave-overhangs" / "wave_overhangs_orca.py"
sys.path.insert(0, str(HERE))

import fake_orca  # noqa: E402

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
    return cond


def fake_cube(**kw):
    """Reuse the synthetic sliced cube from the engine test."""
    src = (HERE / "test_post_script.py").read_text(encoding="utf-8")
    ns = {"math": math}
    exec(src[src.index("def fake_cube"):src.index("def load_tool")], ns)
    return ns["fake_cube"](**kw)


def load_plugin(log_dir):
    """Fresh import of the plugin with the log pointed somewhere we can read."""
    os.environ["ORCA_PLUGIN_LOG_DIR"] = str(log_dir)
    orca = fake_orca.install()
    sys.modules.pop("unlayered_infill_orca", None)
    sys.modules.pop("nonplanar_core", None)
    spec = importlib.util.spec_from_file_location("unlayered_infill_orca", PLUGIN)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["unlayered_infill_orca"] = mod
    spec.loader.exec_module(mod)
    return mod, orca


def read_log(log_dir):
    p = pathlib.Path(log_dir) / "orca-plugins.log"
    return p.read_text(encoding="utf-8") if p.exists() else ""


with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    logs = tmp / "Downloads"
    logs.mkdir()

    # ----------------------------------------------------------------------
    # 1. it imports, registers its capabilities, and says so in the log
    # ----------------------------------------------------------------------
    try:
        plugin, orca = load_plugin(logs)
    except Exception as e:
        print(f"FAILED: the plugin could not even be imported: "
              f"{type(e).__name__}: {e}")
        raise

    check(plugin.npc is not None,
          "the inlined engine failed to exec at import — the plugin is dead on "
          "arrival inside Orca too")

    # Orca instantiates the @orca.plugin class and calls register_capabilities();
    # the decorator alone does not register anything.
    check(len(orca.PLUGINS) == 1,
          f"expected exactly one @orca.plugin class, got {len(orca.PLUGINS)}")
    orca.PLUGINS[0]().register_capabilities()

    names = [c().get_name() for c in orca.REGISTERED]
    check(len(orca.REGISTERED) == 2,
          f"expected 2 registered capabilities, got {len(orca.REGISTERED)}: {names}")
    check("Unlayered Infill" in names and "Unlayered Infill - Check setup" in names,
          f"capability names changed: {names}. A process preset stores these, so "
          f"renaming one orphans the preset.")
    # Importing must be SILENT on disk (see the audit test below). The
    # "loaded" line appears on the first capability call instead.
    check(read_log(logs) == "",
          f"the plugin wrote to disk during import. Orca's audit hook turns "
          f"that into a permission prompt or a failed load:\n{read_log(logs)}")
    orca.REGISTERED[1]().execute()          # Check setup -> first capability use
    log = read_log(logs)
    check("loaded" in log and plugin.PLUGIN_VERSION in log,
          f"the plugin never logged that it loaded. Log was:\n{log}")
    check("engine ok" in log, "the load line does not report engine health")
    before_n = log.count("loaded")
    orca.REGISTERED[1]().execute()
    check(read_log(logs).count("loaded") == before_n,
          "the 'loaded' line is repeated on every call; it must be once")

    # ----------------------------------------------------------------------
    # 2. the defaults are the ones the owner asked for
    # ----------------------------------------------------------------------
    d = plugin._DEFAULTS
    check(str(d["amplitude"]).endswith("%"),
          f"amplitude default is {d['amplitude']!r}; it must be a percentage of "
          f"layer height, not a fixed mm value")
    check(d["amplitude"] == "200%", f"amplitude default is {d['amplitude']!r}, want '200%'")
    check(d["cell_mm"] == "auto",
          f"cell_mm default is {d['cell_mm']!r}; it must follow the nozzle diameter")
    check(d["frequency"] == 1.5 and d["segment_mm"] == 1.0,
          f"0.3.0 frequency/segment controls drifted: {d}")
    check(d["blend_mm"] == 2.0 and d["full_strength"] is False,
          f"0.3.0 blending/full-strength controls drifted: {d}")
    check(d["log"] is True, "logging must be on by default")

    # 200% of a 0.3 mm layer is 0.6 mm — the owner's own worked example
    amp, desc = plugin.npc.resolve_amplitude("200%", [";HEIGHT:0.300\n"] * 5)
    check(abs(amp - 0.6) < 1e-9,
          f"200% of a 0.3 mm layer should be 0.600 mm, got {amp}")
    check("0.300" in desc and "0.600" in desc,
          f"the amplitude description should show the arithmetic, got {desc!r}")

    # ----------------------------------------------------------------------
    # 3. the column grid follows the nozzle
    # ----------------------------------------------------------------------
    noz = plugin.npc.detect_nozzle_diameter(["; nozzle_diameter = 0.6\n"])
    check(noz == 0.6, f"nozzle detection returned {noz!r}, want 0.6")
    check(plugin.npc.detect_nozzle_diameter(["; nozzle_diameter = 0.4,0.4\n"]) == 0.4,
          "multi-extruder nozzle_diameter (comma separated) was not handled")
    check(plugin.npc.detect_nozzle_diameter(["; nothing here\n"]) is None,
          "nozzle detection should return None when the G-code does not say")
    cell, cdesc = plugin.npc.resolve_cell_mm("auto", ["; nozzle_diameter = 0.6\n"])
    check(cell == 0.6 and "nozzle" in cdesc,
          f"'auto' cell size did not follow the nozzle: {cell} / {cdesc!r}")
    cell, _ = plugin.npc.resolve_cell_mm("auto", ["; no nozzle\n"])
    check(cell == plugin.npc.FALLBACK_CELL_MM,
          "'auto' should fall back to the default when the nozzle is unknown")

    # ----------------------------------------------------------------------
    # 4. steps that are not ours are ignored, but logged once
    # ----------------------------------------------------------------------
    cap = orca.REGISTERED[0]()
    r = cap.execute(fake_orca.Context(fake_orca.Step.posSlice))
    check(r.ok, "a non-target step must succeed, not fail")
    r = cap.execute(fake_orca.Context(fake_orca.Step.posSlice))
    check(r.ok, "second non-target call failed")
    log = read_log(logs)
    check(log.count("posSlice") == 1,
          f"posSlice should be logged exactly once, saw {log.count('posSlice')} "
          f"— an unbounded log would fill the owner's Downloads folder")

    # ----------------------------------------------------------------------
    # 5. the real thing: run the export step on a sliced cube
    # ----------------------------------------------------------------------
    gpath = tmp / "cube.gcode"
    src = fake_cube().replace("; generated by",
                              "; nozzle_diameter = 0.6\n; generated by")
    gpath.write_text(src, encoding="utf-8")

    r = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(gpath)))
    check(r.ok, f"the export step failed: {r!r}")
    after = gpath.read_text(encoding="utf-8")
    check(after != src, "the plugin did not modify the G-code at all")
    check(after.startswith(plugin.npc.MARKER_PREFIX),
          "the rewritten file is not stamped")
    check(re.search(r"\d+ infill move", r.message),
          f"the result message does not report what happened: {r.message!r}")

    log = read_log(logs)
    check("EXPORT STEP RUNNING" in log, "the export step was not logged")
    check("DONE" in log, "the successful result was not logged")
    for want in ("layer height", "nozzle", "grid columns", "amplitude",
                 "moves waved", "largest Z"):
        check(want in log, f"the log is missing {want!r} — "
                           f"it has to be readable on its own")
    check("0.600 mm" in log,
          "the log should show the detected 0.6 mm nozzle")
    check("200% of layer height" in log,
          "the log should show the amplitude as the percentage the owner set")
    check("preview will NOT show" in log or "preview" in log.lower(),
          "the log should warn that the preview never updates")

    # ----------------------------------------------------------------------
    # 6. a second export of the same file is a no-op
    # ----------------------------------------------------------------------
    before = gpath.read_text(encoding="utf-8")
    r = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(gpath)))
    check(r.ok, "the second export call failed")
    check(gpath.read_text(encoding="utf-8") == before,
          "a second export changed the file — displacements would double")
    check("already" in r.message.lower(),
          f"the second run should say it was already applied: {r.message!r}")

    # ----------------------------------------------------------------------
    # 7. absolute E is refused, and the refusal is logged
    # ----------------------------------------------------------------------
    abspath = tmp / "abs.gcode"
    abspath.write_text(fake_cube(relative_e=False), encoding="utf-8")
    sz = abspath.stat().st_size
    r = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(abspath)))
    check(not r.ok, "absolute-E G-code should be refused, not silently accepted")
    check("relative" in r.message.lower(),
          f"the refusal must name the fix: {r.message!r}")
    check(abspath.stat().st_size == sz, "the refused file was modified anyway")
    check("REFUSED" in read_log(logs), "the refusal was not logged")

    # ----------------------------------------------------------------------
    # 8. a missing file is handled, not crashed on
    # ----------------------------------------------------------------------
    r = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess,
                                      str(tmp / "nope.gcode")))
    check(not r.ok, "a missing G-code path should be reported as a failure")

    # ----------------------------------------------------------------------
    # 9. config from the preset overrides the defaults
    # ----------------------------------------------------------------------
    g2 = tmp / "cube2.gcode"
    g2.write_text(src, encoding="utf-8")
    cap2 = orca.REGISTERED[0]({"amplitude": "50%", "frequency": 3.0})
    r = cap2.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(g2)))
    check(r.ok, f"a configured run failed: {r!r}")
    check("50% of layer height" in read_log(logs),
          "a preset override of the amplitude was not honoured or not logged")

    g3 = tmp / "cube3.gcode"
    g3.write_text(src, encoding="utf-8")
    cap3 = orca.REGISTERED[0]({"enabled": False})
    r = cap3.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(g3)))
    check(r.ok and g3.read_text(encoding="utf-8") == src,
          "enabled=false should leave the file alone")

    # ----------------------------------------------------------------------
    # 10. Check setup tells the owner where the log is
    # ----------------------------------------------------------------------
    chk = orca.REGISTERED[1]()
    r = chk.execute()
    check(r.ok, f"Check setup failed: {r!r}")
    check(plugin.PLUGIN_VERSION in r.message,
          "Check setup does not report the running version")
    check("orca-plugins.log" in r.message,
          "Check setup does not tell the owner where the log file is")
    check("Export G-code" in r.message,
          "Check setup does not mention the export-only gotcha")
    check("preview" in r.message.lower(),
          "Check setup does not mention the preview gotcha")

    # ----------------------------------------------------------------------
    # 11. logging can be turned off, and never breaks a print
    # ----------------------------------------------------------------------
    size_before = (logs / "orca-plugins.log").stat().st_size
    g4 = tmp / "cube4.gcode"
    g4.write_text(src, encoding="utf-8")
    cap4 = orca.REGISTERED[0]({"log": False})
    r = cap4.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(g4)))
    check(r.ok and g4.read_text(encoding="utf-8") != src,
          "log=false should still do the work")
    check((logs / "orca-plugins.log").stat().st_size == size_before,
          "log=false still wrote to the log")

    # ----------------------------------------------------------------------
    # 11b. with no override, the log really does land in Downloads
    #      (every check above sets ORCA_PLUGIN_LOG_DIR, so without this the
    #      default path would be completely untested)
    # ----------------------------------------------------------------------
    saved_home = os.environ.get("HOME")
    os.environ.pop("ORCA_PLUGIN_LOG_DIR", None)
    home = tmp / "fakehome"
    (home / "Downloads").mkdir(parents=True)
    os.environ["HOME"] = str(home)
    try:
        lp = pathlib.Path(plugin.log_path())
        check(lp == home / "Downloads" / "orca-plugins.log",
              f"with a Downloads folder present the log must go there; "
              f"log_path() gave {lp}")
        # no Downloads folder (non-English Windows, or Linux): fall back home,
        # never to somewhere the owner will not find
        home2 = tmp / "fakehome2"
        home2.mkdir()
        os.environ["HOME"] = str(home2)
        lp2 = pathlib.Path(plugin.log_path())
        check(lp2 == home2 / "orca-plugins.log",
              f"with no Downloads folder the log should fall back to the home "
              f"directory; got {lp2}")
    finally:
        if saved_home is not None:
            os.environ["HOME"] = saved_home
        os.environ["ORCA_PLUGIN_LOG_DIR"] = str(logs)

    # an unwritable log directory must not break the export
    os.environ["ORCA_PLUGIN_LOG_DIR"] = str(tmp / "does" / "not" / "exist")
    g5 = tmp / "cube5.gcode"
    g5.write_text(src, encoding="utf-8")
    r = cap.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(g5)))
    check(r.ok and g5.read_text(encoding="utf-8") != src,
          "an unwritable log location broke the export — logging must never "
          "be able to ruin a print")
    os.environ["ORCA_PLUGIN_LOG_DIR"] = str(logs)

# --------------------------------------------------------------------------
# 12. Wave Overhangs follows the PDF contract and explains missing deps
# --------------------------------------------------------------------------
# numpy/shapely are deliberately absent in this sandbox. That lets us exercise
# the first-install failure the owner is facing without pretending geometry ran.
with tempfile.TemporaryDirectory() as tmp:
    logs = pathlib.Path(tmp) / "Downloads"
    logs.mkdir()
    os.environ["ORCA_PLUGIN_LOG_DIR"] = str(logs)
    orca = fake_orca.install()
    sys.modules.pop("wave_overhangs_orca", None)
    sys.modules.pop("wave_core", None)
    spec = importlib.util.spec_from_file_location("wave_overhangs_orca", WAVE_PLUGIN)
    wave = importlib.util.module_from_spec(spec)
    sys.modules["wave_overhangs_orca"] = wave
    try:
        spec.loader.exec_module(wave)
    except Exception as e:
        check(False, f"Wave Overhangs could not load without optional deps: {e}")
    else:
        check(read_log(logs) == "",
              "Wave Overhangs wrote during import; Orca's audit hook can block it")
        check(len(orca.PLUGINS) == 1,
              f"Wave Overhangs needs exactly one @orca.plugin package, got {len(orca.PLUGINS)}")
        orca.PLUGINS[0]().register_capabilities()
        names = [c().get_name() for c in orca.REGISTERED]
        check(names == ["Wave Overhangs", "Wave Overhangs - Check setup"],
              f"Wave capability identities changed: {names}")
        check(wave.PLUGIN_VERSION == "0.0.15",
              f"Wave runtime version is {wave.PLUGIN_VERSION}, want 0.0.15")

        # The active Wave implementation is deliberately G-code-only. Its
        # source must not retain the removed slice-object planner, host Polygon
        # conversion, or cross-callback geometry stash.
        wave_source = WAVE_PLUGIN.read_text(encoding="utf-8")
        check("def _parse_gcode_geometry(" in wave_source and
              "actual_z = sec[\"segments\"][0].get(\"z\")" in wave_source,
              "Wave does not derive replacement Z from exported bridge moves")
        check("_PLAN" not in wave_source and
              "_carve_layer" not in wave_source and
              "orca.host.Polygon" not in wave_source,
              "Wave still contains the removed slice-object planning path")
        check("no cross-callback geometry" in wave_source and
              "one transactional G-code pass" in wave_source,
              "Wave source no longer documents its transactional architecture")

        result = orca.REGISTERED[1]().execute()
        check(not result.ok and result.kind == fake_orca.PluginResult.RecoverableError,
              "Check setup must return a recoverable failure when dependencies are absent")
        check("numpy" in result.message and "shapely" in result.message and
              "Fully quit OrcaSlicer (not just close the window)" in result.message and
              "Diagnostics" in result.message,
              f"dependency failure does not give a complete beginner-safe fix: {result.message!r}")
        log = read_log(logs)
        check("Wave Overhangs v0.0.15 loaded" in log and "MISSING" in log,
              f"Wave dependency state was not logged clearly:\n{log}")
        pipeline = orca.REGISTERED[0]()
        result = pipeline.execute(fake_orca.Context(fake_orca.Step.posSlice))
        check(not result.ok and "dependency" in result.message.lower(),
              "Wave pipeline must refuse clearly rather than silently no-op without deps")

if failures:
    print(f"FAILED ({len(failures)})")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)

print("ok -- Unlayered Infill rewrites G-code and logs correctly; Wave "
      "Overhangs registers its PDF-shaped capabilities and reports missing "
      "numpy/shapely as a clear recoverable dependency failure")
