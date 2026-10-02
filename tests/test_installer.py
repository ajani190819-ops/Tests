#!/usr/bin/env python3
"""Guard the contract between plugins.json and the unified updater.

    python3 test_installer.py

Orca-Plugins.bat is the ONE file a user downloads: menu, build picker,
OrcaSlicer folder picker, remembered choices, and install engine all live in
it (the old chooser + updater + launcher split ended at 2.0.0). The other two
.bat files are short forwarders kept so copies already on disk keep working.

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
  * the menu, the build picker (main + the five newest branches), the folder
    picker and the remembered choices are all present and wired together
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
# The one file: menu + build picker + folder picker + install engine.
BAT = REPO / "Orca-Plugins.bat"
# The old names, kept as forwarders so copies already on disk keep working.
FORWARDER = REPO / "Update-Orca-Plugins.bat"
CHOOSER = REPO / "Choose-Orca-Plugin-Version.bat"
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

        # The PEP 723 plugin name is a stable identity. Orca's development
        # guide says the full preset reference includes plugin_name as well as
        # capability_name, so putting a release number here can leave a preset
        # pointing at yesterday's identity. Version belongs in Orca's separate
        # Version column, Check setup, logs and G-code stamps.
        mname = re.search(r'^#\s*name\s*=\s*"([^"]+)"', head, re.MULTILINE)
        check(mname is not None, f"{pid}: no PEP 723 name header in the shipped plugin")
        if mname:
            want = p["name"]
            check(mname.group(1) == want,
                  f"{pid}: PEP 723 name is {mname.group(1)!r} but the stable "
                  f"catalogue identity is {want!r}. Never put the version in the "
                  f"plugin name; it can break saved preset references.")
            check(p["version"] not in mname.group(1),
                  f"{pid}: version {p['version']} leaked into the permanent plugin name")

        body = shipped.read_text(encoding="utf-8")

        # Contract copied from the repository's OrcaSlicer Plugin Development
        # PDF: one @orca.plugin package class, typed capability bases, required
        # execute signatures, and dependencies at the PEP 723 TOML root.
        check(body.count("@orca.plugin") == 1,
              f"{pid}: the PDF requires exactly one @orca.plugin class per file")
        check(re.search(r'@orca\.plugin\s+class\s+\w+\(orca\.base\):', body),
              f"{pid}: package class does not subclass orca.base as documented")
        check("def register_capabilities(self):" in body and
              body.count("orca.register_capability(") == len(p["capabilities"]),
              f"{pid}: registered capabilities do not match the catalogue")
        check("orca.slicing.SlicingPipelineCapabilityBase" in body and
              re.search(r'def execute\(self,\s*ctx\):', body),
              f"{pid}: slicing capability does not match the PDF signature")
        check("orca.script.ScriptPluginCapabilityBase" in body and
              re.search(r'def execute\(self\):', body),
              f"{pid}: Check setup does not match the PDF script signature")
        if pid == "wave-overhangs":
            deps_line = re.search(r'^# dependencies = \[([^\n]+)\]$', head, re.MULTILINE)
            table_at = head.find("# [tool.orcaslicer.plugin]")
            check(deps_line is not None and deps_line.start() < table_at,
                  "wave-overhangs: dependencies must be in the PEP 723 TOML root, "
                  "before [tool.orcaslicer.plugin]")
            if deps_line:
                check("numpy" in deps_line.group(1) and "shapely" in deps_line.group(1),
                      "wave-overhangs: Orca's uv installer was not told to install "
                      "both numpy and shapely")
            imports_at = [body.find("import numpy"), body.find("import shapely")]
            first_class = body.find("class WaveOverhangsSlicing")
            check(all(0 <= pos < first_class for pos in imports_at),
                  "wave-overhangs: dependencies must import at module load, not "
                  "inside an audited capability call")
            check("one transactional G-code pass" in body and
                  "uncovered fragments were retained" in body and
                  "no cross-callback geometry" in body and
                  "_PLAN" not in body and
                  "Safety kept Orca's" not in body,
                  "wave-overhangs: the shipped plugin does not enforce/report "
                  "transactional, geometry-bounded bridge replacement")

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
# The sidecar's plugin_name must match the stable PEP 723 name exactly. The
# version is a separate sidecar field; adding it to plugin_name changes the
# identity embedded in preset capability references.
check('call :write_state "%STATE_FILE%" "%PL_NAME%"' in bat and
      "PL_IDENTITY" not in bat,
      "the .bat must write permanent version-free package names to sidecars")
check('echo   [%PL_ACTION%] %PL_NAME% v%PL_VER% %PL_VERMSG%-- %PL_SIZE% bytes' in bat,
      "the .bat's install summary no longer reports the installed version")
check('"enabled": true' in bat,
      "the .bat no longer installs the plugins as enabled")

# The download stack: three methods, a size floor, and the sibling rules.
check("curl.exe -fLsS --retry 2" in bat, "the .bat lost its curl.exe download path")
check("Tls12" in bat and "Invoke-WebRequest" in bat, "the .bat lost its PowerShell download path")
check("bitsadmin /transfer" in bat, "the .bat lost its bitsadmin download path")
check('set "DL_MIN=2000"' in bat and "GEQ %DL_MIN%" in bat,
      "the .bat lost its default 2000-byte code download sanity floor")
check('call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%MANIFEST_PATH%" "%MANIFEST_TMP%" 100' in bat,
      "plugins.json still uses the 2000-byte plugin floor; the real catalogue is smaller")
check(MANIFEST.stat().st_size >= 100 and MANIFEST.stat().st_size < 2000,
      "the manifest regression fixture must prove why catalogue and plugin size floors differ")
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
# 3b. static analysis of the .bat -- it can never be executed in CI, so the
#     cheap structural mistakes have to be caught by reading it
# --------------------------------------------------------------------------
bat_lines = raw.decode("utf-8", "replace").split("\r\n")


def _static_bat_checks(path: pathlib.Path) -> None:
    """Label and parenthesis analysis for one .bat. cmd.exe cannot be run
    here, so a dead label or an unbalanced paren would otherwise only surface
    on the user's machine."""
    _lines = path.read_bytes().decode("utf-8", "replace").split("\r\n")

    _labels = {}
    for _i, _l in enumerate(_lines, 1):
        _s = _l.strip()
        if _s.startswith(":") and not _s.startswith("::") and re.match(r":\w+\s*$", _s):
            _labels[_s[1:].strip().lower()] = _i

    _refs: dict[str, list[int]] = {}
    for _i, _l in enumerate(_lines, 1):
        if _l.strip().lower().startswith("rem "):
            continue
        for _m in re.finditer(r"\b(?:goto|call)\s+:(\w+)", _l, re.I):
            _refs.setdefault(_m.group(1).lower(), []).append(_i)

    _dangling = {k: v for k, v in _refs.items() if k not in _labels}
    check(not _dangling,
          f"{path.name}: jumps to labels that do not exist: {_dangling}. On "
          f"Windows that aborts the script mid-run.")
    check(not (set(_labels) - set(_refs)),
          f"{path.name}: defines labels nothing jumps to: "
          f"{sorted(set(_labels) - set(_refs))}")

    _bal = 0
    _neg = []
    for _i, _l in enumerate(_lines, 1):
        _s = _l.strip().lower()
        if _s.startswith("rem") or _s.startswith("::") or _s.startswith("echo"):
            continue
        _bal += re.sub(r'"[^"]*"', "", _l).count("(") - re.sub(r'"[^"]*"', "", _l).count(")")
        if _bal < 0:
            _neg.append(_i)
    check(_bal == 0 and not _neg,
          f"{path.name}: unbalanced parentheses (net {_bal}, negative at {_neg})")

    # Every version surface in the file must agree with itself. A `rem ... end`
    # marker is what copies already on disk compare against, so a pair that
    # disagrees makes a file hand over to itself forever.
    _set_uv = re.findall(r"(?m)^set UPDATER_VERSION=(\S+)$", "\n".join(_lines))
    _rem_uv = re.findall(r"(?m)^rem UPDATER_VERSION (\S+) end$", "\n".join(_lines))
    check(len(_set_uv) <= 1 and len(_rem_uv) <= 1 and len(_set_uv) == len(_rem_uv),
          f"{path.name}: expected at most one `set UPDATER_VERSION=` and one "
          f"`rem UPDATER_VERSION <v> end` line; got {_set_uv} and {_rem_uv}")
    if _set_uv and _rem_uv:
        check(_set_uv[0] == _rem_uv[0],
              f"{path.name}: the UPDATER_VERSION lines disagree: set={_set_uv[0]} "
              f"rem={_rem_uv[0]}. :self_update compares the rem line, so a "
              f"mismatch makes it hand over to itself forever.")
        check(re.fullmatch(r"\d+\.\d+\.\d+", _set_uv[0]),
              f"{path.name}: UPDATER_VERSION {_set_uv[0]!r} is not x.y.z")
    _set_fv = re.findall(r'(?m)^set "FRONTDOOR_VERSION=([^"]+)"$', "\n".join(_lines))
    _rem_fv = re.findall(r"(?m)^rem FRONTDOOR_VERSION (\S+) end$", "\n".join(_lines))
    if _set_fv or _rem_fv:
        check(len(_set_fv) == 1 and len(_rem_fv) == 1 and _set_fv[0] == _rem_fv[0],
              f"{path.name}: the FRONTDOOR_VERSION markers must appear exactly "
              f"once each and agree: set={_set_fv} rem={_rem_fv}")


