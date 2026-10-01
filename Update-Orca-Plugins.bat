@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ===========================================================================
rem  OrcaSlicer plugin updater  --  Wave Overhangs + Unlayered Infill
rem ===========================================================================
rem
rem  Keep this ONE file anywhere (Downloads, a Tools folder, the Desktop) and
rem  double-click it whenever you want to install or update the plugins. It
rem  needs no Python, Node or Git: it downloads the ready-built plugin files
rem  from the public GitHub repository below and copies them into your
rem  OrcaSlicer data folder, then refreshes every other copy it finds there.
rem
rem  Usage:
rem    Update-Orca-Plugins.bat
rem    Update-Orca-Plugins.bat "C:\Users\you\AppData\Roaming\OrcaSlicer"
rem    Update-Orca-Plugins.bat --local      use plugin files next to this .bat
rem    Update-Orca-Plugins.bat --help
rem
rem  Environment overrides:
rem    ORCA_DATA_DIR    Orca data directory (same as the first argument)
rem    PLUGIN_BRANCH    git ref to download from (default: main)
rem    PLUGIN_ONLY      comma-separated plugin ids, e.g. wave-overhangs
rem
rem  How it decides what to install:
rem    1. fetch plugins.json -- the catalogue -- from REPO at ref REF_1
rem    2. inline PowerShell turns the catalogue into a pipe-delimited plan
rem    3. if the catalogue cannot be fetched or parsed, it falls back to the
rem       hardcoded list at :fallback_plan, which test_installer.py keeps in
rem       sync with plugins.json
rem
rem  The repo must stay public: downloads are unauthenticated
rem  raw.githubusercontent.com requests, and a private repo 404s every file.
rem
rem  This file must keep CRLF line endings. *.bat -text in .gitattributes
rem  keeps git from re-normalising them; do not let an editor (or Python's
rem  Path.write_text) convert them to LF.
rem
rem  Windows shows an "Unknown Publisher" prompt the first time a
rem  downloaded .bat runs. That is expected for every .bat on the
rem  internet, not a warning about this one; right-click the file,
rem  Properties, tick Unblock, and it never appears again.
rem ===========================================================================

title OrcaSlicer plugin updater  --  Wave Overhangs + Unlayered Infill

set "REPO=ajani190819-ops/Tests"
set "MANIFEST_PATH=plugins.json"
set "REF_1=main"
if defined PLUGIN_BRANCH set "REF_1=%PLUGIN_BRANCH%"
set "REF_2=main"
if "%REF_2%"=="%REF_1%" set "REF_2="

rem This updater's own version. The line below it is the machine-readable
rem copy :self_update compares against; test_installer.py keeps them equal.
set UPDATER_VERSION=1.1.0
rem UPDATER_VERSION 1.1.0 end

rem PLUGIN_ONLY is matched as a substring against each plugin id.
if defined PLUGIN_ONLY set "PLUGIN_ONLY=%PLUGIN_ONLY: =%"

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

rem A copy of each downloaded file lands here for Orca's UI installer.
set "DLDIR=%USERPROFILE%\Downloads\OrcaPlugins"

set "LOCAL_MODE="
set "DATA_DIR_ARG="
set "NO_SELF_UPDATE="
if defined ORCA_NO_SELF_UPDATE set "NO_SELF_UPDATE=1"

rem Kept whole because :parse_args shifts them away, and :self_update has to
rem hand the same arguments to the newer copy.
set "ORIG_ARGS=%*"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--help" goto :help
if /i "%~1"=="-h" goto :help
if /i "%~1"=="--local" ( set "LOCAL_MODE=1" ) else if /i "%~1"=="--no-self-update" ( set "NO_SELF_UPDATE=1" ) else if not defined DATA_DIR_ARG set "DATA_DIR_ARG=%~1"
shift
goto :parse_args
:args_done

echo ===========================================================================
echo  OrcaSlicer plugins  --  install / update
echo  Wave Overhangs + Unlayered Infill
echo.
echo  source:    https://github.com/%REPO%   ref: %REF_1%
echo  catalogue: %MANIFEST_PATH%
if defined LOCAL_MODE echo  mode:      --local, plugin files from next to this .bat
echo ===========================================================================
echo.

call :self_update
if defined SELF_UPDATED goto :child_done

call :select_data_dir "%DATA_DIR_ARG%"
if errorlevel 1 goto :fail

set "PLUGIN_ROOT=%TARGET_DATA_DIR%\orca_plugins"
if not exist "%PLUGIN_ROOT%" mkdir "%PLUGIN_ROOT%"
if errorlevel 1 goto :mkdir_failed

echo  Installing into: "%PLUGIN_ROOT%"
echo.

