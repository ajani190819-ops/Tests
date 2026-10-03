@echo off
setlocal EnableExtensions EnableDelayedExpansion
title OrcaSlicer Plugins - install or update

rem ===========================================================================
rem  Orca-Plugins.bat -- the one file: menu, pickers, and install engine.
rem ===========================================================================
rem
rem  Double-click it and press Enter. It installs or updates every OrcaSlicer
rem  plugin in this repo into Orca's own plugin folder.
rem
rem  It used to be three files -- a chooser, an updater engine and a launcher.
rem  From 2.0.0 they are one, and from 2.1.0 the old two filenames are gone
rem  from the repository entirely, so this is the only updater file. It holds
rem  the menu, the build picker (released main or one of the five newest test
rem  branches), the OrcaSlicer folder picker, the remembered choices, and the
rem  full install engine underneath.
rem
rem  Copies of the old files already on disk are not stranded: an old
rem  Orca-Plugins.bat launcher self-updates straight into this file, and an
rem  old Update-Orca-Plugins.bat keeps installing from main exactly as it
rem  always did -- it simply never updates itself again.
rem
rem  It remembers the two things you would otherwise retype every time: which
rem  build you want -- released or a test branch -- and which OrcaSlicer data
rem  folder to install into. Both are shown at the top of the menu, and both
rem  can be changed from it.
rem
rem  It needs no Python, Node or Git: it downloads the ready-built plugin files
rem  from the public GitHub repository below and copies them into your
rem  OrcaSlicer data folder, then refreshes every other copy it finds there.
rem
rem  Usage:
rem    Orca-Plugins.bat                     menu; Enter installs the
rem                                         remembered build into the
rem                                         remembered folder
rem    Orca-Plugins.bat "C:\...\OrcaSlicer" install into that data folder
rem    Orca-Plugins.bat --local             use plugin files next to this .bat
rem    Orca-Plugins.bat --help
rem
rem  Environment overrides:
rem    ORCA_DATA_DIR        Orca data directory (same as the first argument)
rem    PLUGIN_BRANCH        git ref to install from; skips the menu entirely
rem    PLUGIN_ONLY          comma-separated plugin ids, e.g. wave-overhangs
rem    ORCA_NO_SELF_UPDATE  never hand over to a newer copy of this updater
rem
rem  How it decides what to install:
rem    1. fetch plugins.json -- the catalogue -- from REPO at ref REF_1
rem    2. inline PowerShell turns the catalogue into a pipe-delimited plan
rem    3. released main can use the hardcoded list if its catalogue is down;
rem       a test branch instead stops before changing anything. The fallback
rem       list is kept in sync by test_installer.py
rem
rem  The repo must stay public: downloads are unauthenticated
rem  raw.githubusercontent.com requests, and a private repo 404s every file.
rem
rem  This file must keep CRLF line endings. *.bat -text in .gitattributes
rem  keeps git from re-normalising them; do not let an editor (or Python's
rem  Path.write_text) convert them to LF.
rem
rem  Windows shows an "Unknown Publisher" prompt the first time a downloaded
rem  .bat runs. That is expected for every .bat on the internet, not a warning
rem  about this one; right-click the file, Properties, tick Unblock, and it
rem  never appears again.
rem ===========================================================================

rem This file's own version. The launcher and the engine used to be two files
rem with two version numbers; they are one file now, so there is one version.
rem The four lines below are the machine-readable copies :self_update and the
rem old two-file launcher compares against; tests/test_installer.py keeps
rem all four equal. Never bump one without the others.
rem FRONTDOOR_VERSION 2.1.0 end
set "FRONTDOOR_VERSION=2.1.0"
rem UPDATER_VERSION 2.1.0 end
set UPDATER_VERSION=2.1.0

set "REPO=ajani190819-ops/Tests"
set "MANIFEST_PATH=plugins.json"
set "REF_1=main"
if defined PLUGIN_BRANCH set "REF_1=%PLUGIN_BRANCH%"
set "REF_2="
set "BRANCH_MODE="
if /i not "%REF_1%"=="main" set "BRANCH_MODE=1"

rem Where the remembered build and folder live between runs.
set "STATE_DIR=%LOCALAPPDATA%\OrcaPluginUpdater"
set "BRANCH_STATE=%STATE_DIR%\branch.txt"
set "DATADIR_STATE=%STATE_DIR%\datadir.txt"
set "BRANCHES=%TEMP%\orca_branches_%RANDOM%.txt"

set "REMEMBERED=main"
if exist "%BRANCH_STATE%" set /p REMEMBERED=<"%BRANCH_STATE%"
if not defined REMEMBERED set "REMEMBERED=main"
set "REMEMBERED_DIR="
if exist "%DATADIR_STATE%" set /p REMEMBERED_DIR=<"%DATADIR_STATE%"

rem Kept whole because :parse_args shifts them away, and :self_update has to
rem hand the same arguments to the newer copy.
set "ORIG_ARGS=%*"

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

rem A copy of each downloaded file lands here for Orca's UI installer.
set "DLDIR=%USERPROFILE%\Downloads\OrcaPlugins"

rem Standalone tools live in their own subfolder. They are NOT plugins, and a
rem plugin folder must contain exactly one entry .py or Orca cannot tell which
rem file is the plugin.
set "TOOLDIR=%DLDIR%\tools"

set "LOCAL_MODE="
set "DATA_DIR_ARG="
set "NO_SELF_UPDATE="
if defined ORCA_NO_SELF_UPDATE set "NO_SELF_UPDATE=1"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--help" goto :help
if /i "%~1"=="-h" goto :help
if /i "%~1"=="--local" ( set "LOCAL_MODE=1" ) else if /i "%~1"=="--no-self-update" ( set "NO_SELF_UPDATE=1" ) else if not defined DATA_DIR_ARG set "DATA_DIR_ARG=%~1"
shift
goto :parse_args
:args_done

call :self_update
if defined SELF_UPDATED goto :child_done