for _bat_path in (BAT, FORWARDER, CHOOSER):
    if _bat_path.exists():
        _static_bat_checks(_bat_path)

# --- the self-updater ------------------------------------------------------
_bat_text = "\n".join(bat_lines)
_m_start = re.search(r"(?m)^:self_update$", _bat_text)
_m_end = re.search(r"(?m)^:stage_tools$", _bat_text)
check(_m_start is not None, "the .bat has no :self_update routine")
check(_m_end is not None, "the .bat has no :stage_tools routine")
_su = _bat_text[_m_start.end():_m_end.start()] if (_m_start and _m_end) else ""
for _guard, _why in (
        ("ORCA_FRONTDOOR_CHILD", "a handover from an old launcher would self-update again, forever"),
        ("ORCA_UPDATER_CHILD", "a handover from an old updater would self-update again, forever"),
        ("NO_SELF_UPDATE", "--no-self-update would be ignored"),
        ("LOCAL_MODE", "--local would still hit the network"),
        ("BRANCH_MODE", "test mode self-update could replace the strict updater with an older copy")):
    check(re.search(rf"(?m)^if defined {_guard} exit /b 0$", _su),
          f":self_update lost its `if defined {_guard} exit /b 0` line -- {_why}")

# Verify-before-execute: every bail-out between downloading the replacement and
# running it is what stops a 404 page or a wifi portal from being executed.
_verify = re.findall(r"(?m)^findstr .*%NEWBAT%.*$", _su)
_bail = re.findall(r"(?m)^if (?:not )?errorlevel 1 goto :su_skip$", _su)
check(len(_verify) >= 3 and len(_bail) >= 3,
      f":self_update must verify the download before running it; found "
      f"{len(_verify)} findstr check(s) and {len(_bail)} bail-out(s), want 3+ of "
      f"each (is it our .bat? is it a different version?)")
