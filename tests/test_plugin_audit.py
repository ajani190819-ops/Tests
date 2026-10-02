#!/usr/bin/env python3
"""Import each plugin under a HOSTILE audit hook, the way OrcaSlicer does.

    python3 tests/test_plugin_audit.py

WHY THIS EXISTS
  OrcaSlicer runs plugins inside an embedded CPython with a `PluginAuditManager`
  installed as a CPython audit hook (`sys.addaudithook`). It "categorizes
  filesystem/network/process events, prompts the user, and persists granted
  permissions per plugin" — see docs/ORCA-PLUGIN-FACTS.md.

  That makes any filesystem write during module import dangerous: it happens
  inside PluginLoader's load, outside the per-call audit scope. At best the
  owner gets a permission prompt in the middle of installing; at worst the
  load fails and the plugin "won't install".

  v0.3.0 of Unlayered Infill and v0.0.5 of Wave Overhangs shipped exactly that
  bug — they wrote their "loaded" log line at import time. This test is the
  regression guard. It installs an audit hook that RAISES on any write, then
  imports each plugin and demands that it still loads.

  A real audit hook cannot be uninstalled once added, so the hook here is
  armed only around the import and inert the rest of the time.
"""
import ast
import importlib.util
import os
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(HERE))

import fake_orca  # noqa: E402

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
    return cond


# --------------------------------------------------------------------------
# The audit hook. Armed only while `state["strict"]` is True.
# --------------------------------------------------------------------------
state = {"strict": False, "events": []}

WRITE_EVENTS = (
    "open", "os.remove", "os.rename", "os.mkdir", "os.rmdir",
    "shutil.copyfile", "shutil.move",
)


def _audit(event, args):
    if not state["strict"]:
        return
    if event == "open":
        path = args[0] if args else ""
        mode = str(args[1]) if len(args) > 1 and args[1] else ""
        if any(c in mode for c in "wax+"):
            state["events"].append(f"open({path!r}, mode={mode!r})")
            raise PermissionError(
                f"audit hook: filesystem write denied during plugin import: {path}")
        return
    if event in WRITE_EVENTS or event.startswith("socket."):
        state["events"].append(f"{event}{args!r:.80}")
        raise PermissionError(f"audit hook: {event} denied during plugin import")


sys.addaudithook(_audit)