rem PLUGIN_BRANCH set means "just install from that ref, no menu". That is how
rem the old two-file launcher drove the old engine, and how the forwarders and
rem power users still drive this one.
if defined PLUGIN_BRANCH ( set "CHOSEN=%REF_1%" & goto :install )

:menu
echo.
echo ===============================================================
echo  OrcaSlicer Plugins  --  install or update
echo ===============================================================
if /i "%REMEMBERED%"=="main" (echo  Version:  main - the released build) else echo  Version:  %REMEMBERED% - a TEST BUILD
if defined REMEMBERED_DIR goto :menu_folder
echo  Folder:   not chosen yet - you will be asked
goto :menu_body
:menu_folder
echo  Folder:   %REMEMBERED_DIR%
:menu_body
echo ===============================================================
echo.
echo   [1] Install or update the plugins now
echo   [2] Choose the build - main or one of the five newest test branches
echo   [3] Choose which OrcaSlicer folder to install into
echo   [4] Forget my remembered choices
echo   [Q] Quit
echo.
echo   Just press Enter to do [1].
echo.
set "PICK="
set /p PICK=Choice, or Enter to install:
if not defined PICK goto :install_remembered
if "%PICK%"=="1" goto :install_remembered
if "%PICK%"=="2" goto :pick_version
if "%PICK%"=="3" goto :pick_folder
if "%PICK%"=="4" goto :forget
if /i "%PICK%"=="Q" goto :cancel
echo That is not a menu choice.
goto :menu

:forget
del "%BRANCH_STATE%" 2>nul
del "%DATADIR_STATE%" 2>nul
set "REMEMBERED=main"
set "REMEMBERED_DIR="
echo.
echo Forgotten. Back to the released build, and you will be asked for the
echo OrcaSlicer folder on the next install.
goto :menu

:install_remembered
set "CHOSEN=%REMEMBERED%"
goto :install

rem ---------------------------------------------------------------------------
rem  Folder picker entry (menu item 3). The same numbered chooser the install
rem  step uses, but it always asks, so you can point a different OrcaSlicer
rem  version at the plugins without changing the build.
rem ---------------------------------------------------------------------------
:pick_folder
call :select_data_dir "" ask
if not errorlevel 1 set "REMEMBERED_DIR=%TARGET_DATA_DIR%"
goto :menu

rem ---------------------------------------------------------------------------
rem  Build picker. Lists live branches from GitHub so a typo cannot silently
rem  install nothing. Never falls back to another branch's files.
rem ---------------------------------------------------------------------------
:pick_version
del "%BRANCHES%" 2>nul
echo.
echo Fetching the live branch list from GitHub...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { $h=@{'User-Agent'='OrcaPlugins'}; $bs=Invoke-RestMethod -Headers $h -Uri 'https://api.github.com/repos/%REPO%/branches?per_page=100'; $now=[DateTimeOffset]::UtcNow; $rows=@(); foreach($b in $bs){ if($b.name -notmatch '^[A-Za-z0-9][A-Za-z0-9._/-]*$' -or $b.name.Contains('..')){continue}; $c=Invoke-RestMethod -Headers $h -Uri ([string]$b.commit.url); $d=[DateTimeOffset]::Parse($c.commit.committer.date); $age=$now-$d; if($age.TotalMinutes -lt 2){$a='updated just now'}elseif($age.TotalHours -lt 1){$a=('updated {0} minutes ago' -f [math]::Floor($age.TotalMinutes))}elseif($age.TotalDays -lt 1){$a=('updated {0} hours ago' -f [math]::Floor($age.TotalHours))}else{$a=('updated {0} days ago' -f [math]::Floor($age.TotalDays))}; $rows += [pscustomobject]@{Name=$b.name; Date=$d; Age=$a} }; $ordered=@($rows|Where-Object Name -eq 'main')+@($rows|Where-Object Name -ne 'main'|Sort-Object Date -Descending); [IO.File]::WriteAllLines('%BRANCHES%', @($ordered|ForEach-Object{$_.Name+'|'+$_.Age}), [Text.Encoding]::ASCII) } catch { Write-Host ('GitHub branch list failed: '+$_.Exception.Message); exit 1 }"
if errorlevel 1 goto :offline
if not exist "%BRANCHES%" goto :offline

:version_menu
echo.
echo ---------------------------------------------------------------
echo  Which build do you want?
echo  Released main first, then the five newest test branches.
echo ---------------------------------------------------------------
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCHES%") do if !COUNT! LSS 6 (
  set /a COUNT+=1
  set "MENU_!COUNT!=%%A"
  if /i "%%A"=="main" (echo   [!COUNT!] main - RELEASED - %%B) else echo   [!COUNT!] %%A - TEST BUILD - %%B
)
echo.
echo   [A] Show all branches
echo   [T] Type a branch name myself
echo   [R] Go back to the released build
echo   [M] Back to the main menu
echo.
set "PICK="
set /p PICK=Choice:
if not defined PICK goto :version_menu
if /i "%PICK%"=="A" goto :all_branches
if /i "%PICK%"=="T" goto :type_branch
if /i "%PICK%"=="R" (set "CHOSEN=main"&goto :install)
if /i "%PICK%"=="M" (del "%BRANCHES%" 2>nul&goto :menu)
set "CHOSEN="
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (echo That is not a menu choice.&goto :version_menu)
goto :install

:all_branches
echo.
echo All live branches:
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCHES%") do (
  set /a COUNT+=1
  set "MENU_!COUNT!=%%A"
  if /i "%%A"=="main" (echo   [!COUNT!] main - RELEASED - %%B) else echo   [!COUNT!] %%A - TEST BUILD - %%B
)
echo.
set "PICK="
set /p PICK=Enter a number, or M for the short list:
if /i "%PICK%"=="M" goto :version_menu
set "CHOSEN="
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (echo That is not a menu choice.&goto :all_branches)
goto :install