set "MANIFEST_TMP=%TEMP%\orca_manifest_%RANDOM%.json"
set "PLAN_FILE=%TEMP%\orca_plan_%RANDOM%.txt"
set "PLAN_SRC="
set "PLAN_MADE="

call :fetch_manifest
if not defined PLAN_SRC echo  Catalogue unavailable; using the fallback list built into this file.
if defined PLAN_SRC call :build_plan
if not defined PLAN_MADE call :fallback_plan
if not exist "%PLAN_FILE%" goto :no_plan

set /a PLAN_COUNT=0
set /a OK_COUNT=0
set /a FAIL_COUNT=0

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
echo  IMPORTANT -- two reasons it can look like nothing happened:
echo    * Plugins run on "Export G-code file" ONLY. They do NOT run when you
echo      press Print or Send.
echo    * The 3D preview NEVER shows the result. No slicer redraws its preview
echo      after post-processing. To see the change, drag the exported .gcode
echo      file back into OrcaSlicer and look at that.
if exist "%DLDIR%" echo    * Copies for Orca's UI installer are in "%DLDIR%"
if exist "%DLDIR%\unlayered_infill_post.py" echo    * No plugin needed: double-click
if exist "%DLDIR%\unlayered_infill_post.py" echo      "%DLDIR%\unlayered_infill_post.py" and point it at an exported .gcode
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
rem The name is composed as "<name> v<version>" to match the plugin's PEP 723
rem name header, which carries the version so the Plugins dialog shows it.
rem Composing it from PL_VER (read from the downloaded file's header) means it
rem cannot drift from what is actually installed.
call :write_state "%STATE_FILE%" "%PL_NAME% v%PL_VER%" "%PL_CAP1%" "%PL_CAP2%" "%PL_VER%"
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
echo              "%PL_DEST_FILE%"
if not "%SIB_COUNT%"=="0" echo              refreshed %SIB_COUNT% other copy/copies under the plugin root
if defined STAGED echo              copy for Orca's UI installer: "%STAGED%"
exit /b 0

rem ---------------------------------------------------------------------------
rem  try_download  --  fill PL_SRC from REF_1, then REF_2; leaves it empty on
rem  total failure
rem ---------------------------------------------------------------------------
:try_download
set "PL_SRC="
if "%REF_1%"=="" goto :try_ref2
set "PL_TMP=%TEMP%\orca_%RANDOM%_%PL_FILE%"
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%PL_PATH%" "%PL_TMP%"
if not errorlevel 1 ( set "PL_SRC=%PL_TMP%" & goto :try_done )
:try_ref2
if "%REF_2%"=="" goto :try_done
set "PL_TMP=%TEMP%\orca_%RANDOM%_%PL_FILE%"
call :download "https://raw.githubusercontent.com/%REPO%/%REF_2%/%PL_PATH%" "%PL_TMP%"
if not errorlevel 1 set "PL_SRC=%PL_TMP%"
:try_done
exit /b 0

rem ---------------------------------------------------------------------------
rem  fetch_manifest  --  fill PLAN_SRC: a local catalogue in --local mode,
rem  else a download (REF_1, then REF_2)
rem ---------------------------------------------------------------------------
:fetch_manifest
set "PLAN_SRC="
if not defined LOCAL_MODE goto :fm_remote
if exist "%HERE%\%MANIFEST_PATH%" ( set "PLAN_SRC=%HERE%\%MANIFEST_PATH%" & goto :fm_done )
:fm_remote
if "%REF_1%"=="" goto :fm_ref2
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%MANIFEST_PATH%" "%MANIFEST_TMP%"
if not errorlevel 1 ( set "PLAN_SRC=%MANIFEST_TMP%" & goto :fm_done )
:fm_ref2
if "%REF_2%"=="" goto :fm_done
call :download "https://raw.githubusercontent.com/%REPO%/%REF_2%/%MANIFEST_PATH%" "%MANIFEST_TMP%"
if not errorlevel 1 set "PLAN_SRC=%MANIFEST_TMP%"
:fm_done
exit /b 0

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
>  "%PLAN_FILE%" echo wave-overhangs^|Wave Overhangs^|0.0.5^|WaveOverhangs^|wave_overhangs_orca.py^|plugins/wave-overhangs/wave_overhangs_orca.py^|Wave Overhangs^|Wave Overhangs - Check setup^|end
>> "%PLAN_FILE%" echo unlayered-infill^|Unlayered Infill^|0.3.0^|UnlayeredInfill^|unlayered_infill_orca.py^|plugins/unlayered-infill/unlayered_infill_orca.py^|Unlayered Infill^|Unlayered Infill - Check setup^|end
exit /b 0

rem ---------------------------------------------------------------------------
rem  download %1=url %2=dest   (tries curl.exe, then PowerShell, then BITS)
rem ---------------------------------------------------------------------------
:download
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
for %%A in ("%~2") do if %%~zA GTR 2000 exit /b 0
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
rem ---------------------------------------------------------------------------
:select_data_dir
set "TARGET_DATA_DIR="
if not "%~1"=="" (
    set "TARGET_DATA_DIR=%~1"
    goto :ensure_data_dir
)
if defined ORCA_DATA_DIR (
    set "TARGET_DATA_DIR=%ORCA_DATA_DIR%"
    goto :ensure_data_dir
)
if not defined APPDATA (
    echo ERROR: APPDATA is not set. Pass Orca's data directory as the first argument.
    exit /b 1
)

echo.
echo === Locating your OrcaSlicer data directory ===
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

echo.
echo Select the data directory to install into:
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
echo.
set /p DPICK=Enter a number, or type a full path:
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
exit /b 0

rem ---------------------------------------------------------------------------
rem  self_update -- run the newest updater logic without ever rewriting this
rem  file while it is running.
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
:self_update
if defined ORCA_UPDATER_CHILD exit /b 0
if defined NO_SELF_UPDATE exit /b 0
if defined LOCAL_MODE exit /b 0
echo  Checking for a newer version of this updater...
set "NEWBAT=%TEMP%\orca_updater_%RANDOM%.bat"
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/Update-Orca-Plugins.bat" "%NEWBAT%"
if errorlevel 1 goto :su_skip
rem Never execute an unverified download: a 404 page or a wifi login portal
rem must fail this check rather than run.
findstr /b /c:"rem UPDATER_VERSION " "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
findstr /b /c:"set UPDATER_VERSION=" "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
rem Same version as ours? Nothing to do.
findstr /b /c:"rem UPDATER_VERSION %UPDATER_VERSION% end" "%NEWBAT%" >nul 2>nul
if not errorlevel 1 goto :su_skip
set "NEW_UV=?"
rem "rem UPDATER_VERSION 1.1.0 end" -- token 3 is the version, and the
rem trailing "end" absorbs the CR so it never lands in the variable.
for /f "usebackq tokens=3" %%V in (`findstr /b /c:"rem UPDATER_VERSION " "%NEWBAT%"`) do set "NEW_UV=%%V"
echo  Updater: this copy is v%UPDATER_VERSION%, v%NEW_UV% is available.
echo  Running the newer one for this update. Your .bat file is left as it is
echo  -- it will keep fetching the newest version every time you run it.
echo.
set "ORCA_UPDATER_CHILD=1"
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
rem  is not cooperating. They go in the Downloads folder next to the plugin
rem  copies. A failure here is not fatal; the plugins are already installed.
:stage_tools
if defined LOCAL_MODE exit /b 0
if not exist "%DLDIR%" mkdir "%DLDIR%" 2>nul
call :stage_one_tool "plugins/unlayered-infill/unlayered_infill_post.py" "unlayered_infill_post.py"
exit /b 0

:stage_one_tool
call :download "https://raw.githubusercontent.com/%REPO%/%REF_1%/%~1" "%DLDIR%\%~2"
if errorlevel 1 (
    echo  - tool %~2: download failed, skipped
    exit /b 0
)
echo  - tool %~2: saved to "%DLDIR%"
exit /b 0

rem ---------------------------------------------------------------------------
:child_done
if not defined CHILD_RC set "CHILD_RC=0"
exit /b %CHILD_RC%

rem ---------------------------------------------------------------------------
:no_plan
echo.
echo ERROR: no plan could be built -- the catalogue fetch and the fallback
echo list both produced nothing. This .bat is broken; re-download it.
goto :fail

:mkdir_failed
echo ERROR: Could not create the orca_plugins directory.
goto :fail

:help
echo Installs or updates the OrcaSlicer plugins
echo   Wave Overhangs, Unlayered Infill
echo from https://github.com/%REPO% into your Orca data folder.
echo.
echo   Update-Orca-Plugins.bat [data_dir] [--local] [--no-self-update] [--help]
echo.
echo     data_dir   OrcaSlicer data directory; found under %%APPDATA%% if omitted
echo     --local    install plugin files found next to this .bat, no downloads
echo     --no-self-update  do not hand over to a newer copy of this updater
echo     --help     this text
echo.
echo   Environment: ORCA_DATA_DIR, PLUGIN_BRANCH (default main), PLUGIN_ONLY,
echo   ORCA_NO_SELF_UPDATE.
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
exit /b 0

:fail
echo.
echo Install/update FAILED.
if exist "%PLAN_FILE%" del "%PLAN_FILE%" 2>nul
if exist "%MANIFEST_TMP%" del "%MANIFEST_TMP%" 2>nul
pause
exit /b 1

:done
if exist "%PLAN_FILE%" del "%PLAN_FILE%" 2>nul
if exist "%MANIFEST_TMP%" del "%MANIFEST_TMP%" 2>nul
pause
exit /b 0