def import_under_audit(path, modname):
    """Import a plugin with writes denied. Returns (module, error, events)."""
    fake_orca.install()
    for m in (modname, "nonplanar_core", "wave_core"):
        sys.modules.pop(m, None)
    spec = importlib.util.spec_from_file_location(modname, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[modname] = mod

    state["events"] = []
    saved_bytecode = sys.dont_write_bytecode
    sys.dont_write_bytecode = True          # .pyc writes are the test's fault
    state["strict"] = True
    err = None
    try:
        spec.loader.exec_module(mod)
    except BaseException as e:              # noqa: BLE001 - that is the point
        err = e
    finally:
        state["strict"] = False
        sys.dont_write_bytecode = saved_bytecode
    return mod, err, list(state["events"])


PLUGINS = [
    (REPO / "plugins" / "unlayered-infill" / "unlayered_infill_orca.py",
     "unlayered_infill_orca"),
    (REPO / "plugins" / "wave-overhangs" / "wave_overhangs_orca.py",
     "wave_overhangs_orca"),
]

with tempfile.TemporaryDirectory() as tmp:
    # Point the log at a real, writable place: the plugin must still choose
    # not to touch it during import.
    os.environ["ORCA_PLUGIN_LOG_DIR"] = tmp

    for path, modname in PLUGINS:
        if not path.exists():
            failures.append(f"{path} is missing")
            continue

        # ---- 1. no module-level I/O calls at all (static) -----------------
        tree = ast.parse(path.read_text(encoding="utf-8"))
        for node in tree.body:
            if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call):
                fn = node.value.func
                nm = getattr(fn, "id", None) or getattr(fn, "attr", None)
                check(nm not in ("_log", "_write_log", "open", "print"),
                      f"{path.name}: calls {nm}() at module level (line "
                      f"{node.lineno}). Nothing may touch the filesystem while "
                      f"Orca is importing the plugin.")

        # ---- 2. it actually imports with writes denied --------------------
        mod, err, events = import_under_audit(path, modname)
        check(err is None,
              f"{path.name} FAILED TO IMPORT under an audit hook that denies "
              f"writes: {type(err).__name__}: {err}. This is what "
              f"'the plugin would not install' looks like.")
        check(not events,
              f"{path.name} attempted {len(events)} filesystem/network "
              f"operation(s) during import: {events[:3]}")

        # ---- 3. and it is still functional afterwards ---------------------
        if err is None:
            check(hasattr(mod, "PLUGIN_VERSION"),
                  f"{path.name} imported but exposes no PLUGIN_VERSION")
            check(hasattr(mod, "log_path"),
                  f"{path.name} imported but has no log_path()")

    # ----------------------------------------------------------------------
    # 4. a log write that is DENIED must not break the capability either
    # ----------------------------------------------------------------------
    path, modname = PLUGINS[0]
    fake_orca.install()
    sys.modules.pop(modname, None)
    sys.modules.pop("nonplanar_core", None)
    spec = importlib.util.spec_from_file_location(modname, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[modname] = mod
    spec.loader.exec_module(mod)

    state["strict"] = True
    try:
        try:
            mod._log("this write will be denied by the audit hook")
            denied_ok = True
        except BaseException as e:                       # noqa: BLE001
            denied_ok = False
            why = f"{type(e).__name__}: {e}"
    finally:
        state["strict"] = False
    check(denied_ok,
          f"_log() propagated the audit refusal instead of swallowing it "
          f"({why if not denied_ok else ''}). A denied log write must never be "
          f"able to fail a slice.")

    # ----------------------------------------------------------------------
    # 5. NOTHING may be imported for the first time inside a capability call
    #
    # The audit hook is OFF while the plugin module is imported and ON during
    # a capability call, where every file open is audited. A first-use import
    # inside a capability is therefore an audited read of a file the plugin
    # never declared -- OrcaSlicer issue #15944, and the shape of the
    # Unlayered Infill 0.4.3 Refresh failure (`from statistics import
    # multimode`, deep inside the export step).
    #
    # Checked statically, over the plugin module AND over the engine source it
    # inlines as a string, because that is where the offender lived.
    # ----------------------------------------------------------------------
    ENGINE_LITERALS = ("_NONPLANAR_CORE_SRC", "_WAVE_CORE_SRC")

    def sources_of(path):
        """The plugin's own source, plus any engine it inlines as a literal."""
        text = path.read_text(encoding="utf-8")
        yield path.name, text
        tree = ast.parse(text)
        for node in ast.walk(tree):
            if (isinstance(node, ast.Assign)
                    and isinstance(node.value, ast.Constant)
                    and isinstance(node.value.value, str)
                    and any(isinstance(t, ast.Name) and t.id in ENGINE_LITERALS
                            for t in node.targets)):
                name = next(t.id for t in node.targets
                            if isinstance(t, ast.Name) and t.id in ENGINE_LITERALS)
                yield f"{path.name}:{name}", node.value.value

    # The only import allowed below module level is the one that probes for an
    # optional dependency at LOAD time -- it is called from module scope.
    IMPORT_OK_IN = {"_import_deps"}

    for path, _modname in PLUGINS:
        for label, text in sources_of(path):
            tree = ast.parse(text)
            for func in ast.walk(tree):
                if not isinstance(func, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    continue
                if func.name in IMPORT_OK_IN:
                    continue
                for node in ast.walk(func):
                    if isinstance(node, (ast.Import, ast.ImportFrom)):
                        what = getattr(node, "module", None) or ", ".join(
                            a.name for a in node.names)
                        check(False,
                              f"{label}: {func.name}() imports {what!r} at line "
                              f"{node.lineno}. Imports must happen at module "
                              f"load, where Orca's audit hook is off -- a "
                              f"first-use import inside a capability call is "
                              f"audited and can be denied (issue #15944).")

    # ----------------------------------------------------------------------
    # 6. Importing a plugin TWICE -- what Refresh in the Plugins dialog does
    #    -- must leave its inlined engine working
    # ----------------------------------------------------------------------
    for path, modname in PLUGINS:
        attr, func_name = (("npc", "process") if "unlayered" in modname
                           else ("wc", "plan_layer"))
        first = None
        for pass_no in (1, 2, 3):
            fake_orca.install()
            sys.modules.pop(modname, None)
            spec = importlib.util.spec_from_file_location(modname, path)
            mod = importlib.util.module_from_spec(spec)
            sys.modules[modname] = mod
            try:
                spec.loader.exec_module(mod)
            except BaseException as e:                   # noqa: BLE001
                check(False, f"{path.name}: re-import pass {pass_no} (Refresh) "
                             f"failed: {type(e).__name__}: {e}")
                break
            engine = getattr(mod, attr, None)
            ok_now = engine is not None and hasattr(engine, func_name)
            if pass_no == 1:
                # Wave Overhangs' engine needs shapely, which is deliberately
                # absent in this sandbox. Whether it loads at all is not this
                # test's business -- whether re-importing DEGRADES it is.
                first = ok_now
                continue
            check(ok_now == first,
                  f"{path.name}: the inlined engine was "
                  f"{'present' if first else 'absent'} after the first import "
                  f"but {'present' if ok_now else 'absent'} after pass "
                  f"{pass_no}. Re-importing (what Refresh does) must not change "
                  f"the answer -- that is how a working engine turns into "
                  f"'engine MISSING' and every capability starts failing.")
            if ok_now:
                check(sys.modules.get(engine.__name__) is engine,
                      f"{path.name}: re-import pass {pass_no} left sys.modules "
                      f"pointing at a different {attr} than the plugin uses.")

if failures:
    print(f"FAILED ({len(failures)})")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)

print("ok -- all shipped plugins import cleanly under an audit hook that denies "
      "every filesystem write, a denied log write cannot break a capability, no "
      "capability imports anything for the first time inside the audit scope, "
      "and re-importing a plugin (Refresh) leaves its inlined engine working")