_call_at = _su.find('call "%NEWBAT%"')
check(_call_at != -1, ":self_update never hands over to the downloaded copy")
if _call_at != -1:
    check(_su[:_call_at].count("goto :su_skip") >= 3,
          "the verification bail-outs must all come BEFORE the handover, "
          "otherwise an unverified file gets executed first")
# The unified updater must never hand a run back to the old two-file layout.
# Before 2.0.0, main's Orca-Plugins.bat was the launcher alone and carries no
# UPDATER_VERSION marker -- so while a unified build lives on a test branch
# and main still holds the old layout, "a different version" can mean "an
# older file". The handover must require the UPDATER_VERSION marker too:
# proof the download is itself the one-file updater.
_layout_guard = _su.find('findstr /b /c:"rem UPDATER_VERSION " "%NEWBAT%"')
check(_layout_guard != -1 and _call_at != -1 and _layout_guard < _call_at,
      ":self_update may hand over to a download without the UPDATER_VERSION "
      "marker -- a unified copy running from a test branch would hand the "
      "run back to main's old two-file launcher and quietly undo the "
      "unification for that run")
# A running .bat must never be overwritten in place: cmd.exe reads it by byte
# offset and will execute garbage. The design deliberately delegates instead.
check(not re.search(r"(?mi)^\s*(copy|move|xcopy)\b[^\r\n]*%~f0", "\n".join(bat_lines)),
      "the .bat overwrites itself while running -- cmd.exe streams a batch file "
      "from disk as it executes, so this can jump into garbage mid-run")

# --- staged standalone tools ----------------------------------------------
_tools = re.findall(r'call :stage_one_tool "([^"]+)" "([^"]+)"', "\n".join(bat_lines))
check(_tools, "the .bat no longer stages any standalone tool")
for _tpath, _tname in _tools:
    check((REPO / _tpath).exists(),
          f"the .bat stages {_tpath!r}, which is not in the repo -- the download "
          f"would 404")
    check(_tpath.endswith("/" + _tname),
          f"staged tool name {_tname!r} does not match its path {_tpath!r}")
    check("\\" not in _tpath,
          f"staged tool path {_tpath!r} must use forward slashes for a URL")


