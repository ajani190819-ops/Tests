#!/usr/bin/env python3
"""Guard the contract of spatial-hud-template-26.3/Update-SpatialHUD.bat.

    python3 tests/test_spatialhud_installer.py

Update-SpatialHUD.bat is the Spatial HUD installer. It is one file, modelled on
Orca-Plugins.bat: menu, build picker, Modrinth folder picker, download check,
and install all live in it. Windows-only, so this cannot run it. What it CAN
check, without Windows, are the things that break in practice:

  * the file is CRLF and .gitattributes keeps git from re-normalising it
  * the version marker is present and numeric (self-update relies on it)
  * the release tag is built the same way the workflow names it
  * the jar name matches the workflow's published asset name
  * the build picker numbers the rows it prints and checks the same list
  * the download is verified (size and Spatial HUD mod id) before the mods
    folder is touched
  * the old jar is moved to a backup before the new one is copied in, and the
    choices are saved only after the copy succeeds
  * the installer does not depend on a second helper file any more
"""
from __future__ import annotations

import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
BAT = REPO / "spatial-hud-template-26.3" / "Update-SpatialHUD.bat"
OLD_HELPER = REPO / "spatial-hud-template-26.3" / "Update-SpatialHUD.ps1"
WORKFLOW = REPO / ".github" / "workflows" / "build-spatial-hud.yml"
GATTR = REPO / ".gitattributes"
FABRIC_JSON = REPO / "spatial-hud-template-26.3" / "src" / "main" / "resources" / "fabric.mod.json"

failures: list[str] = []


def check(cond: bool, msg: str) -> bool:
    if not cond:
        failures.append(msg)
    return cond


def first_index(lines: list[str], needle: str, start: int = 0) -> int:
    for i in range(start, len(lines)):
        if needle in lines[i]:
            return i
    return -1


