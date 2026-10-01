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

bat_labels = {}
for _i, _l in enumerate(bat_lines, 1):
    _s = _l.strip()
    if _s.startswith(":") and not _s.startswith("::") and re.match(r":\w+\s*$", _s):
        bat_labels[_s[1:].strip().lower()] = _i

bat_refs: dict[str, list[int]] = {}
for _i, _l in enumerate(bat_lines, 1):
    if _l.strip().lower().startswith("rem "):
        continue
    for _m in re.finditer(r"\b(?:goto|call)\s+:(\w+)", _l, re.I):
        bat_refs.setdefault(_m.group(1).lower(), []).append(_i)

_dangling = {k: v for k, v in bat_refs.items() if k not in bat_labels}
check(not _dangling,
      f"the .bat jumps to labels that do not exist: {_dangling}. On Windows "
      f"that aborts the script mid-run.")
check(not (set(bat_labels) - set(bat_refs)),
      f"the .bat defines labels nothing jumps to: "
      f"{sorted(set(bat_labels) - set(bat_refs))}")

_bal = 0
_neg = []
for _i, _l in enumerate(bat_lines, 1):
    _s = _l.strip().lower()
    if _s.startswith("rem") or _s.startswith("::") or _s.startswith("echo"):
        continue
    _bal += re.sub(r'"[^"]*"', "", _l).count("(") - re.sub(r'"[^"]*"', "", _l).count(")")
    if _bal < 0:
        _neg.append(_i)
check(_bal == 0 and not _neg,
      f"unbalanced parentheses in the .bat (net {_bal}, negative at {_neg})")

# --- the self-updater ------------------------------------------------------
_set_uv = re.findall(r"(?m)^set UPDATER_VERSION=(\S+)$", "\n".join(bat_lines))
_rem_uv = re.findall(r"(?m)^rem UPDATER_VERSION (\S+) end$", "\n".join(bat_lines))
check(len(_set_uv) == 1 and len(_rem_uv) == 1,
      f"expected exactly one `set UPDATER_VERSION=` and one "
      f"`rem UPDATER_VERSION <v> end` line; got {_set_uv} and {_rem_uv}")
if len(_set_uv) == 1 and len(_rem_uv) == 1:
    check(_set_uv[0] == _rem_uv[0],
          f"the updater's two version lines disagree: set={_set_uv[0]} "
          f"rem={_rem_uv[0]}. :self_update compares the rem line, so a "
          f"mismatch makes it hand over to itself forever.")
    check(re.fullmatch(r"\d+\.\d+\.\d+", _set_uv[0]),
          f"UPDATER_VERSION {_set_uv[0]!r} is not x.y.z")

_bat_text = "\n".join(bat_lines)
_m_start = re.search(r"(?m)^:self_update$", _bat_text)
_m_end = re.search(r"(?m)^:stage_tools$", _bat_text)
check(_m_start is not None, "the .bat has no :self_update routine")
check(_m_end is not None, "the .bat has no :stage_tools routine")
_su = _bat_text[_m_start.end():_m_end.start()] if (_m_start and _m_end) else ""
for _guard, _why in (
        ("ORCA_UPDATER_CHILD", "the new copy would self-update again, forever"),
        ("NO_SELF_UPDATE", "--no-self-update would be ignored"),
        ("LOCAL_MODE", "--local would still hit the network")):
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
# 3e. test-branch chooser and strict branch isolation
# ---------------------------------------------------------------------------
# A normal double-click must remain released-main. The chooser is deliberately
# separate so its remembered test branch cannot surprise a normal updater run.
check(CHOOSER.exists(), "the double-click branch chooser is missing")
chooser_raw = CHOOSER.read_bytes() if CHOOSER.exists() else b""
chooser = chooser_raw.decode("utf-8", "replace")
check(chooser_raw.count(b"\r\n") == chooser_raw.count(b"\n") > 0,
      "the chooser .bat must use CRLF throughout")
check(chooser_raw.endswith(b"\r\n"), "the chooser .bat does not end with CRLF")
check('set "REF_1=main"' in bat and
      'if defined PLUGIN_BRANCH set "REF_1=%PLUGIN_BRANCH%"' in bat,
      "plain updater runs must default to main; only an explicit environment "
      "override may select a test branch")
check('set "STATE_FILE=%STATE_DIR%\\branch.txt"' in chooser,
      "the chooser no longer remembers its selection between runs")
check('set "PLUGIN_BRANCH=%CHOSEN%"' in chooser and
      'call "%UPDATER%"' in chooser,
      "the chooser does not pass its selected branch to the updater")
check('if /i "%PICK%"=="R" (set "CHOSEN=main"&goto :chosen)' in chooser,
      "the chooser has no obvious Return to released main choice")
check("api.github.com/repos/ajani190819-ops/Tests/branches?per_page=100" in chooser,
      "the chooser no longer fetches the public live GitHub branch list")
check("$b.commit.url" in chooser and "Sort-Object Date -Descending" in chooser,
      "the chooser does not fetch commit dates and sort test branches newest first")
check("$bs=Invoke-RestMethod" in chooser and "$bs=@(Invoke-RestMethod" not in chooser,
      "Windows PowerShell 5.1 would preserve GitHub's branch array as one nested "
      "System.Object[] and fail to convert the commit URL to System.Uri")
check("$commitUri=[string]$b.commit.url" in chooser and
      "-Uri $commitUri" in chooser,
      "the chooser does not force each GitHub commit URL to one string URI")
check("if !COUNT! LSS 6" in chooser and "Show all branches" in chooser,
      "the chooser must show main plus five recent branches and offer the full list")
check("Nothing will silently switch to another branch" in chooser and
      'if /i "%PICK%"=="R" goto :refresh' in chooser,
      "a GitHub API failure must offer retry/manual/cancel, never silently use main")
check('findstr /r /x "[A-Za-z0-9][A-Za-z0-9._/-]*"' in chooser and
      'findstr /c:".."' in chooser,
      "manually typed branch names are not validated before becoming a URL")

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
check("if defined BRANCH_MODE exit /b 0" in _su,
      "test mode self-update could replace the strict updater with an older copy from the branch")

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
