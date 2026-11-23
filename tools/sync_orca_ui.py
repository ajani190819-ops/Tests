#!/usr/bin/env python3
"""Keep settings/setup helpers identical in the two independent Orca plugins.

Edit shared/orca/settings_ui.py, then run this tool. Installed plugins remain
one entry file each, with no shared-file lookup, import or installation step.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "shared/orca/settings_ui.py"
TARGETS = [ROOT / "plugins/unlayered-infill/unlayered_infill_orca.py",
           ROOT / "plugins/wave-overhangs/wave_overhangs_orca.py"]
START = "# --- BEGIN SHARED ORCA SETTINGS UI ---\n"
END = "# --- END SHARED ORCA SETTINGS UI ---\n"


def sync(check=False):
    source = SOURCE.read_text(encoding="utf-8").rstrip() + "\n"
    bad = []
    for path in TARGETS:
        text = path.read_text(encoding="utf-8")
        if text.count(START) != 1 or text.count(END) != 1:
            raise ValueError(f"Expected one settings UI block in {path.relative_to(ROOT)}")
        first = text.index(START) + len(START)
        last = text.index(END, first)
        if text[first:last] != source:
            bad.append(str(path.relative_to(ROOT)))
            if not check:
                path.write_text(text[:first] + source + text[last:], encoding="utf-8")
    if check and bad:
        print("FAILED: settings/setup helpers out of sync: " + ", ".join(bad))
        return 1
    print(("updated: " + ", ".join(bad)) if bad else "ok -- Orca settings/setup helpers synchronized")
    return 0


if __name__ == "__main__":
    sys.exit(sync("--check" in sys.argv))
