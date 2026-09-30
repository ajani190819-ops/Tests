#!/usr/bin/env python3
"""Guard the contract between plugins.json and Update-Orca-Plugins.bat.

    python3 test_installer.py

The .bat is Windows-only, so this cannot run it. What it CAN do is check the
things that actually break in practice, none of which need Windows:

  * the catalogue is valid and every "ready" plugin really ships the file it
    promises, at the path the updater will build a URL from
  * the catalogue's version matches the PEP 723 header of the shipped file --
    and that header uses the exact literal form the .bat's findstr parses
  * no field contains a character that would corrupt the pipe-delimited plan
    the .bat parses with `for /f ... delims=|`
  * the .bat's built-in fallback list still matches the catalogue
  * the .bat keeps its CRLF line endings and .gitattributes keeps git from
    re-normalising them away (users download it from raw.githubusercontent.com)
  * a faithful replay of the install loop does the right thing: first run
    installs, second run overwrites, a copy parked under a different folder
    name gets updated too, a `_subscribed` cloud copy is left alone, and
    PLUGIN_ONLY narrows the plan
"""
from __future__ import annotations

import json
import pathlib
import re
import shutil
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
MANIFEST = REPO / "plugins.json"
BAT = REPO / "Update-Orca-Plugins.bat"
GATTR = REPO / ".gitattributes"

failures: list[str] = []


def check(cond: bool, msg: str) -> bool:
    if not cond:
        failures.append(msg)
    return cond


def cmd_tokens(line: str, delims: str, count: int) -> list[str]:
    """Model of `for /f "tokens=1-N delims=..."`: split on any delimiter,
    collapse consecutive delimiters, skip leading ones, take N tokens."""
    parts, run = [], []
    for ch in line:
        if ch in delims:
            if run:
                parts.append("".join(run))
                run = []
        else:
            run.append(ch)
    if run:
        parts.append("".join(run))
    return parts[:count]


bat = BAT.read_text(encoding="utf-8", errors="replace")


# --------------------------------------------------------------------------
# 1. catalogue
# --------------------------------------------------------------------------
manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
plugins = manifest["plugins"]
ready = [p for p in plugins if p["status"] == "ready"]

check(bool(ready), "no plugins are marked ready -- the updater would install nothing")
check(len({p["id"] for p in plugins}) == len(plugins), "duplicate plugin ids in plugins.json")