# --------------------------------------------------------------------------
# 3c. the standalone tools themselves
# --------------------------------------------------------------------------
for _tpath, _tname in _tools:
    _tf = REPO / _tpath
    if not _tf.exists():
        continue
    _tsrc = _tf.read_text(encoding="utf-8")
    _tv = re.search(r'(?m)^TOOL_VERSION\s*=\s*"([^"]+)"', _tsrc)
    check(_tv is not None, f"{_tname} has no TOOL_VERSION")
    # it must agree with the plugin it shares an engine with
    _sib = _tf.parent
    _plug = next((q for q in _sib.glob("*_orca.py")), None)
    if _tv and _plug is not None:
        _pv = re.search(r'(?m)^PLUGIN_VERSION\s*=\s*"([^"]+)"',
                        _plug.read_text(encoding="utf-8"))
        check(_pv and _tv.group(1) == _pv.group(1),
              f"{_tname} is v{_tv.group(1)} but {_plug.name} is "
              f"v{_pv.group(1) if _pv else '?'} -- they share an engine and must "
              f"share a version")
    check(re.search(r'add_argument\([^)]*"--inplace"', _tsrc, re.S),
          f"{_tname} does not register an --inplace argument, so it cannot be "
          f"used as a slicer post-processing script")
    check(re.search(r'add_argument\([^)]*"--version"', _tsrc, re.S),
          f"{_tname} has no --version flag, so the owner cannot check which "
          f"copy they are running")


# --------------------------------------------------------------------------
# 3c2. every plugin has a changelog, and it is in step with the version
# ---------------------------------------------------------------------------
# The changelog is what the owner reads in the Plugins dialog (via Check
# setup) and on GitHub. A version bump without a changelog entry is the
# easiest thing in the world to forget, so it is a hard failure here.
CHANGELOG_HEAD = re.compile(r"^##\s+(\d+\.\d+\.\d+)\s*[\u2014\u2013-]\s*(\S+)\s*$",
                            re.MULTILINE)
for p_ in manifest["plugins"]:
    cl = REPO / "plugins" / p_["id"] / "CHANGELOG.md"
    if not check(cl.exists(), f"{p_['id']}: no CHANGELOG.md"):
        continue
    heads = CHANGELOG_HEAD.findall(cl.read_text(encoding="utf-8"))
    if not check(bool(heads), f"{p_['id']}: CHANGELOG.md has no version headings"):
        continue
    newest = heads[0][0]
    check(newest == p_["version"],
          f"{p_['id']}: newest CHANGELOG.md entry is {newest} but the "
          f"catalogue ships {p_['version']}. Write the changelog entry.")
    versions = [h[0] for h in heads]
    check(len(versions) == len(set(versions)),
          f"{p_['id']}: duplicate versions in CHANGELOG.md: {versions}")
    # the plugin carries a copy of it for Check setup to print
    src = (REPO / p_["path"]).read_text(encoding="utf-8")
    check("CHANGELOG_RECENT" in src,
          f"{p_['id']}: the plugin does not carry CHANGELOG_RECENT, so Check "
          f"setup cannot show the owner what changed")
    check(f"v{newest}" in src.split("CHANGELOG_RECENT", 1)[-1][:4000],
          f"{p_['id']}: the embedded changelog does not start at v{newest}. "
          f"Run: python3 tools/sync_changelog.py")

# a project-wide changelog must exist and mention the current versions
root_cl = REPO / "CHANGELOG.md"
if check(root_cl.exists(), "no project-wide CHANGELOG.md at the repo root"):
    rc = root_cl.read_text(encoding="utf-8")
    for p_ in manifest["plugins"]:
        check(p_["version"] in rc,
              f"CHANGELOG.md never mentions {p_['id']} {p_['version']}")

# ---------------------------------------------------------------------------
# 3d. the staging folder must not mix tools in with the plugin copies
# ---------------------------------------------------------------------------
# %DLDIR% is the folder the README tells people to point Orca's installer at.
# Orca wants exactly one entry .py per plugin folder, so the standalone tools
# (which are not plugins) must be staged somewhere else.
bat_txt = BAT.read_text(encoding="utf-8", errors="replace")
check('set "TOOLDIR=' in bat_txt,
      "the .bat no longer defines TOOLDIR; standalone tools would land beside "
      "the plugin copies and make the plugin entry file ambiguous")
tool_dl = re.search(r':stage_one_tool\s*\r?\n\s*call :download\s+"[^"]+"\s+"([^"]+)"',
                    bat_txt)
