#!/usr/bin/env python3
"""Write each capability's get_default_config() to docs/config-reference/.

    python3 tools/dump_default_config.py
    python3 tools/dump_default_config.py --check   # fail if stale

WHY THIS EXISTS
  The Config tab in OrcaSlicer shows the configuration SAVED for a capability,
  which on an upgraded install is whatever an older build wrote -- not the
  running build's defaults. The owner needs something to compare against:
  "this is what it should say". These files are that reference, generated from
  the shipped plugin files so they cannot drift into fiction.
"""
import importlib.util
import json
import os
import pathlib
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent
OUT = REPO / "docs" / "config-reference"
sys.path.insert(0, str(REPO / "tests"))

import fake_orca  # noqa: E402

TARGETS = [
    ("plugins/unlayered-infill/unlayered_infill_orca.py", [
        ("UnlayeredInfill", "unlayered-infill.json"),
        ("UnlayeredInfillCheck", "unlayered-infill-check-setup.json"),
    ]),
    ("plugins/wave-overhangs/wave_overhangs_orca.py", [
        ("WaveOverhangsSlicing", "wave-overhangs.json"),
        ("WaveOverhangsCheck", "wave-overhangs-check.json"),
    ]),
]

MINIMAL = {"unlayered-infill.json": "unlayered-infill-minimal.json"}


def load(relpath, modname):
    fake_orca.install()
    spec = importlib.util.spec_from_file_location(modname, REPO / relpath)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[modname] = mod
    spec.loader.exec_module(mod)
    return mod


def main():
    check_only = "--check" in sys.argv[1:]
    os.environ.setdefault("ORCA_PLUGIN_LOG_DIR", tempfile.mkdtemp())
    OUT.mkdir(parents=True, exist_ok=True)
    stale = []
    for relpath, caps in TARGETS:
        mod = load(relpath, pathlib.Path(relpath).stem)
        for clsname, filename in caps:
            cfg = getattr(mod, clsname)().get_default_config()
            wanted = [(filename, cfg)]
            if filename in MINIMAL:
                wanted.append((MINIMAL[filename],
                               {k: v for k, v in cfg.items()
                                if not k.startswith("_")}))
            for name, payload in wanted:
                text = json.dumps(payload, indent=2) + "\n"
                path = OUT / name
                if check_only:
                    old = path.read_text(encoding="utf-8") if path.exists() else ""
                    if old != text:
                        stale.append(name)
                else:
                    path.write_text(text, encoding="utf-8")
                    print(f"wrote docs/config-reference/{name}")
    if check_only:
        if stale:
            print("STALE: " + ", ".join(stale))
            print("Run: python3 tools/dump_default_config.py")
            return 1
        print("ok: docs/config-reference matches the shipped plugins")
    return 0


if __name__ == "__main__":
    sys.exit(main())
