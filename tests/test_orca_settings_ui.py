#!/usr/bin/env python3
"""Settings/transaction regressions with host/DOM doubles, NOT native Orca tests."""
import ast
import copy
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests"))
import fake_orca

PLUGINS = [("unlayered-infill", "unlayered_infill_orca", "UnlayeredInfill", "UnlayeredInfillCheck"),
           ("wave-overhangs", "wave_overhangs_orca", "WaveOverhangsSlicing", "WaveOverhangsCheck")]


def load_plugins():
    result = []
    for folder, module, main, check in PLUGINS:
        orca = fake_orca.install()
        path = ROOT / "plugins" / folder / (module + ".py")
        name = "settings_test_" + module
        spec = importlib.util.spec_from_file_location(name, path)
        mod = importlib.util.module_from_spec(spec)
        sys.modules[name] = mod
        spec.loader.exec_module(mod)
        result.append((mod, getattr(mod, main), getattr(mod, check), orca))
    return result


def schema(page):
    return json.loads(re.search(r'<script id="schema" type="application/json">(.*?)</script>', page, re.S)[1])


class Preset:
    def __init__(self, name, values):
        self.name = name
        self.values = dict(values)
        self.is_dirty = False

    def config_value(self, key):
        return self.values.get(key)


class SetupHost:
    """v2.5.1 serialized-value contract, instrumented to forbid other writes."""
    def __init__(self, decision="yes", arcs="1", relative="0"):
        self.process = Preset('0.20 Standard <owner>', {
            "enable_arc_fitting": arcs, "sparse_infill_density": "15%",
            "top_shell_layers": "4", "bottom_shell_layers": "3", "spiral_mode": "0",
            "detect_overhang_wall": "1"})
        self.printer = Preset('My printer', {"use_relative_e_distances": relative})
        self.filament = Preset('My filament', {"filament_flow_ratio": "0.99"})
        self.snapshot_count = 0
        self.confirms, self.transactions, self.events = [], [], []
        self.decision = decision
        self.on_snapshot = self.on_confirm = self.on_begin = None
        self.fail_at = None
        self.result_patch = {}
        self.forbidden_writes = []
        self.bundle = types.SimpleNamespace(current_process_preset=lambda: self.process,
                                            current_printer_preset=lambda: self.printer)
        self.api = types.SimpleNamespace(
            preset_bundle=self.preset_bundle,
            preset=types.SimpleNamespace(process=types.SimpleNamespace(begin_update=self.begin),
                                         printer=types.SimpleNamespace(begin_update=self.forbidden, update=self.forbidden),
                                         filament=types.SimpleNamespace(begin_update=self.forbidden, update=self.forbidden)),
            ui=types.SimpleNamespace(message=self.message))

    def forbidden(self, *args, **kwargs):
        self.forbidden_writes.append((args, kwargs))
        raise AssertionError("Printer/filament writes are forbidden")

    def preset_bundle(self):
        self.snapshot_count += 1
        if self.on_snapshot:
            self.on_snapshot(self.snapshot_count)
        return self.bundle

    def message(self, **kwargs):
        self.events.append("confirm")
        self.confirms.append(kwargs)
        if self.fail_at == "confirm":
            raise PermissionError("confirmation denied")
        if self.on_confirm:
            self.on_confirm()
        return self.decision

    def begin(self, *args, **kwargs):
        self.events.append("begin")
        if args or kwargs:
            raise AssertionError("Never request a preset switch")
        if self.fail_at == "begin":
            raise RuntimeError("selection canceled")
        tx = types.SimpleNamespace(target_name=self.process.name, category="process", selection_changed=False,
                                   staged=[], commits=0)
        self.transactions.append(tx)
        if self.on_begin:
            self.on_begin(tx)

        def stage(key, value):
            self.events.append("set")
            if self.fail_at == "set":
                raise PermissionError("staging denied")
            tx.staged.append((key, value))

        def commit():
            self.events.append("commit")
            tx.commits += 1
            if self.fail_at == "commit":
                raise RuntimeError("validation refused the batch")
            for key, value in tx.staged:
                self.process.values[key] = value
            self.process.is_dirty = True
            result = dict(target_name=tx.target_name, category="process", selection_changed=False,
                          keys=[key for key, _value in tx.staged], dirty=True)
            result.update(self.result_patch)
            return types.SimpleNamespace(**result)
        tx.set, tx.commit = stage, commit
        return tx


class SettingsPageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugins = load_plugins()

    def test_every_control_visible_once_with_groups_units_defaults_and_json(self):
        for mod, main, check, _orca in self.plugins:
            for capability, for_check in ((main, False), (check, True)):
                with self.subTest(plugin=mod._UI_PRODUCT, check=for_check):
                    cap = capability()
                    self.assertIs(cap.has_config_ui(), True)
                    page = cap.get_config_ui()
                    data = schema(page)
                    defaults = mod._CHECK_DEFAULTS if for_check else mod.annotated_defaults()
                    self.assertEqual(data["defaults"], defaults)
                    fields = [f for group in data["groups"] for f in group["fields"]]
                    expected = {k for k in defaults if not k.startswith("_")}
                    self.assertEqual({f["key"] for f in fields}, expected)
                    self.assertEqual(len(fields), len(expected))
                    self.assertTrue(all(f["help"] and f["label"] for f in fields))
                    self.assertTrue(all(f["default"] == defaults[f["key"]] for f in fields))
                    self.assertNotIn("<details", page)
                    self.assertNotIn("window.confirm", page)
                    self.assertNotIn("display: none", page)
                    self.assertIn("JSON fallback", page)
                    self.assertIn("setup fixes run only from the Script action" if for_check else "Schematic only", page)
                    self.assertIn("schema", page)
                    if not for_check:
                        self.assertEqual(len(data["groups"]), len(mod._SECTIONS))
                        self.assertTrue(any(f["unit"] for f in fields))

    def test_unsectioned_new_control_cannot_disappear(self):
        for mod, _main, _check, _orca in self.plugins:
            with self.subTest(plugin=mod._UI_PRODUCT), mock.patch.dict(mod._DEFAULTS, future_setting=17):
                data = schema(mod._config_ui())
                self.assertEqual(data["groups"][-1]["title"], "Other settings")
                self.assertEqual(data["groups"][-1]["fields"][0]["key"], "future_setting")

    def test_html_generation_is_pure_and_schema_cannot_escape_its_script(self):
        payload = '</script><script>bad()</script>&\u2028'
        for mod, main, check, _orca in self.plugins:
            with self.subTest(plugin=mod._UI_PRODUCT), mock.patch.dict(mod._NOTES, _amplitude=payload, _enabled=payload):
                for capability in (main, check):
                    cap = capability()
                    cap.get_config = mock.Mock(side_effect=AssertionError("do not read config here"))
                    cap.save_config = mock.Mock(side_effect=AssertionError("no persistence in HTML getter"))
                    with mock.patch.object(mod, "_log", side_effect=AssertionError("no I/O")), \
                         mock.patch.object(mod, "_setup_snapshot", side_effect=AssertionError("no presets in config UI")):
                        page = cap.get_config_ui()
                    self.assertNotIn(payload, page)
                    self.assertEqual(page.count("</script>"), 2)
                    if capability is main:
                        fields = [f for g in schema(page)["groups"] for f in g["fields"]]
                        self.assertEqual(next(f["help"] for f in fields if f["key"] == "enabled"), payload)

    def test_setup_entry_is_only_the_script_not_the_slicing_callback(self):
        for mod, main, check, _orca in self.plugins:
            tree = ast.parse(Path(mod.__file__).read_text())
            for classname in (main.__name__, check.__name__):
                cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == classname)
                execute = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == "execute")
                source = ast.unparse(execute)
                self.assertEqual(source.count("_setup_assistant_lines()"), int(classname == check.__name__))
                if classname == main.__name__:
                    for forbidden in ("host.ui", "begin_update", "host.preset", "_setup_snapshot"):
                        self.assertNotIn(forbidden, source)
            # Still exactly the existing two registrations and stable identities.
            registration = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name.endswith("Plugin"))
            self.assertEqual(ast.unparse(registration).count("orca.register_capability("), 2)
            self.assertNotRegex(main().get_name(), r"\d+\.\d+")

    def test_actual_javascript_roundtrips_values_handles_scope_and_races(self):
        node = shutil.which("node")
        self.assertIsNotNone(node, "Node.js is required for the settings page JS regressions")
        pages = [{"name": mod._UI_PRODUCT + (" check" if capability is check else " main"),
                  "html": capability().get_config_ui()}
                 for mod, main, check, _orca in self.plugins for capability in (main, check)]
        result = subprocess.run([node, str(ROOT / "tests/orca_settings_dom.cjs")],
                                input=json.dumps(pages), text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("4 shipped settings pages", result.stdout)
        print(result.stdout.strip())

    def test_shared_helpers_are_identical_in_both_independent_entry_files(self):
        shared = (ROOT / "shared/orca/settings_ui.py").read_text().rstrip()
        for mod, _main, _check, _orca in self.plugins:
            source = Path(mod.__file__).read_text()
            deployed = source.split("# --- BEGIN SHARED ORCA SETTINGS UI ---\n")[1].split("# --- END SHARED ORCA SETTINGS UI ---")[0].rstrip()
            self.assertEqual(deployed, shared)


class SetupAssistantTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugins = load_plugins()
        cls.infill = cls.plugins[0][0]
        cls.wave = cls.plugins[1][0]

    def run_setup(self, host, mod=None):
        mod = mod or self.infill
        with mock.patch.object(mod.orca, "host", host.api, create=True):
            return "\n".join(mod._setup_assistant_lines())

    def assert_no_writes(self, host):
        self.assertEqual(sum(tx.commits for tx in host.transactions), 0)
        self.assertEqual(host.forbidden_writes, [])
        self.assertEqual(host.process.values["enable_arc_fitting"], "1")
        self.assertEqual(host.printer.values, {"use_relative_e_distances": "0"})
        self.assertEqual(host.filament.values, {"filament_flow_ratio": "0.99"})

    def test_exact_approved_process_diff_uses_one_unsaved_active_transaction(self):
        host = SetupHost()
        result = self.run_setup(host)
        self.assertEqual(host.events, ["confirm", "begin", "set", "commit"])
        self.assertEqual(host.transactions[0].staged, [("enable_arc_fitting", "0")])
        self.assertEqual(host.process.values["enable_arc_fitting"], "0")
        self.assertTrue(host.process.is_dirty)
        self.assertEqual(host.forbidden_writes, [])
        self.assertIn("UNSAVED", result)
        confirmation = host.confirms[0]
        self.assertEqual(confirmation["buttons"], "yes_no_cancel")
        for phrase in (host.process.name, 'enable_arc_fitting: "1" -> "0"', "entire diff", "read-only", "unsaved"):
            self.assertIn(phrase, confirmation["text"])
        self.assertIn("manual", result)
        self.assertEqual(host.printer.values["use_relative_e_distances"], "0")
        self.assertEqual(host.filament.values["filament_flow_ratio"], "0.99")
        self.assertEqual(host.snapshot_count, 5)

    def test_no_cancel_close_and_non_yes_never_open_a_transaction(self):
        for decision in ("no", "cancel", None, "ok", "YES", True, 1):
            with self.subTest(decision=decision):
                host = SetupHost(decision=decision)
                result = self.run_setup(host)
                self.assertIn("Not approved", result)
                self.assertEqual(host.transactions, [])
                self.assert_no_writes(host)

    def test_missing_older_or_restricted_host_is_advice_only(self):
        for api in (types.SimpleNamespace(), types.SimpleNamespace(preset_bundle=None)):
            host = SetupHost(); host.api = api
            result = self.run_setup(host)
            self.assertIn("advice only", result)
            self.assertIn("Relative E", result)
            self.assert_no_writes(host)
        for missing in ("ui", "preset"):
            host = SetupHost(); delattr(host.api, missing)
            result = self.run_setup(host)
            self.assertIn("Read-only fallback", result)
            self.assertIn('"1" -> "0"', result)
            self.assertEqual(host.transactions, [])
            self.assert_no_writes(host)

    def test_untouched_compatible_or_unreadable_values_are_not_guessed(self):
        for arcs in ("0", "false", None, "unknown"):
            host = SetupHost(arcs=arcs)
            result = self.run_setup(host)
            self.assertIn("No process-setting changes proposed", result)
            self.assertEqual(host.transactions, [])
            self.assertEqual(host.confirms, [])
        host = SetupHost(arcs=True)
        self.assertIn("advice only", self.run_setup(host))
        self.assertEqual(host.transactions, [])

    def test_undahang_does_not_offer_unnecessary_infill_fixes(self):
        host = SetupHost()
        result = self.run_setup(host, self.wave)
        self.assertIn("none is proposed", result)
        self.assertIn("Bridge", result)
        self.assertEqual(host.confirms, [])
        self.assertEqual(host.transactions, [])
        self.assert_no_writes(host)

    def test_profile_or_reviewed_value_changes_during_review_abort_without_switch(self):
        for mutation in (lambda h: setattr(h.process, "name", "other process"),
                         lambda h: h.process.values.update(top_shell_layers="8"),
                         lambda h: setattr(h.process, "is_dirty", True),
                         lambda h: setattr(h.printer, "name", "other printer"),
                         lambda h: h.printer.values.update(use_relative_e_distances="1")):
            host = SetupHost()
            host.on_confirm = lambda: mutation(host)
            result = self.run_setup(host)
            self.assertIn("changed during review", result)
            self.assertEqual(host.transactions, [])
            self.assertEqual(host.process.values["enable_arc_fitting"], "1")

    def test_bound_target_category_selection_or_values_must_still_match(self):
        for mutate in (lambda tx: setattr(tx, "target_name", "other"),
                       lambda tx: setattr(tx, "category", "printer"),
                       lambda tx: setattr(tx, "selection_changed", True)):
            host = SetupHost(); host.on_begin = mutate
            result = self.run_setup(host)
            self.assertIn("Nothing committed", result)
            self.assertEqual(host.transactions[0].staged, [])
            self.assert_no_writes(host)
        host = SetupHost()
        host.on_begin = lambda _tx: host.process.values.update(bottom_shell_layers="7")
        self.assertIn("Nothing committed", self.run_setup(host))
        self.assertEqual(host.transactions[0].staged, [], "A stale bound snapshot must abort before staging")
        self.assert_no_writes(host)

    def test_reviewed_values_are_rechecked_even_after_staging(self):
        host = SetupHost()
        host.on_snapshot = lambda count: host.process.values.update(top_shell_layers="9") if count == 4 else None
        self.assertIn("Nothing committed", self.run_setup(host))
        self.assertEqual(host.transactions[0].staged, [("enable_arc_fitting", "0")])
        self.assert_no_writes(host)

    def test_transaction_errors_cancel_or_audit_refusal_never_retry_or_save(self):
        for phase in ("confirm", "begin", "set", "commit"):
            with self.subTest(phase=phase):
                host = SetupHost(); host.fail_at = phase
                self.assertIn("No retry", self.run_setup(host))
                self.assertLessEqual(len(host.transactions), 1)
                self.assertLessEqual(sum(tx.commits for tx in host.transactions), 1)
                self.assertEqual(host.process.values["enable_arc_fitting"], "1")
                self.assertEqual(host.forbidden_writes, [])
        class AuditDenied(BaseException):
            pass
        host = SetupHost()
        host.api.ui.message = mock.Mock(side_effect=AuditDenied("policy"))
        self.assertIn("No retry", self.run_setup(host))
        self.assertEqual(host.transactions, [])

    def test_only_allowlisted_process_key_value_can_be_staged(self):
        for key, value in (("use_relative_e_distances", "1"), ("enable_arc_fitting", "1"),
                           ("sparse_infill_density", "30%")):
            host = SetupHost()
            with mock.patch.object(self.infill, "_setup_changes", return_value=[(key, "old", value, "test")]):
                result = self.run_setup(host)
            self.assertIn("Unapproved", result)
            self.assertEqual(host.transactions[0].staged, [])
            self.assert_no_writes(host)

    def test_host_result_and_readback_must_confirm_exact_target_and_keys(self):
        for patch in ({"target_name": "other"}, {"category": "printer"}, {"keys": []},
                      {"keys": ["enable_arc_fitting", "unapproved"]}, {"selection_changed": True}):
            host = SetupHost(); host.result_patch = patch
            result = self.run_setup(host)
            self.assertIn("did not confirm the exact result", result)
            self.assertNotIn("changes applied", result)
            self.assertEqual(host.transactions[0].commits, 1)
        host = SetupHost()
        host.on_snapshot = lambda count: host.process.values.update(enable_arc_fitting="1") if count == 5 else None
        self.assertIn("did not confirm the exact result", self.run_setup(host))

    def test_rerun_after_apply_does_not_repeat_prompt_or_commit(self):
        host = SetupHost()
        self.run_setup(host)
        result = self.run_setup(host)
        self.assertIn("No process-setting changes proposed", result)
        self.assertEqual(len(host.confirms), 1)
        self.assertEqual(len(host.transactions), 1)

    def test_printer_introspection_failure_cannot_turn_into_a_printer_fix(self):
        host = SetupHost()
        host.bundle.current_printer_preset = mock.Mock(side_effect=RuntimeError("unavailable"))
        result = self.run_setup(host)
        self.assertIn("Printer (read-only): unavailable", result)
        self.assertIn("manual", result)
        self.assertEqual(host.transactions[0].staged, [("enable_arc_fitting", "0")])
        self.assertEqual(host.forbidden_writes, [])

    def test_script_entry_runs_setup_even_with_the_optional_guide_off(self):
        with tempfile.TemporaryDirectory() as storage, mock.patch.dict(os.environ, ORCA_PLUGIN_STORAGE_DIR=storage):
            for mod, _main, check, _orca in self.plugins:
                with self.subTest(plugin=mod._UI_PRODUCT):
                    cap = check(); cap.set_config({"settings_guide": False})
                    # Wave's report itself does not use geometry, but a missing
                    # dependency normally returns early. Do not pretend this
                    # double loads native numpy/shapely or runs an export.
                    with mock.patch.object(mod, "_setup_assistant_lines", return_value=["SETUP-SENTINEL"]), \
                         mock.patch.multiple(mod, _UI_INFILL=mod._UI_INFILL, **({"np": object(), "shapely": object()} if not mod._UI_INFILL else {})):
                        result = cap.execute()
                    self.assertTrue(result.ok, result.message)
                    self.assertIn("SETUP-SENTINEL", result.message)
                    self.assertNotIn("--- what every setting means ---", result.message)



if __name__ == "__main__":
    unittest.main()