check(tool_dl is not None, "could not find the :stage_one_tool download line")
if tool_dl:
    check("%TOOLDIR%" in tool_dl.group(1),
          f"standalone tools are staged to {tool_dl.group(1)!r}, which is the "
          f"plugin staging folder. Use %TOOLDIR%.")

# ---------------------------------------------------------------------------
# 3e. the one file, and the two old names still working
# ---------------------------------------------------------------------------
# Orca-Plugins.bat is the single file a user downloads: the menu, the build
# picker, the OrcaSlicer folder picker, the remembered choices and the whole
# install engine live in it. The old engine filename stays reachable as a
# forwarder so copies already on disk keep self-updating.

# A normal double-click must stay on released main, and only an explicit
# choice may move off it.
check('set "REF_1=main"' in bat and
      'if defined PLUGIN_BRANCH set "REF_1=%PLUGIN_BRANCH%"' in bat,
      "plain updater runs must default to main; only an explicit environment "
      "override may select a test branch")
check('set "REMEMBERED=main"' in bat and
      'if exist "%BRANCH_STATE%" set /p REMEMBERED=<"%BRANCH_STATE%"' in bat,
      "the updater no longer remembers the chosen build between runs")
check('set "DATADIR_STATE=%STATE_DIR%\\datadir.txt"' in bat,
      "the updater no longer knows about the remembered OrcaSlicer folder")
check('if defined REMEMBERED_DIR if exist "%REMEMBERED_DIR%"' in bat,
      "a remembered OrcaSlicer folder that still exists must be used without "
      "asking -- that is the whole point of remembering it")

# The menu must offer both pickers, and the folder picker must be reachable
# on its own, not only as part of an install.
check('echo   [2] Choose the build' in bat and
      'echo   [3] Choose which OrcaSlicer folder' in bat,
      "the menu no longer offers the build picker and the OrcaSlicer folder picker")
check('call :select_data_dir "" ask' in bat,
      "menu item 3 must force the interactive folder picker instead of "
      "silently reusing the remembered folder")
check('if defined PLUGIN_BRANCH ( set "CHOSEN=%REF_1%" & goto :install )' in bat,
      "PLUGIN_BRANCH must skip the menu and install from that ref directly -- "
      "that is how the forwarders and the old two-file copies drive this one")
check('set "REF_1=%CHOSEN%"' in bat,
      "the build chosen in the menu must drive the install ref")

# The build picker: live GitHub branch list, main pinned first, the five
# newest test branches, and never a silent fallback to another build.
check('set "REPO=ajani190819-ops/Tests"' in bat,
      "the updater points at the wrong repository")
check("api.github.com/repos/%REPO%/branches?per_page=100" in bat or
      "api.github.com/repos/ajani190819-ops/Tests/branches?per_page=100" in bat,
      "the build picker no longer fetches the public live GitHub branch list")
check("$b.commit.url" in bat and "Sort-Object Date -Descending" in bat,
      "the build picker does not fetch commit dates and sort test builds newest first")
check("$bs=Invoke-RestMethod" in bat and "$bs=@(Invoke-RestMethod" not in bat,
      "Windows PowerShell 5.1 would preserve GitHub's branch array as one nested "
      "System.Object[] and fail to convert the commit URL to System.Uri")
check("([string]$b.commit.url)" in bat,
      "the build picker does not force each GitHub commit URL to one string URI")
check("if !COUNT! LSS 6" in bat,
      "the build picker must show main plus the five newest test branches")

# Self-update: fetch, verify, hand over, never rewrite the running file.
# `bat` is LF-normalised by read_text, so anchor without \r.
_fv_rem = re.search(r"(?m)^rem FRONTDOOR_VERSION (\S+) end$", bat)
_fv_set = re.search(r'(?m)^set "FRONTDOOR_VERSION=([^"]+)"$', bat)
check(_fv_rem and _fv_set and _fv_rem.group(1) == _fv_set.group(1),
      "the updater's two FRONTDOOR_VERSION markers must agree: "
      f"rem says {_fv_rem.group(1) if _fv_rem else 'MISSING'}, "
      f"set says {_fv_set.group(1) if _fv_set else 'MISSING'}. A copy already "
      "on disk compares the rem marker to decide whether to hand over, so "
      "both must move together or a fix never reaches anyone.")
_uv_rem = re.search(r"(?m)^rem UPDATER_VERSION (\S+) end$", bat)
check(_fv_rem and _uv_rem and _fv_rem.group(1) == _uv_rem.group(1),
      "the FRONTDOOR_VERSION and UPDATER_VERSION markers must stay equal: "
      "the launcher and the engine became one file at 2.0.0, so one file "
      "means one version number.")
