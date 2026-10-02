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
import json
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
          f"— an unbounded log would fill the plugin storage folder")

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
    cap2 = orca.REGISTERED[0]()
    cap2.set_config({"amplitude": "50%", "frequency": 3.0})
    r = cap2.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(g2)))
    check(r.ok, f"a configured run failed: {r!r}")
    check("50% of layer height" in read_log(logs),
          "a preset override of the amplitude was not honoured or not logged")

    g3 = tmp / "cube3.gcode"
    g3.write_text(src, encoding="utf-8")
    cap3 = orca.REGISTERED[0]()
    cap3.set_config({"enabled": False})
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
    check("--- what changed recently ---" in r.message
          and "v" + plugin.PLUGIN_VERSION in r.message,
          "Check setup no longer prints the changelog for the running version "
          "-- that is the only place in Orca the owner can read it")

    # The settings guide: every control explained, in the one place the owner
    # is already looking. Added in 0.4.0, matching Wave Overhangs 0.0.27.
    check("--- what every setting means ---" in r.message,
          "Check setup does not print the settings guide")
    for key in plugin._DEFAULTS:
        check(f"\n{key} = " in r.message,
              f"the settings guide never explains {key!r}")
    guide = r.message[r.message.index("--- what every setting means ---"):]
    check(max(len(ln) for ln in guide.splitlines()) <= 72,
          "a settings-guide line is over 72 columns; Orca's message box clips "
          "long lines rather than reflowing them")
    check("DEFAULTS" in guide,
          "the guide must say it is showing defaults -- a capability cannot "
          "read another capability's config, so calling them 'your values' "
          "would be a lie")
    # ...and it can be silenced once it has been read
    chk.set_config({"settings_guide": False})
    quiet = chk.execute()
    check(quiet.ok and "--- what every setting means ---" not in quiet.message,
          "settings_guide=false does not hide the guide")
    check(plugin.PLUGIN_VERSION in quiet.message,
          "hiding the guide also hid the diagnostics")
    # The owner hit this twice: a config saved by an older release keeps that
    # release's settings, so `pattern`/`shape` never appeared in the panel.
    # The diagnostics must explain it, and must survive settings_guide=false
    # -- it is the part you need precisely when the guide looks wrong.
    check("--- not seeing all the settings? ---" in quiet.message,
          "the check must explain a stale saved config")
    check(str(len(plugin._DEFAULTS)) in quiet.message,
          "the check must state how many settings this build has, so the "
          "user can compare it against what the panel shows")
    check("Restore defaults" in quiet.message,
          "the stale-config explanation must give Orca's documented manual "
          "fix (Config tab -> Restore defaults), not the preset recipe that "
          "was based on the wrong storage model")
    check("process preset" not in quiet.message.lower(),
          "the check still blames the process preset; Orca stores capability "
          "config globally in orca_plugins/config.json "
          "(docs/ORCA-PLUGIN-FACTS.md, Capability configuration)")

    # ----------------------------------------------------------------------
    # 10b. a config saved by an older build gains this build's new settings
    #      ("I can't see shape and pattern in Orca", 2026-10-02)
    # ----------------------------------------------------------------------
    # Exactly what 0.3.4 would have left behind: nine settings, no notes, and
    # two of them changed by the user.
    old_saved = {"enabled": True, "amplitude": "150%", "frequency": 2.5,
                 "segment_mm": 1.0, "cell_mm": "auto", "blend_mm": 2.0,
                 "full_strength": False, "require_relative_e": True,
                 "log": True}
    for cap_cls, label in ((orca.REGISTERED[0], "Unlayered Infill"),
                           (orca.REGISTERED[1], "Unlayered Infill - Check setup")):
        stale = cap_cls()
        stale.set_config(dict(old_saved), version="0.3.4")
        version, migrated = stale.migrate_config_if_needed()
        check(version == "0.3.4",
              "migrate_config_if_needed must report the version that saved it")
        check(stale.saved_configs,
              f"{label}: migration never called save_config, so the panel "
              f"stays stale")
        written = json.loads(stale.saved_configs[-1])
        check(written["amplitude"] == "150%" and written["frequency"] == 2.5,
              f"{label}: migration overwrote values the user had set")
        check(migrated == written, "the returned config is not what was saved")
    # The main capability is the one that must gain the 0.4.0 wave controls.
    main = orca.REGISTERED[0]()
    main.set_config(dict(old_saved), version="0.3.4")
    main.migrate_config_if_needed()
    panel_after = json.loads(main.get_config())
    for key in ("pattern", "wave_angle", "shape", "layer_phase", "max_lift_mm"):
        check(key in panel_after,
              f"{key!r} is still missing from a migrated 0.3.4 config -- this "
              f"is the exact bug the owner reported")
        check("_" + key in panel_after,
              f"the migrated config has no note explaining {key!r}")
    check(plugin._cfg(main)["amplitude"] == "150%",
          "the user's amplitude did not survive migration")

    # Nothing saved yet: Orca already shows get_default_config(), so writing
    # would be noise. A config that is already current must not be rewritten
    # either -- save_config() on every slice would churn Orca's config file.
    fresh = orca.REGISTERED[0]()
    fresh.migrate_config_if_needed()
    check(not fresh.saved_configs,
          "migration wrote a config even though nothing was saved")
    current = orca.REGISTERED[0]()
    current.set_config(plugin.annotated_defaults(), version=plugin.PLUGIN_VERSION)
    current.migrate_config_if_needed()
    check(not current.saved_configs,
          "migration rewrote an already-current config")

    # A key we no longer recognise is kept, not silently deleted.
    kept = orca.REGISTERED[0]()
    kept.set_config(dict(old_saved, some_old_key=7), version="0.3.4")
    kept.migrate_config_if_needed()
    check(json.loads(kept.get_config()).get("some_old_key") == 7,
          "migration threw away a setting the owner had typed")

    # And it can never break a slice: Orca refusing the write, or the host
    # handing back junk, must both be survivable.
    refused = orca.REGISTERED[0]()
    refused.set_config(dict(old_saved), version="0.3.4")
    refused.save_ok = False
    refused.migrate_config_if_needed()
    g_mig = tmp / "cube_migrate.gcode"
    g_mig.write_text(src, encoding="utf-8")
    r = refused.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess,
                                          str(g_mig)))
    check(r.ok, "a refused config save must not fail the export")
    broken = orca.REGISTERED[0]()
    broken.set_config("{not json at all", version="0.3.4")
    broken.migrate_config_if_needed()
    check(True, "unreachable")  # reaching here at all is the assertion

    # The config panel must carry the same notes, so the JSON the owner edits
    # explains itself without opening a README.
    panel = orca.REGISTERED[0]().get_default_config()
    check(panel is not plugin._DEFAULTS,
          "get_default_config handed out the live _DEFAULTS dict to be mutated")
    for key in plugin._DEFAULTS:
        check(key in panel, f"the config panel lost the {key!r} setting")
        check("_" + key in panel, f"the config panel has no note for {key!r}")
    # notes must not come back in as settings
    cap_cfg = orca.REGISTERED[0]()
    cap_cfg.set_config(panel)
    for key, value in plugin._cfg(cap_cfg).items():
        check(not key.startswith("_"),
              f"the note {key!r} was read back as a setting")

    # 0.4.0 wave controls must be reachable from Orca, not just the CLI
    for key in ("pattern", "wave_angle", "shape", "layer_phase", "max_lift_mm"):
        check(key in plugin._DEFAULTS,
              f"the {key!r} wave control is missing from the Orca settings")

    # ----------------------------------------------------------------------
    # 11. logging can be turned off, and never breaks a print
    # ----------------------------------------------------------------------
    size_before = (logs / "orca-plugins.log").stat().st_size
    g4 = tmp / "cube4.gcode"
    g4.write_text(src, encoding="utf-8")
    cap4 = orca.REGISTERED[0]()
    cap4.set_config({"log": False})
    r = cap4.execute(fake_orca.Context(fake_orca.Step.psGCodePostProcess, str(g4)))
    check(r.ok and g4.read_text(encoding="utf-8") != src,
          "log=false should still do the work")
    check((logs / "orca-plugins.log").stat().st_size == size_before,
          "log=false still wrote to the log")

    # ----------------------------------------------------------------------
    # 11b. with no override, the log lands in Orca's plugin storage.
    #      That is the no-prompt path: normal slicing must not ask the owner to
    #      authorize writes to Downloads just so a diagnostic line can be saved.
    # ----------------------------------------------------------------------
    os.environ.pop("ORCA_PLUGIN_LOG_DIR", None)
    storage = tmp / "plugin-storage"
    storage.mkdir()
    os.environ["ORCA_PLUGIN_STORAGE_DIR"] = str(storage)
    try:
        lp = pathlib.Path(plugin.log_path())
        check(lp == storage / "orca-plugins.log",
              f"default log path must use plugin storage to avoid approval "
              f"prompts; log_path() gave {lp}")
        check(pathlib.Path(plugin._state_path()).parent == storage,
              "state diagnostics must also use plugin storage by default")
    finally:
        os.environ.pop("ORCA_PLUGIN_STORAGE_DIR", None)
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
        # A capability name is its identity in Orca: renaming one detaches it
        # from any process preset that already selected it, so this is pinned
        # deliberately and may only change with a user-facing note in the
        # changelog. The script capability was renamed once, in 0.0.27, when
        # it took on the settings guide. Versions must never appear here.
        check(names == ["Wave Overhangs",
                        "Wave Overhangs - Settings guide & check"],
              f"Wave capability identities changed: {names}")
        check(not any(ch.isdigit() for ch in "".join(names)),
              f"a version leaked into a capability name: {names}")
        check(wave.PLUGIN_VERSION == "0.0.33",
              f"Wave runtime version is {wave.PLUGIN_VERSION}, want 0.0.33")

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

        # --- the settings panel must explain itself (0.0.27) ---
        # Orca shows the config as JSON, which cannot carry comments, so the
        # notes are shipped as "_"-prefixed keys. Two things must hold: every
        # setting is explained, and no note can ever be mistaken for a
        # setting on the way back in.
        panel = orca.REGISTERED[0]().get_default_config()
        settings = {k: v for k, v in panel.items() if not k.startswith("_")}
        notes = {k for k in panel if k.startswith("_")}
        check(settings == wave._DEFAULTS,
              "the settings in the panel drifted from _DEFAULTS")
        unexplained = sorted(k for k in wave._DEFAULTS if "_" + k not in notes)
        check(not unexplained,
              f"settings with no explanation in the panel: {unexplained}")
        orphans = sorted(n for n in notes
                         if n != "_READ_ME" and n[1:] not in wave._DEFAULTS)
        check(not orphans, f"notes describing settings that do not exist: {orphans}")
        # Order matters: a note is only useful if it sits above its setting.
        keys = list(panel)
        misplaced = [k for k in wave._DEFAULTS
                     if "_" + k in notes
                     and keys.index("_" + k) != keys.index(k) - 1]
        check(not misplaced, f"notes not directly above their setting: {misplaced}")

        # Feeding the panel straight back must yield exactly the defaults --
        # notes dropped, nothing renamed, nothing lost.
        class _Panel:
            def get_config(self):
                return json.dumps(panel)

        check(wave._cfg(_Panel()) == wave._DEFAULTS,
              "notes leaked into the live config or a setting was lost")
        # And a user who deletes every note must still get a working config.
        class _Stripped:
            def get_config(self):
                return json.dumps(settings)

        check(wave._cfg(_Stripped()) == wave._DEFAULTS,
              "deleting the notes must not change behaviour")

        # --- a config saved by an older build gains this build's settings ---
        # Same bug as Unlayered Infill: the Config panel shows the saved copy,
        # so settings added later are invisible until they are merged in.
        stale = orca.REGISTERED[0]()
        stale.set_config({"enabled": True, "wall_snap": False}, version="0.0.19")
        version, merged = stale.migrate_config_if_needed()
        check(version == "0.0.19",
              "Wave migration must report the version that saved the config")
        check(stale.saved_configs, "Wave migration never called save_config")
        after = json.loads(stale.get_config())
        missing = sorted(k for k in wave._DEFAULTS if k not in after)
        check(not missing, f"settings still missing after migration: {missing}")
        check(after["wall_snap"] is False,
              "Wave migration overwrote a value the user had set")
        check(merged == after, "the returned config is not what was saved")
        unchanged = orca.REGISTERED[0]()
        unchanged.set_config(wave.annotated_defaults(), version=wave.PLUGIN_VERSION)
        unchanged.migrate_config_if_needed()
        check(not unchanged.saved_configs,
              "Wave migration rewrote an already-current config")
        # The arc note is the one people go looking for; it must point at
        # Orca's own setting rather than leaving them hunting in the plugin.
        arc_note = panel["_arc_fitting"]
        check("Print Settings" in arc_note and "Precision" in arc_note
              and "7433" in arc_note,
              "the arc_fitting note must say where Orca's own arc fitting lives")

        # --- the settings guide is readable inside Orca (0.0.27) ---
        # The whole point is not having to open a README on GitHub, so the
        # menu item must print the explanations itself, show the value
        # actually in force, and be switchable off once you know them.
        guide_cap = wave.WaveOverhangsCheck()
        check(guide_cap.get_name() == "Wave Overhangs - Settings guide & check",
              f"menu item is named {guide_cap.get_name()!r}")
        import json as _json
        catalogue = _json.loads((REPO / "plugins.json").read_text(encoding="utf-8"))
        wave_entry = [e for e in catalogue["plugins"] if e["id"] == "wave-overhangs"][0]
        check(guide_cap.get_name() in wave_entry["capabilities"],
              "the capability rename did not reach plugins.json")
        check(guide_cap.get_name() in
              (REPO / "Orca-Plugins.bat").read_bytes().decode("ascii", "replace"),
              "the capability rename did not reach the unified updater's fallback row")
        check(not any(ch.isdigit() for ch in guide_cap.get_name().split("-")[-1]),
              "a capability name must never carry a version")

        guide = wave.settings_guide_lines(dict(wave._DEFAULTS, print_speed=5.0))
        guide_text = "\n".join(guide)
        unexplained = [k for k in wave._DEFAULTS if f"\n{k} = " not in "\n" + guide_text]
        check(not unexplained, f"settings missing from the printed guide: {unexplained}")
        check('print_speed = 5.0   (default "orca")' in guide_text,
              "the guide must show the value in force and flag a changed one")
        # The stock config follows Orca's bridge speed, and the guide must not
        # label the default as if it were a change the user made.
        stock_guide = "\n".join(wave.settings_guide_lines(dict(wave._DEFAULTS)))
        check('print_speed = "orca"' in stock_guide
              and "print_speed = \"orca\"   (default" not in stock_guide,
              "the guide should show the unchanged default without a (default ...) tag")
        longest = max(len(line) for line in guide)
        check(longest <= 72,
              f"guide lines must stay readable in Orca's message box, got {longest}")

        # The toggle itself is pure config reading, so it can be checked here
        # where numpy/shapely are deliberately absent. The full execute()
        # path needs the deps and is covered in tests/test_wave_gcode.py.
        check(guide_cap._want_guide(), "the guide must be on by default")
        for off_value in (False, "false", "off", "no", 0):
            guide_cap.set_config({"settings_guide": off_value})
            check(not guide_cap._want_guide(),
                  f"settings_guide={off_value!r} did not turn the guide off")
        for on_value in (True, "true", 1):
            guide_cap.set_config({"settings_guide": on_value})
            check(guide_cap._want_guide(),
                  f"settings_guide={on_value!r} did not turn the guide on")
        guide_cap.set_config({})
        check(guide_cap._want_guide(), "an empty config must still show the guide")
        # Its own config must be self-explaining too.
        own = wave.WaveOverhangsCheck().get_default_config()
        check("_settings_guide" in own and own["settings_guide"] is True,
              "the guide toggle is missing its note or its default")

        result = orca.REGISTERED[1]().execute()
        check(not result.ok and result.kind == fake_orca.PluginResult.RecoverableError,
              "Check setup must return a recoverable failure when dependencies are absent")
        check("numpy" in result.message and "shapely" in result.message and
              "Fully quit OrcaSlicer (not just close the window)" in result.message and
              "Diagnostics" in result.message,
              f"dependency failure does not give a complete beginner-safe fix: {result.message!r}")
        log = read_log(logs)
        check("Wave Overhangs v0.0.33 loaded" in log and "MISSING" in log,
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

print("ok -- Unlayered Infill rewrites G-code; Wave plugins register their "
      "capabilities and report missing numpy/shapely as clear recoverable "
      "dependency failures")