for p in plugins:
    pid = p["id"]
    for key in ("id", "name", "version", "status", "file", "path", "orca_dir", "capabilities"):
        check(key in p, f"{pid}: missing key {key!r}")

    if p["status"] != "ready":
        check(p["file"] is None and p["path"] is None,
              f"{pid}: status is {p['status']!r} but it still declares a file")
        continue

    # Pipe and caret would break the plan file / batch escaping.
    for key in ("id", "name", "version", "orca_dir", "file", "path"):
        val = str(p[key])
        check("|" not in val, f"{pid}: {key} contains '|', which breaks the updater's plan format")
        check("^" not in val and "%" not in val and "!" not in val,
              f"{pid}: {key} contains a character cmd.exe would mangle")
    for cap in p["capabilities"]:
        check("|" not in cap and "^" not in cap and "%" not in cap and "!" not in cap,
              f"{pid}: capability {cap!r} contains a character cmd.exe would mangle")

    # The plugins live one folder per plugin: plugins/<id>/<file>.
    check(p["path"] == f"plugins/{pid}/{p['file']}",
          f"{pid}: path {p['path']!r} should be plugins/{pid}/{p['file']!r}")
    check("\\" not in p["path"],
          f"{pid}: path must use forward slashes -- it becomes a URL")

    # The updater's --local mode also looks for the file next to the .bat
    # itself as a fallback, so the bare filename must stay a bare filename.
    check("/" not in p["file"] and "\\" not in p["file"],
          f"{pid}: file {p['file']!r} must be a bare filename")

    # The .bat's plan has exactly two capability slots; a third capability
    # would be silently dropped into the sidecar's second slot.
    check(len(p["capabilities"]) == 2,
          f"{pid}: the .bat supports exactly 2 capabilities, catalogue lists {len(p['capabilities'])}")

    shipped = REPO / p["path"]
    check(shipped.exists(), f"{pid}: {p['path']} does not exist, so the download would 404")
    if shipped.exists():
        check(shipped.stat().st_size > 2000,
              f"{pid}: {p['path']} is smaller than the updater's 2000-byte sanity floor")
        head = shipped.read_text(encoding="utf-8")[:4000]
        m = re.search(r'^#\s*version\s*=\s*"([^"]+)"', head, re.MULTILINE)
        check(m is not None, f"{pid}: no PEP 723 version header in the shipped plugin")
        if m:
            check(m.group(1) == p["version"],
                  f"{pid}: plugins.json says v{p['version']} but the file says v{m.group(1)}")

        # The .bat greps the downloaded file with:
        #     findstr /b /c:"# version = "
        # so the header must keep that exact literal spelling, not just any
        # regex-equivalent form.
        check('# version = "' in head,
              f"{pid}: version header is not the literal `# version = \"...\"` the .bat parses")

        # ...and then extracts the value with:
        #     for /f "usebackq tokens=N delims== " %%V
        # Simulate that tokenization here so the header shape and the .bat's
        # token count cannot drift apart. (tokens=3 because with delims of
        # "=" and " " the line splits into: # / version / "x.y.z".)
        m2 = re.search(r'tokens=(\d+) delims== " %%V', bat)
        check(m2 is not None, "the .bat's version-header parse line changed shape")
        if m2:
            hdr = re.search(r'^# version = "([^"]*)"$', head, re.MULTILINE).group(0)
            toks = cmd_tokens(hdr, "= ", int(m2.group(1)))
            check(len(toks) == int(m2.group(1)) and toks[-1] == f'"{m.group(1)}"',
                  f"{pid}: the .bat's for /f would not extract v{m.group(1)} from the "
                  f"version header -- got tokens {toks}")

        # ...and its "is this really a plugin" check greps for the PEP 723
        # fence before trusting the download.
        check("# /// script" in head,
              f"{pid}: shipped file has no `# /// script` fence, so the .bat would reject it")

        # The PEP 723 *name* carries the version, so OrcaSlicer's Plugins
        # dialog shows which build is installed in its Name column (it has a
        # Version column too -- this is belt and braces, and it is the column
        # people actually read). The version deliberately does NOT go in the
        # capability names: a process preset stores the capability name as its
        # value, so renaming capabilities per version would make every update
        # orphan the preset and block slicing until it was re-picked.
        mname = re.search(r'^#\s*name\s*=\s*"([^"]+)"', head, re.MULTILINE)
        check(mname is not None, f"{pid}: no PEP 723 name header in the shipped plugin")
        if mname:
            want = f"{p['name']} v{p['version']}"
            check(mname.group(1) == want,
                  f"{pid}: PEP 723 name is {mname.group(1)!r} but the catalogue "
                  f"implies {want!r} (catalogue name + ' v' + version). The .bat "
                  f"composes the sidecar's plugin_name the same way, so these "
                  f"must agree or Orca's .install_state.json names a plugin that "
                  f"does not exist.")

        body = shipped.read_text(encoding="utf-8")

        # What the plugin reports about itself at runtime -- "Check setup" and
        # the G-code stamp -- must be the version that was actually installed.
        mpv = re.search(r'^PLUGIN_VERSION\s*=\s*"([^"]+)"', body, re.MULTILINE)
        check(mpv is not None,
              f"{pid}: no module-level PLUGIN_VERSION constant; Check setup and the "
              f"G-code stamp have nothing truthful to report")
        if mpv:
            check(mpv.group(1) == p["version"],
                  f"{pid}: PLUGIN_VERSION is {mpv.group(1)!r} but the file ships as "
                  f"v{p['version']}")

        # Some engines carry their own stamp version inside the inlined source
        # (it is what lands in the exported G-code). Same rule.
        mmv = re.search(r'MARKER_VERSION\s*=\s*\\"([^"\\]+)\\"', body)
        if mmv:
            check(mmv.group(1) == p["version"],
                  f"{pid}: the G-code stamp says v{mmv.group(1)} but the plugin "
                  f"ships as v{p['version']}")

        # Every capability the sidecar advertises must actually exist in the
        # file Orca will load.
        for cap in p["capabilities"]:
            check(cap in shipped.read_text(encoding="utf-8"),
                  f"{pid}: capability {cap!r} does not appear in {p['path']}")

    check(len(p["capabilities"]) >= 1, f"{pid}: no capabilities, Orca would show nothing to enable")


# --------------------------------------------------------------------------
# 2. the .bat agrees with the catalogue
# --------------------------------------------------------------------------
check(f'set "REPO={manifest["repo"]}"' in bat,
      f"the .bat does not point at {manifest['repo']}")
check('set "MANIFEST_PATH=plugins.json"' in bat,
      "the .bat does not fetch plugins.json")
check('set "REF_1=main"' in bat,
      "the .bat's default ref is not main")