check('set "SELF_REF=%REMEMBERED%"' in bat and
      'if defined PLUGIN_BRANCH set "SELF_REF=%REF_1%"' in bat and
      'call :download "https://raw.githubusercontent.com/%REPO%/%SELF_REF%/Orca-Plugins.bat"'
      in bat,
      "the updater must self-update from the build the user actually chose")
check('findstr /b /c:"rem FRONTDOOR_VERSION " "%NEWBAT%"' in bat and
      'if defined ORCA_FRONTDOOR_CHILD exit /b 0' in bat,
      "the updater's self-update must verify the download and not recurse")
# Compare lowercase against lowercase: the variable names are uppercase in
# the source, so an uppercase needle against a lowered haystack never matches
# and the check would silently pass.
_bat_lower = bat.lower()
for marker in ('copy /y "%newbat%"', 'move /y "%newbat%"', '"%~f0"'):
    check(marker not in _bat_lower,
          f"the updater must never overwrite itself while running ({marker})")

# A redirection must not be swallowed by an unclosed quote.
#
# cmd.exe decides what is a redirection and what is plain text by toggling a
# quoting flag on EVERY `"` it meets. It does not understand `\"` as an
# escape. So `findstr /c:\"set \\\"NAME=\" \"%F%\" >nul 2>nul` holds five quotes,
# leaves cmd inside a quoted string at the end of the line, and hands `>nul`
# and `2>nul` to findstr as filenames. The user sees:
#
#     FINDSTR: Cannot open >nul
#     FINDSTR: Cannot open 2>nul
#
# ...plus the matched line printed to the screen, because the output was
# never redirected. That shipped in front door 1.0.0 and was reported from a
# real Windows run; nothing here could catch it, because cmd.exe cannot be
# run in this sandbox. This walks each line the way cmd.exe does instead.
for _bat_path in (BAT, FORWARDER, CHOOSER):
    if not _bat_path.exists():
        continue
    for _n, _raw in enumerate(
            _bat_path.read_bytes().split(b"\r\n"), 1):
        _line = _raw.decode("utf-8", "replace")
        _stripped = _line.strip()
        if not _stripped or _stripped.lower().startswith("rem") \
                or _stripped.startswith("::"):
            continue
        _inside = False
        _bad = False
        for _k, _ch in enumerate(_line):
            if _ch == '"':
                _inside = not _inside
            elif _inside and _ch in "<>|" and _line[_k - 1:_k] != "^":
                # A redirection or pipe character inside quotes is only
                # legitimate when it is genuinely part of a string being
                # echoed or set; those do not end in `>nul`-style plumbing.
                if re.search(r"[<>|]\s*(nul|&\d|\d>)", _line[_k:], re.I):
                    _bad = True
        check(not _bad,
              f"{_bat_path.name}:{_n}: a redirection is inside an unclosed "
              f"quote, so cmd.exe will pass it as an argument instead of "
              f"redirecting. Count the quotes -- cmd toggles on every one and "
              f'does not honour \\" as an escape:\n    {_stripped}')

# Every goto/call target must exist. cmd.exe cannot be run here, so a dead
# label would otherwise only surface on the user's machine.
for _bat_path in (BAT, FORWARDER, CHOOSER):
    if not _bat_path.exists():
        continue
    _t = _bat_path.read_bytes().decode("ascii", "replace")
    _labels = {m.group(1).lower()
               for m in re.finditer(r"^\s*:([A-Za-z_]\w*)", _t, re.M)}
    _jumps = {m.group(1).lower()
              for m in re.finditer(r"\b(?:goto|call)\s+:([A-Za-z_]\w*)", _t, re.I)}
    check(not (_jumps - _labels),
          f"{_bat_path.name}: jumps to labels that do not exist: "
          f"{sorted(_jumps - _labels)}")

# The old chooser filename must keep working as a pure forwarder.
check(CHOOSER.exists(), "the old Choose-Orca-Plugin-Version.bat must stay as a forwarder")
chooser_raw = CHOOSER.read_bytes() if CHOOSER.exists() else b""
chooser = chooser_raw.decode("utf-8", "replace")
check(chooser_raw.count(b"\r\n") == chooser_raw.count(b"\n") > 0,
      "the forwarder .bat must use CRLF throughout")
check(chooser_raw.endswith(b"\r\n"), "the forwarder .bat does not end with CRLF")
check('call "%FRONTDOOR%" %*' in chooser,
      "the chooser forwarder must hand the whole run, arguments and all, to the one file")