def main() -> int:
    raw = BAT.read_bytes()
    text = raw.decode("utf-8").replace("\r\n", "\n")
    lines = text.split("\n")

    # --- line endings and single-file layout ---------------------------------
    check(b"\r\n" in raw, "Update-SpatialHUD.bat must have CRLF line endings")
    check(b"\n" not in raw.replace(b"\r\n", b""), "Update-SpatialHUD.bat has bare LF line endings")
    check(not OLD_HELPER.exists(), "Update-SpatialHUD.ps1 must be gone: the installer is one file")
    gattr = GATTR.read_text(encoding="utf-8")
    check(re.search(r"^\*\.bat\s+-text\s*$", gattr, re.M) is not None,
          ".gitattributes must keep '*.bat -text' so raw downloads stay CRLF")

    # --- version marker ------------------------------------------------------
    marker = [ln for ln in lines if ln.startswith("rem UPDATER_VERSION ")]
    check(len(marker) == 1, "exactly one 'rem UPDATER_VERSION <n> end' marker is required")
    if marker:
        m = re.fullmatch(r"rem UPDATER_VERSION (\d+) end", marker[0])
        check(m is not None, "UPDATER_VERSION marker must be 'rem UPDATER_VERSION <integer> end'")
        ver = re.search(r'^set "UPDATER_VERSION=(\d+)"$', text, re.M)
        check(ver is not None, 'set "UPDATER_VERSION=<integer>" line is missing')
        if m and ver:
            check(m.group(1) == ver.group(1), "the marker and set UPDATER_VERSION must match")

    # --- constants that must agree with the workflow and the mod -----------
    wf = WORKFLOW.read_text(encoding="utf-8")
    check('SAFE="${REF//\\//-}"' in wf, "workflow no longer derives SAFE as ${REF//\\//-}")
    check('BUILD_TAG="spatial-hud-build-$SAFE"' in wf, "workflow tag prefix changed")
    check('set "BUILD_PREFIX=spatial-hud-build-"' in text, "installer build prefix must match the workflow")
    check('set "LATEST_TAG=spatial-hud-latest"' in text, "installer latest tag must match the workflow")
    check('%BUILD:/=-%' in text, "installer must turn '/' in a branch name into '-' like the workflow")
    check('set "JAR_NAME=spatial-hud-1.0.0.jar"' in text, "installer jar name must match the workflow asset")
    check('spatial-hud-1.0.0.jar' in wf, "workflow no longer publishes spatial-hud-1.0.0.jar")
    fabric_id = re.search(r'"id":\s*"([^"]+)"', FABRIC_JSON.read_text(encoding="utf-8"))
    check(fabric_id is not None and fabric_id.group(1) == "spatialhud",
          "fabric.mod.json id must be 'spatialhud' (the installer checks it)")
    check("id -ne 'spatialhud'" in text, "installer must reject a jar whose mod id is not spatialhud")

    # --- menu ----------------------------------------------------------------
    for label in ("[1] Install or update", "[2] Choose the build", "[3] Change the Modrinth mods folder",
                  "[4] Forget my choices", "[Q] Quit"):
        check(label in text, f"menu item missing: {label}")
    check("Read-Host" not in text, "installer must use cmd set /p menus, not PowerShell prompts")
    check(text.count('set /p "PICK=') >= 3, "menus must use set /p choices")

    # --- build picker numbering ---------------------------------------------
    check('set /a COUNT=0' in text, "picker must count the rows it prints")
    check('set "MENU_!COUNT!=%%A"' in text, "picker must store each printed row by its number")
    check('echo   [!COUNT!] %%A - %%B' in text, "picker must print the number it stores")
    check('for /L %%I in (1,1,%COUNT%)' in text, "picker must check the typed number against the same list")
    check('set "CHOSEN=!MENU_%%I!"' in text, "picker must read the chosen row back from the stored list")
    check('LSS 6' in text, "picker must show at most six rows: main plus the five newest")
    check("Released main first" in text, "picker wording must say main comes first")

    # --- download is verified before the mods folder is touched -------------
    dl = first_index(lines, "Step 1 of 3")
    check(dl >= 0, "install must announce Step 1 (download and check)")
    check(first_index(lines, "Invoke-WebRequest -UseBasicParsing -Headers $h -Uri $j.browser_download_url") >= 0,
          "install must download the release asset")
    check("-ne [int64]$j.size" in text, "download must be compared with the size GitHub reported")
    check("'the download was incomplete'" in text, "an incomplete download must fail the check")
    check("GetEntry('fabric.mod.json')" in text, "the jar must be opened and its fabric.mod.json read")
    check("'the file is not the Spatial HUD mod'" in text, "a non-Spatial HUD jar must fail the check")

    move_i = first_index(lines, 'move /y "%MODS_DIR%\\spatial-hud-*.jar" "%BACKUP_DIR%\\"')
    copy_i = first_index(lines, 'copy /y "%JAR_TMP%" "%MODS_DIR%\\%JAR_NAME%"')
    save_i = first_index(lines, '>"%BUILD_STATE%" echo %BUILD%')
    check(move_i >= 0, "old jar must be moved to the backup folder, not deleted")
    check(copy_i >= 0, "new jar must be copied into the mods folder")
    check(save_i >= 0, "the build choice must be saved")
    if move_i >= 0 and copy_i >= 0:
        check(move_i < copy_i, "the old jar must be moved aside before the new one is copied in")
    if copy_i >= 0 and save_i >= 0:
        check(copy_i < save_i, "the choices must be saved only after the copy succeeds")
    check(first_index(lines, 'move /y "%BACKUP_DIR%\\spatial-hud-*.jar" "%MODS_DIR%\\"') >= 0,
          "a failed copy must put the old jar back")
    check("Step 1 of 3" in text and "Step 2 of 3" in text and "Step 3 of 3" in text,
          "install must show its three steps")

    # --- self update ---------------------------------------------------------
    check('set "SELF_REF=arena/b4016c28-tests"' in text,
          "self-update must read the session branch (change to main after merging)")
    check('findstr /b /c:"rem UPDATER_VERSION "' in text,
          "self-update must only run a copy that carries the UPDATER_VERSION marker")
    check("if %NEW_VER% LEQ %UPDATER_VERSION% goto :su_skip" in text,
          "self-update must only hand over to a strictly newer version")
    check("SPATIALHUD_CHILD" in text, "self-update must not loop")

    # --- balanced cmd parentheses (outside double quotes) -------------------
    depth = 0
    for n, ln in enumerate(lines, 1):
        if ln.lstrip().lower().startswith("rem "):
            continue
        in_q = False
        ln = ln.replace("echo(", "echo ")  # `echo(text` is a plain echo, not a block
        for ch in ln:
            if ch == '"':
                in_q = not in_q
            elif not in_q:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth < 0:
                        check(False, f"unbalanced ')' near line {n}")
                        depth = 0
    check(depth == 0, "unbalanced '(' in Update-SpatialHUD.bat")

    if failures:
        print("FAILED")
        for f in failures:
            print(" -", f)
        return 1
    print("ok: Update-SpatialHUD.bat contract holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