# The fallback list is only used when PowerShell cannot parse the catalogue, so
# it silently rots unless something checks it.
for p in ready:
    line = "|".join([p["id"], p["name"], p["version"], p["orca_dir"],
                     p["file"], p["path"], p["capabilities"][0], p["capabilities"][1]])
    check(line.replace("|", "^|") + "^|end" in bat,
          f"{p['id']}: the .bat fallback list is stale, expected a line with:\n      {line}")

fallback_ids = re.findall(r'echo ([a-z-]+)\^\|', bat)
check(sorted(set(fallback_ids)) == sorted(p["id"] for p in ready),
      f"fallback list {sorted(set(fallback_ids))} != ready plugins {sorted(p['id'] for p in ready)}")

# The plan reader and the sidecar writer, pinned so they cannot drift from
# what the catalogue promises.
check('for /f "usebackq tokens=1-9 delims=|" %%A in ("%PLAN_FILE%")' in bat,
      "the .bat plan reader changed shape -- update this test deliberately")
check('{ "%~3": true }' in bat and '{ "%~4": true }' in bat,
      "the .bat sidecar template no longer writes both capabilities")
check('"installed_from": "local"' in bat and '"installed_version": "%~5"' in bat
      and '"plugin_name": "%~2"' in bat,
      "the .bat sidecar template no longer matches Orca's .install_state.json")
# The sidecar's plugin_name must match the plugin's PEP 723 name, which now
# carries the version. Composing it from PL_VER -- which the .bat reads out of
# the downloaded file's own header -- means it tracks what was really installed
# instead of whatever the catalogue happened to say.
check('call :write_state "%STATE_FILE%" "%PL_NAME% v%PL_VER%"' in bat,
      "the .bat no longer composes the versioned plugin name for the sidecar, so "
      "Orca's .install_state.json would name a plugin that does not exist")
check('echo   [%PL_ACTION%] %PL_NAME% v%PL_VER% %PL_VERMSG%-- %PL_SIZE% bytes' in bat,
      "the .bat's install summary no longer reports the installed version")
check('"enabled": true' in bat,
      "the .bat no longer installs the plugins as enabled")

# The download stack: three methods, a size floor, and the sibling rules.
check("curl.exe -fLsS --retry 2" in bat, "the .bat lost its curl.exe download path")
check("Tls12" in bat and "Invoke-WebRequest" in bat, "the .bat lost its PowerShell download path")
check("bitsadmin /transfer" in bat, "the .bat lost its bitsadmin download path")
check("GTR 2000" in bat, "the .bat lost its 2000-byte download sanity floor")
check("_subscribed" in bat, "the .bat no longer skips _subscribed cloud copies")
check("Slicing Pipeline Plugin" in bat,
      "the .bat no longer tells users where to select the plugin")


# --------------------------------------------------------------------------
# 3. the .bat survives being downloaded from raw.githubusercontent.com
# --------------------------------------------------------------------------
raw = BAT.read_bytes()
check(raw.count(b"\r\n") == raw.count(b"\n") > 0,
      "the .bat has LF-only lines; it must be CRLF throughout (see its header)")
check(b"\r" not in raw.replace(b"\r\n", b""),
      "the .bat contains a bare CR")
check(raw.endswith(b"\r\n"),
      "the .bat does not end with a newline")

ga = GATTR.read_text(encoding="utf-8") if GATTR.exists() else ""
check(re.search(r"(?m)^\*\.bat\s+-text\s*$", ga),
      ".gitattributes must pin `*.bat -text` or git will store the .bat with LF "
      "and raw.githubusercontent.com will serve it broken")


# --------------------------------------------------------------------------
# 4. replay the updater's install loop
# --------------------------------------------------------------------------
def report_and_exit() -> None:
    print(f"FAILED ({len(failures)})")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)


# The replay copies real files around, so stop here rather than crashing on a
# plugin the checks above already flagged as missing.
if any(p["status"] == "ready" and not (REPO / p["path"]).exists()
       for p in plugins if p.get("path")):
    report_and_exit()


def build_plan(man: dict, only: list[str] | None = None) -> list[dict]:
    """Mirror of the PowerShell step in :build_plan."""
    out = []
    for p in man["plugins"]:
        if p["status"] != "ready":
            continue
        if only and p["id"] not in only:
            continue
        out.append(p)
    return out