check("/main/Orca-Plugins.bat" in chooser and
      'findstr /b /c:"rem FRONTDOOR_VERSION " "%FRONTDOOR%"' in chooser,
      "the chooser forwarder must fetch and verify Orca-Plugins.bat when it is not alongside")
# It must not have kept a second copy of the install logic.
check("plugins.json" not in chooser and ":install_one" not in chooser and
      len(chooser_raw) < 4000,
      "the chooser forwarder must not carry its own installer logic")

# The old updater filename must keep working as a forwarder too, and it must
# still satisfy the verification that copies of the OLD two-file updater and
# launcher perform on this exact URL:
#   * an old Update-Orca-Plugins.bat (<= 1.4.0) hands its run over to whatever
#     newer version it finds at this URL, gated on `rem UPDATER_VERSION <v>
#     end` and `set UPDATER_VERSION=` lines;
#   * an old Orca-Plugins.bat launcher (<= 1.0.1) downloads this URL as its
#     "engine" and refuses to run it unless it is >= 2000 bytes and contains
#     `set UPDATER_VERSION=` and `if defined PLUGIN_BRANCH set`.
# Strip any of those markers and every copy already on disk either strands on
# an old version forever or stops with "NOTHING WAS INSTALLED".
check(FORWARDER.exists(), "the old Update-Orca-Plugins.bat must stay as a forwarder")
fwd_raw = FORWARDER.read_bytes() if FORWARDER.exists() else b""
fwd = fwd_raw.decode("utf-8", "replace")
check(fwd_raw.count(b"\r\n") == fwd_raw.count(b"\n") > 0,
      "the Update-Orca-Plugins.bat forwarder must use CRLF throughout")
check(fwd_raw.endswith(b"\r\n"), "the forwarder does not end with CRLF")
check('call "%FRONTDOOR%" %*' in fwd,
      "the forwarder must hand the whole run, arguments and all, to the one file")
check("raw.githubusercontent.com/%REPO%/%FWD_REF%/Orca-Plugins.bat" in fwd and
      'findstr /b /c:"rem FRONTDOOR_VERSION " "%FRONTDOOR%"' in fwd,
      "the forwarder must fetch and verify Orca-Plugins.bat when it is not alongside")
check('if defined PLUGIN_BRANCH set "FWD_REF=%PLUGIN_BRANCH%"' in fwd,
      "the forwarder must fetch the build an old launcher asked for via "
      "PLUGIN_BRANCH, never silently main")
check(len(fwd_raw) >= 2000,
      "the forwarder is smaller than the 2000-byte floor old launchers check")
_f_uv = re.search(r"(?m)^rem UPDATER_VERSION (\S+) end\r?$", fwd)
check(_f_uv is not None and _fv_rem is not None and _f_uv.group(1) == _fv_rem.group(1),
      "the forwarder's UPDATER_VERSION must equal Orca-Plugins.bat's version: "
      "an old updater hands its run to this file, so it must describe the "
      "same release it forwards to")
check("plugins.json" not in fwd and ":install_one" not in fwd and
      len(fwd_raw) < 4000,
      "the forwarder must not carry its own installer logic")


# The old bug was REF_1 -> REF_2(main) fallback for both manifests and plugin
# files. A test build must now be one ref only, and a bad catalogue must stop.
check('set "REF_2="' in bat,
      "the updater still configures a second ref; test builds could mix with main")
_try_start = _bat_text.find(":try_download")
_try_end = _bat_text.find(":fetch_manifest")
_try_body = _bat_text[_try_start:_try_end]
check("%REF_2%" not in _try_body and ":try_ref2" not in _try_body,
      ":try_download still falls back to REF_2/main")
_fm_start = _bat_text.find(":fetch_manifest")
_fm_end = _bat_text.find(":preflight_branch")
_fm_body = _bat_text[_fm_start:_fm_end]
check("%REF_2%" not in _fm_body and ":fm_ref2" not in _fm_body,
      "manifest fetch still falls back to REF_2/main")
check("if not defined PLAN_SRC if defined BRANCH_MODE goto :branch_manifest_failed" in bat and
      "if not defined PLAN_MADE if defined BRANCH_MODE goto :branch_manifest_failed" in bat,
      "a missing or invalid test-branch catalogue does not stop the install")

# All-or-nothing means preflight must occur before the install loop and retain
# a failure from ANY plugin (not merely whichever plugin was checked last).
_preflight_call = bat.find("call :preflight_branch")
_install_loop = bat.find('for /f "usebackq tokens=1-9 delims=|" %%A in ("%PLAN_FILE%") do (')
_select_data = bat.find('call :select_data_dir "%DATA_DIR_ARG%"')
check(0 <= _preflight_call < _select_data < _install_loop and
      "if not defined BRANCH_MODE goto :preflight_done" in bat,
      "test-branch preflight must finish before Orca folders are selected or "
      "the first plugin is installed")
