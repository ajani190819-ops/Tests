#!/usr/bin/env python3
"""Bump a plugin's version everywhere it is written down, in one command.

    python3 tools/bump_version.py wave-overhangs 0.0.51
    python3 tools/bump_version.py unlayered-infill 0.4.10 --check
    python3 tools/bump_version.py --check-docs

A release touches the PEP 723 header, PLUGIN_VERSION, the standalone tool's
TOOL_VERSION and MARKER_VERSION, plugins.json, the .bat fallback line, and the
headline version callouts in README.md, plugins/<id>/README.md, and
docs/ROADMAP.md. Editing those by hand is how a release ends up half-bumped.

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

DOC_PATTERNS = {
    "wave-overhangs": [
        (ROOT / "README.md", [
            (r"(\| \[Wave Overhangs\]\(plugins/wave-overhangs/\) \| )\d+\.\d+\.\d+( \|)", r"\g<1>{v}\g<2>"),
            (r"(\| File → Plugins, \*\*Version\*\* column \| `)\d+\.\d+\.\d+(`)", r"\g<1>{v}\g<2>"),
            (r"(\| The updater's output \| `Wave Overhangs v)\d+\.\d+\.\d+(` \|)", r"\g<1>{v}\g<2>"),
            (r"(\| \*\*Settings guide & check\*\*, first line \| `Wave Overhangs v)\d+\.\d+\.\d+( -- setup check` \|)", r"\g<1>{v}\g<2>"),
            (r"( for Unlayered Infill and \*\*)\d+\.\d+\.\d+(\*\* for Wave\s+Overhangs\.)", r"\g<1>{v}\g<2>"),
            (r"(Version column to confirm )\d+\.\d+\.\d+( / )", r"\g<1>{v}\g<2>"),
        ]),
        (ROOT / "plugins" / "wave-overhangs" / "README.md", [
            (r"(Version column reads \*\*)\d+\.\d+\.\d+(\*\*\.)", r"\g<1>{v}\g<2>"),
        ]),
        (ROOT / "docs" / "ROADMAP.md", [
            (r"(plugins: Wave Overhangs \*\*)\d+\.\d+\.\d+(\*\* and Unlayered Infill)", r"\g<1>{v}\g<2>"),
            (r"(\| Wave Overhangs \| )\d+\.\d+\.\d+(\.)", r"\g<1>{v}\g<2>"),
        ]),
    ],
    "unlayered-infill": [
        (ROOT / "README.md", [
            (r"(\| \[Unlayered Infill\]\(plugins/unlayered-infill/\) \| )\d+\.\d+\.\d+( \|)", r"\g<1>{v}\g<2>"),
            (r"(applies to `Unlayered Infill`; its current (?:test-)?build version is `)\d+\.\d+\.\d+(`)", r"\g<1>{v}\g<2>"),
            (r"(build reads \*\*)\d+\.\d+\.\d+(\*\* for Unlayered Infill)", r"\g<1>{v}\g<2>"),
            (r"(Version column to confirm \d+\.\d+\.\d+ / )\d+\.\d+\.\d+(\.)", r"\g<1>{v}\g<2>"),
        ]),
        (ROOT / "plugins" / "unlayered-infill" / "README.md", [
            (r"(Version column currently reads \*\*)\d+\.\d+\.\d+(\*\*, and exported G-code is stamped with\n`; unlayered-infill v)\d+\.\d+\.\d+(`)", r"\g<1>{v}\g<2>{v}\g<3>"),
        ]),
        (ROOT / "docs" / "ROADMAP.md", [
            (r"(and Unlayered Infill \*\*)\d+\.\d+\.\d+(\*\*, with the)", r"\g<1>{v}\g<2>"),
            (r"(\| Unlayered Infill \| )\d+\.\d+\.\d+(\.)", r"\g<1>{v}\g<2>"),
        ]),
    ],
}


def plugin_entry(plugin_id):
    data = json.loads(CATALOGUE.read_text(encoding="utf-8"))
    for entry in data["plugins"]:
        if entry["id"] == plugin_id:
            return data, entry
    raise SystemExit(f"{plugin_id!r} is not in plugins.json. Known: "
                     + ", ".join(p["id"] for p in data["plugins"]))


def sync_docs_for(plugin_id, version, check):
    changed = []
    missing = []
    for doc_path, rules in DOC_PATTERNS.get(plugin_id, []):
        if not doc_path.exists():
            continue
        text = doc_path.read_text(encoding="utf-8")
        before = text
        for pattern, tmpl in rules:
            repl = tmpl.format(v=version)
            new_text, count = re.subn(pattern, repl, text, count=1, flags=re.M)
            if count == 0:
                missing.append(f"{doc_path.relative_to(ROOT)}: unmatched pattern {pattern!r}")
            text = new_text
        if text != before:
            changed.append(str(doc_path.relative_to(ROOT)))
            if not check:
                doc_path.write_text(text, encoding="utf-8")
    return changed, missing


def check_all_docs():
    data = json.loads(CATALOGUE.read_text(encoding="utf-8"))
    bad = []
    for entry in data["plugins"]:
        if entry.get("status") != "ready":
            continue
        changed, missing = sync_docs_for(entry["id"], entry["version"], check=True)
        bad.extend(f"{entry['id']} drifted in {c}" for c in changed)
        bad.extend(missing)
    if bad:
        print("FAILED: documentation versions drifted from plugins.json:")
        for item in bad:
            print(f"  - {item}")
        return 1
    print("ok -- README.md, plugins/*/README.md, and docs/ROADMAP.md match plugins.json")
    return 0


def main():
    if "--check-docs" in sys.argv[1:]:
        return check_all_docs()

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
        for pattern, replacement in (
                (r'(^# version = ")[^"]+(")', rf'\g<1>{version}\g<2>'),
                (r'(^PLUGIN_VERSION = ")[^"]+(")', rf'\g<1>{version}\g<2>'),
                (r'(^TOOL_VERSION = ")[^"]+(")', rf'\g<1>{version}\g<2>'),
                (r'(^MARKER_VERSION = ")[^"]+(")', rf'\g<1>{version}\g<2>')):
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

    doc_changes, doc_missing = sync_docs_for(plugin_id, version, check=check)
    changes.extend(doc_changes)
    missing.extend(doc_missing)

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