def install_one(p: dict, plugin_root: pathlib.Path) -> str:
    """Mirror of :install_one -- returns 'NEW' or 'UPDATED'.

    The sidecar is written exactly the way the .bat's :write_state writes it
    (same fields, same spacing), just with \n instead of CRLF.
    """
    src = REPO / p["path"]
    dest_dir = plugin_root / p["orca_dir"]
    was_there = (dest_dir / p["file"]).exists()
    dest_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dest_dir / p["file"])
    sidecar = (
        "{\n"
        '  "capabilities": [\n'
        f'    {{ "{p["capabilities"][0]}": true }},\n'
        f'    {{ "{p["capabilities"][1]}": true }}\n'
        "  ],\n"
        '  "enabled": true,\n'
        '  "installed_from": "local",\n'
        f'  "installed_version": "{p["version"]}",\n'
        # ":install_one" composes this as "%PL_NAME% v%PL_VER%" so it matches
        # the PEP 723 name Orca reads out of the plugin file itself.
        f'  "plugin_name": "{p["name"]} v{p["version"]}"\n'
        "}\n"
    )
    (dest_dir / ".install_state.json").write_text(sidecar, encoding="utf-8")
    # :update_siblings -- refresh other copies, but never a cloud/subscribed one
    for other in plugin_root.rglob(p["file"]):
        if other.parent == dest_dir:
            continue
        if "_subscribed" in str(other.parent):
            continue
        shutil.copyfile(src, other)
    return "UPDATED" if was_there else "NEW"


with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp) / "OrcaSlicer" / "orca_plugins"

    plan = build_plan(manifest)
    check([p["id"] for p in plan] == [p["id"] for p in ready], "plan does not match the ready plugins")

    # -- first run: nothing installed yet
    results = {p["id"]: install_one(p, root) for p in plan}
    check(all(v == "NEW" for v in results.values()),
          f"first run should report NEW for everything, got {results}")
    for p in plan:
        installed = root / p["orca_dir"] / p["file"]
        check(installed.exists(), f"{p['id']}: not installed")
        check(installed.read_bytes() == (REPO / p["path"]).read_bytes(),
              f"{p['id']}: installed file differs from the shipped one")
        state_file = root / p["orca_dir"] / ".install_state.json"
        check(state_file.exists(), f"{p['id']}: sidecar not written")
        if state_file.exists():
            state = json.loads(state_file.read_text(encoding="utf-8"))
            check(state["installed_version"] == p["version"],
                  f"{p['id']}: sidecar version {state['installed_version']} != catalogue {p['version']}")
            check([k for c in state["capabilities"] for k in c] == p["capabilities"],
                  f"{p['id']}: sidecar capabilities do not match the catalogue")
            check(state["enabled"] is True,
                  f"{p['id']}: sidecar does not enable the plugin")
            check(state["installed_from"] == "local",
                  f"{p['id']}: sidecar installed_from should be 'local'")
            # The whole point of the versioned name: the sidecar must call the
            # plugin exactly what the plugin calls itself, or Orca's saved
            # enable-state belongs to a plugin name that does not exist.
            hdr_name = re.search(
                r'^#\s*name\s*=\s*"([^"]+)"',
                installed.read_text(encoding="utf-8")[:4000], re.MULTILINE)
            hdr_name_val = hdr_name.group(1) if hdr_name else None
            check(state["plugin_name"] == hdr_name_val,
                  f"{p['id']}: sidecar plugin_name {state['plugin_name']!r} != the "
                  f"installed file's PEP 723 name {hdr_name_val!r}")
            check(state["plugin_name"].endswith(f"v{p['version']}"),
                  f"{p['id']}: sidecar plugin_name {state['plugin_name']!r} does not "
                  f"show the installed version, so the Plugins dialog cannot either")

    # -- an older copy under a different folder name, plus a cloud copy
    p0 = plan[0]
    stray = root / "SupportFinsOld"
    stray.mkdir()
    (stray / p0["file"]).write_text("stale", encoding="utf-8")
    cloud = root / "_subscribed" / "abc123"
    cloud.mkdir(parents=True)
    (cloud / p0["file"]).write_text("cloud copy", encoding="utf-8")

    # -- second run: everything already there
    results = {p["id"]: install_one(p, root) for p in plan}
    check(all(v == "UPDATED" for v in results.values()),
          f"second run should report UPDATED for everything, got {results}")
    check((stray / p0["file"]).read_bytes() == (REPO / p0["path"]).read_bytes(),
          "a copy under a different folder name should have been updated too")
    check((cloud / p0["file"]).read_text(encoding="utf-8") == "cloud copy",
          "the _subscribed cloud copy must be left alone")

    # -- PLUGIN_ONLY narrows the plan
    only = build_plan(manifest, only=[ready[0]["id"]])
    check(len(only) == 1 and only[0]["id"] == ready[0]["id"], "PLUGIN_ONLY filtering is broken")


# --------------------------------------------------------------------------
if failures:
    report_and_exit()

print(f"ok -- catalogue, .bat fallback list, CRLF discipline and install replay "
      f"all agree ({len(ready)} ready plugin(s), {len(plugins)} listed)")