_plan_report = bat.find("echo  Plan: %PLAN_COUNT% plugin(s).")
check(_install_loop < _plan_report and
      'if "%PLAN_COUNT%"=="0" goto :no_plan' in bat[_install_loop:_plan_report] and
      ":branch_manifest_failed" not in bat[_install_loop:_plan_report],
      "the normal install loop must reach its Plan summary; a branch-failure "
      "label inserted here would make every successful run fail")
check(bat.count("\n:branch_manifest_failed\n") == 1,
      "expected exactly one branch-manifest failure routine")
check('set "PREFLIGHT_FAILED=1"' in bat and
      "if defined PREFLIGHT_FAILED exit /b 1" in bat,
      "preflight does not remember an early missing plugin; a later success could hide it")
check('if defined BRANCH_MODE if exist "%PREFLIGHT_DIR%\\%PL_FILE%"' in bat,
      "branch installs do not use the files that passed all-or-nothing preflight")
check("This updater will NOT use main" in bat,
      "the strict branch failure does not plainly tell the user that main was not used")

# The chosen build is only written to the remembered-build file after that
# build's catalogue downloaded and parsed, so a dead branch name can never be
# persisted and strand the next run on a build that 404s.
write_at = bat.index('>"%BRANCH_STATE%" echo %CHOSEN%')
check(0 <= bat.index("call :fetch_manifest") < write_at and
      bat.index("if not defined PLAN_MADE if defined BRANCH_MODE goto :branch_manifest_failed")
      < write_at,
      "the chosen build must not be remembered before its catalogue downloads; "
      "there is no engine download to gate on any more, so the catalogue is "
      "the proof the branch is real")

# The branch and versions must be hard to miss at both ends. The end summary is
# populated only after a plugin is successfully copied, using the downloaded
# file's header version rather than the catalogue's claim.
check(bat.count("***************************************************************") >= 8 and
      "* BRANCH: %REF_1%" in bat and "* BUILD TO INSTALL -- branch: %REF_1%" in bat,
      "the selected branch is not printed loudly at the start")
check("* INSTALLED FROM BRANCH: %REF_1%" in bat and
      '>> "%INSTALLED_SUMMARY%" echo    %PL_NAME% v%PL_VER%' in bat and
      'type "%INSTALLED_SUMMARY%"' in bat,
      "the final banner does not report the actual installed plugin versions")

# A small install replay for the new failure path: if the second file is absent,
# preflight returns no staged set and therefore installation has not begun.
def replay_branch_preflight(plan: list[dict], available: set[str]) -> tuple[bool, list[str]]:
    staged = []
    for plugin in plan:
        if plugin["path"] not in available:
            return False, []
        staged.append(plugin["path"])
    return True, staged

_branch_paths = {p["path"] for p in ready}
_ok, _staged = replay_branch_preflight(ready, _branch_paths)
check(_ok and set(_staged) == _branch_paths,
      "branch preflight replay should stage a complete branch")
if len(ready) >= 2:
    _ok, _staged = replay_branch_preflight(ready, {ready[0]["path"]})
    check(not _ok and not _staged,
          "a missing later branch plugin must leave nothing ready to install")


# ---------------------------------------------------------------------------
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
        # :install_one writes the permanent version-free PEP 723/config identity.
        # Release numbers belong only in installed_version.
        f'  "plugin_name": "{p["name"]}"\n'
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
            want_identity = p["name"]
            check(state["plugin_name"] == want_identity,
                  f"{p['id']}: sidecar plugin_name must preserve {want_identity!r}; "
                  f"got {state['plugin_name']!r}")

        # Orca requires a plugin folder to contain EXACTLY ONE entry file
        # (one .py or one .whl): find_installed_plugin_entry in
        # PythonFileUtils.cpp picks the entry point by scanning the folder.
        # Two .py files there and the plugin does not load.
        entries = sorted(q.name for q in (root / p["orca_dir"]).glob("*.py"))
        check(len(entries) == 1,
              f"{p['id']}: the plugin folder holds {len(entries)} .py files "
              f"({entries}). Orca needs exactly one entry file per folder.")
        whls = sorted(q.name for q in (root / p["orca_dir"]).glob("*.whl"))
        check(not whls, f"{p['id']}: unexpected .whl beside the entry file: {whls}")

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
