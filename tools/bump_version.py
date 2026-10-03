#!/usr/bin/env python3
"""Bump a plugin's version everywhere it is written down, in one command.

    python3 tools/bump_version.py wave-overhangs 0.0.37
    python3 tools/bump_version.py unlayered-infill 0.4.5 --check

A release touches the PEP 723 header, PLUGIN_VERSION, the standalone tool's
TOOL_VERSION and MARKER_VERSION, plugins.json and the .bat fallback line.
Editing those by hand is how a release ends up half-bumped and
tests/test_installer.py reports four unrelated-looking failures.

Orca-Plugins.bat is read and written as BYTES on purpose: it must stay CRLF,
and Python's text mode silently rewrites every line ending.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BAT = ROOT / "Orca-Plugins.bat"
CATALOGUE = ROOT / "plugins.json"


def plugin_entry(plugin_id):
    data = json.loads(CATALOGUE.read_text(encoding="utf-8"))
    for entry in data["plugins"]:
        if entry["id"] == plugin_id:
            return data, entry
    raise SystemExit(f"{plugin_id!r} is not in plugins.json. Known: "
                     + ", ".join(p["id"] for p in data["plugins"]))


def sub_once(text, pattern, replacement, what, changes, check):
    new, n = re.subn(pattern, replacement, text, count=1)
    if n == 0:
        return text, False
    if new != text:
        changes.append(what)
    return new, True


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    check = "--check" in sys.argv[1:]
    if len(args) != 2:
        raise SystemExit(__doc__)
    plugin_id, version = args
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit(f"{version!r} is not a three-part version")

    data, entry = plugin_entry(plugin_id)
    old = entry["version"]
    folder = ROOT / "plugins" / plugin_id
    changes = []
    missing = []

    for path in sorted(folder.glob("*.py")):
        text = path.read_text(encoding="utf-8")
        before = text
        for pattern, replacement, what in (
                (r'(^# version = ")[^"]+(")', rf'\g<1>{version}\g<2>',
                 f"{path.name}: PEP 723 header"),
                (r'(^PLUGIN_VERSION = ")[^"]+(")', rf'\g<1>{version}\g<2>',
                 f"{path.name}: PLUGIN_VERSION"),
                (r'(^TOOL_VERSION = ")[^"]+(")', rf'\g<1>{version}\g<2>',
                 f"{path.name}: TOOL_VERSION"),
                (r'(^MARKER_VERSION = ")[^"]+(")', rf'\g<1>{version}\g<2>',
                 f"{path.name}: MARKER_VERSION")):
            text = re.sub(pattern, replacement, text, count=1, flags=re.M)
        if text != before:
            changes.append(f"{path.relative_to(ROOT)}")
            if not check:
                path.write_text(text, encoding="utf-8")

    if entry["version"] != version:
        entry["version"] = version
        changes.append("plugins.json")
        if not check:
            CATALOGUE.write_text(json.dumps(data, indent=2) + "\n",
                                 encoding="utf-8")

    raw = BAT.read_bytes()
    name = entry["name"].encode()
    needle = name + b"^|" + old.encode() + b"^|"
    if needle in raw:
        changes.append("Orca-Plugins.bat (fallback line, bytes/CRLF safe)")
        if not check:
            BAT.write_bytes(raw.replace(
                needle, name + b"^|" + version.encode() + b"^|", 1))
    elif (name + b"^|" + version.encode() + b"^|") not in raw:
        missing.append("Orca-Plugins.bat fallback line")

    changelog = folder / "CHANGELOG.md"
    if changelog.exists() and f"## {version}" not in changelog.read_text(
            encoding="utf-8"):
        missing.append(f"{changelog.relative_to(ROOT)}: no '## {version}' entry "
                       f"(write it, bullets FIRST -- sync_changelog.py turns "
                       f"the first bullet into the plugin description)")

    verb = "would change" if check else "changed"
    for item in changes:
        print(f"  {verb}: {item}")
    for item in missing:
        print(f"  STILL TO DO: {item}")
    if not changes and not missing:
        print(f"  {plugin_id} is already at {version} everywhere")
    print()
    print("next: python3 tools/sync_engine.py && python3 tools/sync_changelog.py "
          "&& python3 tools/dump_default_config.py && python3 tools/check_all.py")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