:offline
echo.
echo GitHub's live branch list could not be loaded.
echo Nothing will silently switch to another build.
echo   [T] Type a branch name you already know
echo   [R] Retry the live list
echo   [M] Back to the main menu
set "PICK="
set /p PICK=Enter T, R, or M:
if /i "%PICK%"=="T" goto :type_branch
if /i "%PICK%"=="R" goto :pick_version
if /i "%PICK%"=="M" goto :menu
echo That is not a menu choice.
goto :offline

:type_branch
set "CHOSEN="
set /p CHOSEN=Type the exact branch name:
if not defined CHOSEN goto :version_menu
echo(!CHOSEN!| findstr /r /x "[A-Za-z0-9][A-Za-z0-9._/-]*" >nul
if errorlevel 1 (echo Invalid branch name. Use letters, numbers, dot, dash, underscore, or slash.&goto :type_branch)
echo(!CHOSEN!| findstr /c:".." >nul
if not errorlevel 1 (echo Invalid branch name: two dots are not allowed.&goto :type_branch)
goto :install

rem ---------------------------------------------------------------------------
rem  Install. This is the engine that used to live in Update-Orca-Plugins.bat,
rem  running inside the same file as the menu. The chosen build is remembered
rem  only after its catalogue downloads, so a dead branch name is never
rem  written to disk, and the OrcaSlicer folder is chosen only after a test
rem  branch has passed its all-or-nothing preflight.
rem ---------------------------------------------------------------------------
:install
if not defined CHOSEN set "CHOSEN=%REMEMBERED%"
set "REF_1=%CHOSEN%"
set "BRANCH_MODE="
if /i not "%REF_1%"=="main" set "BRANCH_MODE=1"

echo.
echo ===========================================================================
echo  OrcaSlicer plugins  --  install / update
echo  Wave Overhangs + Unlayered Infill
echo.
echo  ***************************************************************
echo  * BRANCH: %REF_1%
echo  ***************************************************************
echo  source:    https://github.com/%REPO%   ref: %REF_1%
echo  catalogue: %MANIFEST_PATH%
if defined LOCAL_MODE echo  mode:      --local, plugin files from next to this .bat
echo ===========================================================================
echo.
if /i "%CHOSEN%"=="main" (echo  This is the RELEASED build.) else echo  This is a TEST BUILD. It will not borrow files from main.
echo.

set "MANIFEST_TMP=%TEMP%\orca_manifest_%RANDOM%.json"
set "PLAN_FILE=%TEMP%\orca_plan_%RANDOM%.txt"
set "PLAN_SRC="
set "PLAN_MADE="

call :fetch_manifest
if not defined PLAN_SRC if defined BRANCH_MODE goto :branch_manifest_failed
if not defined PLAN_SRC echo  Catalogue unavailable; using the fallback list built into this file.
if defined PLAN_SRC call :build_plan
if not defined PLAN_MADE if defined BRANCH_MODE goto :branch_manifest_failed
if not defined PLAN_MADE call :fallback_plan
if not exist "%PLAN_FILE%" goto :no_plan

rem The build proved itself real -- its catalogue downloaded and parsed -- so
rem it is safe to remember it for the next run. A dead branch name can never
rem be persisted, because the lines above stop first.
if not exist "%STATE_DIR%" mkdir "%STATE_DIR%" 2>nul
>"%BRANCH_STATE%" echo %CHOSEN%

rem A test branch is all-or-nothing. Download and validate every plugin before
rem changing Orca's folders, so a missing second plugin cannot leave a mixed
rem or half-updated installation.
if not defined BRANCH_MODE goto :preflight_done
call :preflight_branch
if errorlevel 1 goto :fail
:preflight_done

echo.
echo  ***************************************************************
echo  * BUILD TO INSTALL -- branch: %REF_1%
echo  ***************************************************************
if defined BRANCH_MODE type "%PREFLIGHT_SUMMARY%"
if not defined BRANCH_MODE for /f "usebackq tokens=2,3 delims=|" %%A in ("%PLAN_FILE%") do echo    %%A v%%B
echo  ***************************************************************

rem Only choose or create Orca folders after a test branch has passed its
rem all-files preflight. A missing branch file therefore changes nothing.
call :select_data_dir "%DATA_DIR_ARG%"
if errorlevel 1 goto :fail
set "PLUGIN_ROOT=%TARGET_DATA_DIR%\orca_plugins"
if not exist "%PLUGIN_ROOT%" mkdir "%PLUGIN_ROOT%"
if errorlevel 1 goto :mkdir_failed
echo  Installing into: "%PLUGIN_ROOT%"
echo.

set /a PLAN_COUNT=0
set /a OK_COUNT=0
set /a FAIL_COUNT=0
set "INSTALLED_SUMMARY=%TEMP%\orca_installed_%RANDOM%.txt"
> "%INSTALLED_SUMMARY%" echo    Installed versions:

rem id|name|version|orca_dir|file|repo_path|cap1|cap2 -- plus a sacrificial
rem 9th field that absorbs the CR `echo` appends to the fallback lines.
for /f "usebackq tokens=1-9 delims=|" %%A in ("%PLAN_FILE%") do (
    set /a PLAN_COUNT+=1
    call :install_one "%%A" "%%B" "%%C" "%%D" "%%E" "%%F" "%%G" "%%H"
    if not errorlevel 1 ( set /a OK_COUNT+=1 ) else set /a FAIL_COUNT+=1
)

if "%PLAN_COUNT%"=="0" goto :no_plan
echo.
echo  Plan: %PLAN_COUNT% plugin(s).  OK: %OK_COUNT%   failed: %FAIL_COUNT%
if "%OK_COUNT%"=="0" (
    if "%FAIL_COUNT%"=="0" echo  Nothing matched PLUGIN_ONLY=%PLUGIN_ONLY%.
    goto :fail
)

call :stage_tools

echo.
echo  Next steps in OrcaSlicer:
echo    1. Restart OrcaSlicer, or reopen File ^> Plugins.
echo    2. Confirm each plugin is enabled and shows two capabilities
echo       -- the worker and "... - Check setup".
echo    3. To use one, pick it under Others ^> Slicing Pipeline Plugin.
echo    4. Unlayered Infill needs "Use relative E distances" enabled.
echo.
echo  IMPORTANT -- what you will and will not see in the preview:
echo    * Wave Overhangs edits exported Bridge G-code after slicing. Its
echo      changes do NOT appear in the normal preview; export and reopen the file.
echo    * Post-processing runs on Export G-code file, not Print or Send.
if exist "%DLDIR%" echo    * Copies for Orca's UI installer are in "%DLDIR%"
if exist "%TOOLDIR%\unlayered_infill_post.py" echo    * No plugin needed: double-click
if exist "%TOOLDIR%\unlayered_infill_post.py" echo      "%TOOLDIR%\unlayered_infill_post.py" and point it at an exported .gcode
echo.
echo  ***************************************************************
echo  * INSTALLED FROM BRANCH: %REF_1%
echo  * Versions installed:
if exist "%INSTALLED_SUMMARY%" type "%INSTALLED_SUMMARY%"
echo  ***************************************************************
echo.
echo  Remembered for next time:
echo    build:  %CHOSEN%
echo    folder: %TARGET_DATA_DIR%
echo.
echo  Install/update complete.
goto :done

rem ---------------------------------------------------------------------------
rem  install_one  %1=id %2=name %3=version %4=orca_dir %5=file %6=repo_path
rem                %7=cap1 %8=cap2
rem ---------------------------------------------------------------------------
:install_one
set "PL_ID=%~1"
set "PL_NAME=%~2"
set "PL_VER=%~3"
set "PL_DIR=%~4"
set "PL_FILE=%~5"
set "PL_PATH=%~6"
set "PL_CAP1=%~7"
set "PL_CAP2=%~8"
set "PL_SRC="
set "PL_KEEP=1"

if not defined PLUGIN_ONLY goto :filter_done
set "PL_TEST=%PLUGIN_ONLY%"
set "PL_TEST=%PL_TEST:%PL_ID%=%"
if "%PL_TEST%"=="%PLUGIN_ONLY%" set "PL_KEEP=0"
:filter_done
if "%PL_KEEP%"=="0" exit /b 0

echo.
echo --- %PL_NAME% (%PL_ID%) ---

if not defined LOCAL_MODE goto :get_remote
if exist "%HERE%\%PL_PATH%" ( set "PL_SRC=%HERE%\%PL_PATH%" & goto :have_src )
if exist "%HERE%\%PL_FILE%" ( set "PL_SRC=%HERE%\%PL_FILE%" & goto :have_src )
echo   [SKIP] --local: no "%PL_PATH%" next to this .bat.
exit /b 1

:get_remote
if defined BRANCH_MODE if exist "%PREFLIGHT_DIR%\%PL_FILE%" ( set "PL_SRC=%PREFLIGHT_DIR%\%PL_FILE%" & goto :have_src )
call :try_download
if defined PL_SRC goto :have_src
echo   [FAIL] could not download %PL_NAME%.
echo          URL: https://raw.githubusercontent.com/%REPO%/%REF_1%/%PL_PATH%
exit /b 1

:have_src
rem What landed must look like a PEP 723 plugin, not a saved error page.
findstr /c:"# /// script" "%PL_SRC%" >nul 2>nul
if errorlevel 1 (
    echo   [FAIL] "%PL_SRC%" has no PEP 723 header -- not an Orca plugin.
    exit /b 1
)

rem The version stamped in the file's header wins over the catalogue's.
set "HDR_VER="
for /f "usebackq tokens=3 delims== " %%V in (`findstr /b /c:"# version = " "%PL_SRC%"`) do set "HDR_VER=%%~V"
if defined HDR_VER set "PL_VER=%HDR_VER%"

set "PL_DEST_DIR=%PLUGIN_ROOT%\%PL_DIR%"
set "PL_DEST_FILE=%PL_DEST_DIR%\%PL_FILE%"
set "STATE_FILE=%PL_DEST_DIR%\.install_state.json"

set "WAS_THERE=0"
if exist "%PL_DEST_FILE%" set "WAS_THERE=1"
set "OLD_VER="
if exist "%STATE_FILE%" for /f "usebackq tokens=2 delims=:, " %%V in (`findstr /c:"installed_version" "%STATE_FILE%"`) do set "OLD_VER=%%~V"

if not exist "%PL_DEST_DIR%" mkdir "%PL_DEST_DIR%"
if errorlevel 1 (
    echo   [FAIL] could not create "%PL_DEST_DIR%".
    exit /b 1
)

copy /Y "%PL_SRC%" "%PL_DEST_FILE%" >nul
if errorlevel 1 (
    echo   [FAIL] could not copy into "%PL_DEST_DIR%".
    exit /b 1
)

rem Orca's Plugins dialog writes this sidecar when installing locally; writing
rem it here keeps the copy discoverable and already enabled.
rem Package names are permanent and version-free. Orca stores them in saved
rem preset/config identities; real versions live in installed_version.
call :write_state "%STATE_FILE%" "%PL_NAME%" "%PL_CAP1%" "%PL_CAP2%" "%PL_VER%"
if errorlevel 1 (
    echo   [FAIL] could not write "%STATE_FILE%".
    exit /b 1
)

rem Refresh any other copies of this file under the plugin root -- e.g. an
rem install parked under a different folder name -- but never a _subscribed
rem cloud copy.
set /a SIB_COUNT=0
for /f "delims=" %%F in ('dir /s /b "%PLUGIN_ROOT%\%PL_FILE%" 2^>nul') do (
    set "SIB=%%F"
    if /i not "%%F"=="%PL_DEST_FILE%" if /i "!SIB:_subscribed=!"=="!SIB!" (
        copy /Y "%PL_SRC%" "%%F" >nul
        if not errorlevel 1 set /a SIB_COUNT+=1
    )
)

rem Keep a copy in Downloads\OrcaPlugins for Orca's UI installer.
set "STAGED="
if exist "%USERPROFILE%\Downloads" (
    if not exist "%DLDIR%" mkdir "%DLDIR%" 2>nul
    if exist "%DLDIR%" copy /Y "%PL_SRC%" "%DLDIR%\%PL_FILE%" >nul
    if exist "%DLDIR%\%PL_FILE%" set "STAGED=%DLDIR%\%PL_FILE%"
)

set "PL_ACTION=INSTALLED"
if "%WAS_THERE%"=="1" set "PL_ACTION=UPDATED"
set "PL_VERMSG="
if defined OLD_VER if not "%OLD_VER%"=="%PL_VER%" set "PL_VERMSG=(was v%OLD_VER%) "
for %%A in ("%PL_DEST_FILE%") do set "PL_SIZE=%%~zA"
echo   [%PL_ACTION%] %PL_NAME% v%PL_VER% %PL_VERMSG%-- %PL_SIZE% bytes
>> "%INSTALLED_SUMMARY%" echo    %PL_NAME% v%PL_VER%
echo              "%PL_DEST_FILE%"
if not "%SIB_COUNT%"=="0" echo              refreshed %SIB_COUNT% other copy/copies under the plugin root
if defined STAGED echo              copy for Orca's UI installer: "%STAGED%"
exit /b 0

rem ---------------------------------------------------------------------------
rem  try_download  --  fill PL_SRC from the selected ref only. Never fall
rem  back to main: mixing branch and released files makes a test meaningless.
rem ---------------------------------------------------------------------------
:try_download
set "PL_SRC="
if "%REF_1%"=="" goto :try_done
set "PL_TMP=%TEMP%\orca_%RANDOM%_%PL_FILE%"
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%PL_PATH%" "%PL_TMP%"
if not errorlevel 1 set "PL_SRC=%PL_TMP%"
:try_done
exit /b 0

rem ---------------------------------------------------------------------------
rem  fetch_manifest  --  fill PLAN_SRC from the selected ref only. In test
rem  branch mode the caller stops if it is absent; it never borrows main.
rem ---------------------------------------------------------------------------
:fetch_manifest
set "PLAN_SRC="
if not defined LOCAL_MODE goto :fm_remote
if exist "%HERE%\%MANIFEST_PATH%" ( set "PLAN_SRC=%HERE%\%MANIFEST_PATH%" & goto :fm_done )
:fm_remote
if "%REF_1%"=="" goto :fm_done
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%MANIFEST_PATH%" "%MANIFEST_TMP%" 100
if not errorlevel 1 set "PLAN_SRC=%MANIFEST_TMP%"
:fm_done
exit /b 0

rem ---------------------------------------------------------------------------
rem  preflight_branch -- fetch every selected test-branch plugin before install.
rem ---------------------------------------------------------------------------
:preflight_branch
set "PREFLIGHT_DIR=%TEMP%\orca_preflight_%RANDOM%"
set "PREFLIGHT_SUMMARY=%PREFLIGHT_DIR%\versions.txt"
mkdir "%PREFLIGHT_DIR%" 2>nul
if errorlevel 1 exit /b 1
> "%PREFLIGHT_SUMMARY%" echo    Selected test build:
set "PREFLIGHT_FAILED="
for /f "usebackq tokens=1-9 delims=|" %%A in ("%PLAN_FILE%") do call :preflight_one "%%A" "%%B" "%%E" "%%F"
if defined PREFLIGHT_FAILED exit /b 1
exit /b 0

:preflight_one
set "PF_ID=%~1"
set "PF_NAME=%~2"
set "PF_FILE=%~3"
set "PF_PATH=%~4"
if defined PLUGIN_ONLY (
    set "PF_TEST=%PLUGIN_ONLY%"
    set "PF_TEST=!PF_TEST:%PF_ID%=!"
    if "!PF_TEST!"=="%PLUGIN_ONLY%" exit /b 0
)
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%PF_PATH%" "%PREFLIGHT_DIR%\%PF_FILE%"
if errorlevel 1 goto :pf_fail
findstr /c:"# /// script" "%PREFLIGHT_DIR%\%PF_FILE%" >nul 2>nul
if errorlevel 1 goto :pf_fail
set "PF_VER="
for /f "usebackq tokens=3 delims== " %%V in (`findstr /b /c:"# version = " "%PREFLIGHT_DIR%\%PF_FILE%"`) do set "PF_VER=%%~V"
if not defined PF_VER goto :pf_fail
>> "%PREFLIGHT_SUMMARY%" echo    %PF_NAME% v%PF_VER%
exit /b 0
:pf_fail
set "PREFLIGHT_FAILED=1"
echo.
echo  ***************************************************************
echo  * TEST BUILD NOT INSTALLED
echo  * Branch: %REF_1%
echo  * Missing or invalid: %PF_PATH%
echo  * Nothing has been changed. This updater will NOT use main.
echo  ***************************************************************
exit /b 1

rem ---------------------------------------------------------------------------
rem  build_plan  --  inline PowerShell turns the catalogue into the plan file.
rem  One line, no caret continuations. The plan is written with LF line endings
rem  so `for /f` never sees a stray CR at the end of a field.
rem ---------------------------------------------------------------------------
:build_plan
echo   Reading catalogue: "%PLAN_SRC%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { $m = Get-Content -Raw -LiteralPath '%PLAN_SRC%' | ConvertFrom-Json; $l = @(); foreach ($p in $m.plugins) { if ($p.status -ne 'ready') { continue }; $c = @($p.capabilities); if ($c.Count -lt 2) { exit 3 }; $l += (@($p.id, $p.name, $p.version, $p.orca_dir, $p.file, $p.path, [string]$c[0], [string]$c[1]) -join '|') }; if ($l.Count -eq 0) { exit 2 }; $nl = [string][char]10; [IO.File]::WriteAllText('%PLAN_FILE%', ($l -join $nl) + $nl) } catch { Write-Host ('  ' + $_.Exception.Message); exit 1 }"
if not errorlevel 1 if exist "%PLAN_FILE%" set "PLAN_MADE=1"
exit /b 0

rem ---------------------------------------------------------------------------
rem  fallback_plan  --  the catalogue, hardcoded. Only used when plugins.json
rem  cannot be fetched or parsed; test_installer.py fails if this rots.
rem  The trailing ^|end field is sacrificial: `for /f` keeps the CR that echo
rem  writes, and this way it lands on a field nothing reads.
rem ---------------------------------------------------------------------------
:fallback_plan
echo   Using the fallback plan built into this file.
>  "%PLAN_FILE%" echo wave-overhangs^|Wave Overhangs^|0.0.46^|WaveOverhangs^|wave_overhangs_orca.py^|plugins/wave-overhangs/wave_overhangs_orca.py^|Wave Overhangs^|Wave Overhangs - Settings guide & check^|end
>> "%PLAN_FILE%" echo unlayered-infill^|Unlayered Infill^|0.4.8^|UnlayeredInfill^|unlayered_infill_orca.py^|plugins/unlayered-infill/unlayered_infill_orca.py^|Unlayered Infill^|Unlayered Infill - Check setup^|end
exit /b 0

rem ---------------------------------------------------------------------------
rem  download %1=url %2=dest %3=minimum bytes (default 2000 for code)
rem  The catalogue passes 100 because valid JSON is much smaller than a plugin.
rem ---------------------------------------------------------------------------
:download
set "DL_MIN=2000"
if not "%~3"=="" set "DL_MIN=%~3"
del "%~2" 2>nul
where curl.exe >nul 2>nul
if not errorlevel 1 (
    echo   [curl] %~1
    curl.exe -fLsS --retry 2 -o "%~2" "%~1"
    if not errorlevel 1 if exist "%~2" goto :download_check
)
echo   [powershell] %~1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri '%~1' -OutFile '%~2' } catch { Write-Host ('  ' + $_.Exception.Message); exit 1 }"
if not errorlevel 1 if exist "%~2" goto :download_check
echo   [bitsadmin] %~1
bitsadmin /transfer OrcaPluginDL /priority foreground "%~1" "%~2" >nul 2>nul
if exist "%~2" goto :download_check
echo   All download methods failed.
exit /b 1

:download_check
rem Reject empty / truncated files (e.g. an error page saved as the plugin).
for %%A in ("%~2") do if %%~zA GEQ %DL_MIN% exit /b 0
echo   Downloaded file looks empty or truncated.
del "%~2" 2>nul
exit /b 1

rem ---------------------------------------------------------------------------
rem  write_state %1=file %2=pluginname %3=cap1 %4=cap2 %5=version
rem ---------------------------------------------------------------------------
:write_state
>  "%~1" echo {
>> "%~1" echo   "capabilities": [
>> "%~1" echo     { "%~3": true },
>> "%~1" echo     { "%~4": true }
>> "%~1" echo   ],
>> "%~1" echo   "enabled": true,
>> "%~1" echo   "installed_from": "local",
>> "%~1" echo   "installed_version": "%~5",
>> "%~1" echo   "plugin_name": "%~2"
>> "%~1" echo }
exit /b 0

rem ---------------------------------------------------------------------------
rem  Choose the OrcaSlicer data directory (prefers a nightly folder)
rem
rem  %1  an explicit directory from the command line, or empty
rem  %2  "ask" to always show the numbered picker -- that is what menu item 3
rem      uses. Without it, a remembered folder that still exists is used
rem      without asking, which is the whole point of remembering it.
rem ---------------------------------------------------------------------------
:select_data_dir
set "SD_ASK=%~2"
set "TARGET_DATA_DIR="
if /i "%SD_ASK%"=="ask" goto :sd_scan
if not "%~1"=="" (
    set "TARGET_DATA_DIR=%~1"
    goto :ensure_data_dir
)
if defined ORCA_DATA_DIR (
    set "TARGET_DATA_DIR=%ORCA_DATA_DIR%"
    goto :ensure_data_dir
)
if defined REMEMBERED_DIR if exist "%REMEMBERED_DIR%" (
    echo  Using the remembered OrcaSlicer folder:
    echo    "%REMEMBERED_DIR%"
    set "TARGET_DATA_DIR=%REMEMBERED_DIR%"
    goto :ensure_data_dir
)

:sd_scan
if not defined APPDATA (
    echo ERROR: APPDATA is not set. Pass Orca's data directory as the first argument.
    exit /b 1
)

echo.
echo === Locating your OrcaSlicer data directory ===
rem The last directory installed into is remembered here so repeat runs do
rem not have to retype it. Delete the file to be asked from scratch.
set "REMEMBERED_DIR="
if exist "%DATADIR_STATE%" set /p REMEMBERED_DIR=<"%DATADIR_STATE%"
if defined REMEMBERED_DIR if not exist "%REMEMBERED_DIR%" set "REMEMBERED_DIR="
set /a NCOUNT=0
set /a OCOUNT=0
for /d %%D in ("%APPDATA%\OrcaSlicer*") do (
    set "NAME=%%~nxD"
    set "IS_NIGHTLY="
    echo(!NAME! | findstr /i "night dev alpha beta" >nul && set "IS_NIGHTLY=1"
    if defined IS_NIGHTLY (
        set /a NCOUNT+=1
        set "NCAND_!NCOUNT!=%%~fD"
    ) else (
        set /a OCOUNT+=1
        set "OCAND_!OCOUNT!=%%~fD"
    )
)
if /i "%SD_ASK%"=="ask" goto :dir_menu
if "%NCOUNT%"=="1" (
    set "TARGET_DATA_DIR=!NCAND_1!"
    echo Found nightly data dir: "!TARGET_DATA_DIR!"
    goto :ensure_data_dir
)
if "%NCOUNT%"=="0" if "%OCOUNT%"=="1" (
    set "TARGET_DATA_DIR=!OCAND_1!"
    echo Found data dir: "!TARGET_DATA_DIR!"
    goto :ensure_data_dir
)
if "%NCOUNT%"=="0" if "%OCOUNT%"=="0" (
    set "TARGET_DATA_DIR=%APPDATA%\OrcaSlicer"
    echo No existing OrcaSlicer data dir found; defaulting to:
    echo   "!TARGET_DATA_DIR!"
    goto :ensure_data_dir
)

:dir_menu
echo.
echo Select the OrcaSlicer data directory to install into:
set /a IDX=0
if not "%NCOUNT%"=="0" echo   -- nightly --
for /L %%I in (1,1,%NCOUNT%) do (
    set /a IDX+=1
    set "MENU_!IDX!=!NCAND_%%I!"
    echo   [!IDX!] !NCAND_%%I!
)
if not "%OCOUNT%"=="0" echo   -- other Orca data dirs --
for /L %%I in (1,1,%OCOUNT%) do (
    set /a IDX+=1
    set "MENU_!IDX!=!OCAND_%%I!"
    echo   [!IDX!] !OCAND_%%I!
)
if "%NCOUNT%"=="0" if "%OCOUNT%"=="0" echo   (none found under "%APPDATA%" - type a full path below)
echo.
if defined REMEMBERED_DIR echo   [Enter] Keep using: %REMEMBERED_DIR%
if defined REMEMBERED_DIR echo.
set "DPICK="
set /p DPICK=Enter a number, a full path, or just Enter to keep:
if not defined DPICK if defined REMEMBERED_DIR (set "TARGET_DATA_DIR=%REMEMBERED_DIR%"&goto :ensure_data_dir)
if not defined DPICK exit /b 1
set "TARGET_DATA_DIR="
for /L %%I in (1,1,%IDX%) do (
    if "%DPICK%"=="%%I" set "TARGET_DATA_DIR=!MENU_%%I!"
)
if not defined TARGET_DATA_DIR set "TARGET_DATA_DIR=%DPICK%"

:ensure_data_dir
set "TARGET_DATA_DIR=%TARGET_DATA_DIR:"=%"
if not exist "%TARGET_DATA_DIR%" (
    echo.
    echo Orca data directory does not exist yet:
    echo "%TARGET_DATA_DIR%"
    set /p MAKE_DIR=Create it? [Y/N]
    if /I not "!MAKE_DIR!"=="Y" exit /b 1
    mkdir "%TARGET_DATA_DIR%"
    if errorlevel 1 exit /b 1
)
rem Remember this directory so the next run uses it without asking.
if defined LOCALAPPDATA (
    if not exist "%STATE_DIR%" mkdir "%STATE_DIR%" 2>nul
    >"%DATADIR_STATE%" echo %TARGET_DATA_DIR%
)
exit /b 0

rem ---------------------------------------------------------------------------
rem  self_update -- run the newest updater without ever rewriting this file
rem  while it is running.
rem
rem  cmd.exe streams a .bat from disk by byte offset as it executes, so a file
rem  that overwrites itself mid-run can jump into garbage. So this does NOT
rem  replace the file on disk. It fetches the latest copy to a temp file and,
rem  if that copy is a different version, hands this run over to it. The file
rem  you double-click never changes -- a bad download cannot leave you without
rem  a working updater -- and you still get the newest behaviour every run.
rem
rem  Plugin updates never need any of this: the plugin files and the catalogue
rem  are downloaded fresh on every run already.
rem
rem  It updates from the build you last chose, not always from main, so that
rem  testing a branch tests that branch's updater too. Both child guards are
rem  needed: an old launcher hands over with ORCA_FRONTDOOR_CHILD set, and an
rem  old updater with ORCA_UPDATER_CHILD set.
rem ---------------------------------------------------------------------------
:self_update
if defined ORCA_FRONTDOOR_CHILD exit /b 0
if defined ORCA_UPDATER_CHILD exit /b 0
if defined NO_SELF_UPDATE exit /b 0
if defined LOCAL_MODE exit /b 0
if defined BRANCH_MODE exit /b 0
echo Checking for a newer version of this updater...
set "SELF_REF=%REMEMBERED%"
if defined PLUGIN_BRANCH set "SELF_REF=%REF_1%"
set "NEWBAT=%TEMP%\orca_updater_%RANDOM%.bat"
call :download "https://raw.githubusercontent.com/%REPO%/%SELF_REF%/Orca-Plugins.bat" "%NEWBAT%"
if errorlevel 1 goto :su_skip
rem Never execute an unverified download: a 404 page or a wifi login portal
rem must fail this check rather than run.
findstr /b /c:"rem FRONTDOOR_VERSION " "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
findstr /c:"FRONTDOOR_VERSION=" "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
rem Same version as ours? Nothing to do.
findstr /b /c:"rem FRONTDOOR_VERSION %FRONTDOOR_VERSION% end" "%NEWBAT%" >nul 2>nul
if not errorlevel 1 goto :su_skip
rem Never hand this run back to the old two-file layout. While this build
rem lives on a test branch, main's Orca-Plugins.bat is still the old
rem launcher, which carries no UPDATER_VERSION marker -- so a "different"
rem version there is an OLDER file, not a newer one. Only a download that is
rem itself the one-file updater may take over the run.
findstr /b /c:"rem UPDATER_VERSION " "%NEWBAT%" >nul 2>nul
if errorlevel 1 (
    echo  The %SELF_REF% build still has the old two-file updater, so this
    echo  unified copy stays in charge for this run.
    goto :su_skip
)
set "NEW_FV=?"
rem "rem FRONTDOOR_VERSION 2.0.0 end" -- token 3 is the version, and the
rem trailing "end" absorbs the CR so it never lands in the variable.
for /f "usebackq tokens=3" %%V in (`findstr /b /c:"rem FRONTDOOR_VERSION " "%NEWBAT%"`) do set "NEW_FV=%%V"
echo  This copy is v%FRONTDOOR_VERSION%, v%NEW_FV% is available.
echo  Running the newer one for this update. Your .bat file is left as it is
echo  -- it will keep fetching the newest version every time you run it.
echo.
set "ORCA_FRONTDOOR_CHILD=1"
set "CHILD_RC=1"
call "%NEWBAT%" %ORIG_ARGS%
set "CHILD_RC=%ERRORLEVEL%"
set "SELF_UPDATED=1"
del "%NEWBAT%" 2>nul
exit /b 0
:su_skip
if exist "%NEWBAT%" del "%NEWBAT%" 2>nul
exit /b 0

rem ---------------------------------------------------------------------------
rem  stage_tools -- the standalone scripts. These are NOT plugins: they run on
rem  an already-exported .gcode file, so they work even if Orca's plugin system
rem  is not cooperating.
rem
rem  They go in a SEPARATE tools\ subfolder, never beside the plugin copies.
rem  Orca requires a plugin folder to hold exactly ONE entry .py file, and
rem  %DLDIR% is the folder users are told to point Orca's installer at. A
rem  stray second .py in there makes the entry point ambiguous.
rem  A failure here is not fatal; the plugins are already installed.
rem ---------------------------------------------------------------------------
:stage_tools
if defined LOCAL_MODE exit /b 0
if not exist "%TOOLDIR%" mkdir "%TOOLDIR%" 2>nul
call :stage_one_tool "plugins/unlayered-infill/unlayered_infill_post.py" "unlayered_infill_post.py"
exit /b 0

:stage_one_tool
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%~1" "%TOOLDIR%\%~2"
if errorlevel 1 (
    echo  - tool %~2: download failed, skipped
    exit /b 0
)
echo  - tool %~2: saved to "%TOOLDIR%"
exit /b 0

rem ---------------------------------------------------------------------------
:child_done
if not defined CHILD_RC set "CHILD_RC=0"
exit /b %CHILD_RC%

rem ---------------------------------------------------------------------------
:branch_manifest_failed
echo.
echo  ***************************************************************
echo  * TEST BUILD NOT INSTALLED
echo  * Branch: %REF_1%
echo  * plugins.json is missing or invalid on that branch.
echo  * Nothing has been changed. This updater will NOT use main.
echo  * Pick "Choose the build" on the menu to go back to released main.
echo  ***************************************************************
goto :fail

:no_plan
echo.
echo ERROR: no plan could be built -- the catalogue fetch and the fallback
echo list both produced nothing. This .bat is broken; re-download it.
goto :fail

:mkdir_failed
echo ERROR: Could not create the orca_plugins directory.
goto :fail

rem ---------------------------------------------------------------------------
:help
echo Installs or updates the OrcaSlicer plugins
echo   Wave Overhangs, Unlayered Infill
echo from https://github.com/%REPO% into your Orca data folder.
echo.
echo   Orca-Plugins.bat [data_dir] [--local] [--no-self-update] [--help]
echo.
echo     data_dir   OrcaSlicer data directory; found under %%APPDATA%% if omitted
echo     --local    install plugin files found next to this .bat, no downloads
echo     --no-self-update  do not hand over to a newer copy of this updater
echo     --help     this text
echo.
echo   With no arguments it shows a menu: install now (Enter), choose the
echo   build -- released main or one of the five newest test branches -- or
echo   choose which OrcaSlicer folder to install into. The build and the
echo   folder are remembered between runs, and "Forget my remembered
echo   choices" resets both.
echo.
echo   Environment: ORCA_DATA_DIR, PLUGIN_BRANCH (default main), PLUGIN_ONLY,
echo   ORCA_NO_SELF_UPDATE. Setting PLUGIN_BRANCH skips the menu and installs
echo   from that git ref directly -- that is how the old two-file updater was
echo   driven.
echo.
echo   You do not need to re-download this file to get new plugin versions:
echo   it fetches the catalogue and every plugin fresh on each run. It also
echo   checks whether the updater itself has been updated, and if so runs the
echo   newer copy from your temp folder for that run.
echo.
echo   Restart OrcaSlicer afterwards; each plugin appears in File ^> Plugins
echo   with two capabilities, and is selected per process preset under
echo   Others ^> Slicing Pipeline Plugin.
echo.
echo   First run may show an "Unknown Publisher" prompt; that is Windows
echo   flagging any downloaded .bat. Right-click this file, Properties,
echo   tick Unblock, and it never appears again.
echo.
echo   Your remembered choices live in:
echo     %STATE_DIR%
exit /b 0

rem ---------------------------------------------------------------------------
:fail
echo.
echo Install/update FAILED.
if exist "%PLAN_FILE%" del "%PLAN_FILE%" 2>nul
if exist "%MANIFEST_TMP%" del "%MANIFEST_TMP%" 2>nul
if defined PREFLIGHT_DIR if exist "%PREFLIGHT_DIR%" rmdir /s /q "%PREFLIGHT_DIR%" 2>nul
if defined INSTALLED_SUMMARY if exist "%INSTALLED_SUMMARY%" del "%INSTALLED_SUMMARY%" 2>nul
del "%BRANCHES%" 2>nul
echo.
echo  If you are offline, put the plugin files next to Orca-Plugins.bat and
echo  run:   Orca-Plugins.bat --local
pause
exit /b 1

:done
if exist "%PLAN_FILE%" del "%PLAN_FILE%" 2>nul
if exist "%MANIFEST_TMP%" del "%MANIFEST_TMP%" 2>nul
if defined PREFLIGHT_DIR if exist "%PREFLIGHT_DIR%" rmdir /s /q "%PREFLIGHT_DIR%" 2>nul
if defined INSTALLED_SUMMARY if exist "%INSTALLED_SUMMARY%" del "%INSTALLED_SUMMARY%" 2>nul
del "%BRANCHES%" 2>nul
pause
exit /b 0

rem ---------------------------------------------------------------------------
:cancel
del "%BRANCHES%" 2>nul
if defined PLAN_FILE if exist "%PLAN_FILE%" del "%PLAN_FILE%" 2>nul
if defined MANIFEST_TMP if exist "%MANIFEST_TMP%" del "%MANIFEST_TMP%" 2>nul
exit /b 0
