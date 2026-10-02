@echo off
setlocal EnableExtensions
title OrcaSlicer Plugins

rem ===========================================================================
rem  This file is now part of Orca-Plugins.bat.
rem
rem  It is kept as a forwarder so copies already sitting in Downloads folders,
rem  and the older launchers that download it as their install engine, keep
rem  working instead of silently doing nothing. It does no installing of its
rem  own: it finds or fetches Orca-Plugins.bat and hands the whole run over,
rem  arguments and all.
rem
rem  There is nothing to maintain here. Everything -- the menu, the build
rem  picker, the OrcaSlicer folder picker, the remembered choices, the install
rem  engine and self-update -- lives in Orca-Plugins.bat.
rem
rem  Compatibility notes, because copies of the old two-file updater still
rem  check this exact URL for their own updates:
rem    * the version lines below let an old Update-Orca-Plugins.bat hand its
rem      run over to this file, which then hands it to Orca-Plugins.bat;
rem    * the PLUGIN_BRANCH line below is how an old Orca-Plugins.bat launcher
rem      tells this file which build it wanted. Keep both, or those copies
rem      stop with "NOTHING WAS INSTALLED".
rem ===========================================================================

rem UPDATER_VERSION 2.0.1 end
set UPDATER_VERSION=2.0.1
if defined PLUGIN_BRANCH set "FWD_REF=%PLUGIN_BRANCH%"
if not defined FWD_REF set "FWD_REF=main"

set "REPO=ajani190819-ops/Tests"
set "TEMPCOPY="

echo.
echo  ---------------------------------------------------------------
echo   Update-Orca-Plugins.bat is now part of Orca-Plugins.bat
echo  ---------------------------------------------------------------
echo   Same job, and now one file does all of it: the menu, the build
echo   picker, the OrcaSlicer folder picker, and the install engine.
echo   Handing over now.
echo.

set "FRONTDOOR=%~dp0Orca-Plugins.bat"
if exist "%FRONTDOOR%" goto :run

set "FRONTDOOR=%TEMP%\Orca-Plugins_%RANDOM%.bat"
echo  Fetching Orca-Plugins.bat from build %FWD_REF%...
call :download "https://raw.githubusercontent.com/%REPO%/%FWD_REF%/Orca-Plugins.bat" "%FRONTDOOR%"
if errorlevel 1 goto :failed
rem Never run an unverified download: a 404 page or a wifi portal must fail.
findstr /b /c:"rem FRONTDOOR_VERSION " "%FRONTDOOR%" >nul 2>nul
if errorlevel 1 goto :failed
set "TEMPCOPY=1"

:run
call "%FRONTDOOR%" %*
set "RC=%ERRORLEVEL%"
if defined TEMPCOPY del "%FRONTDOOR%" 2>nul
echo.
echo  Tip: next time, double-click Orca-Plugins.bat directly.
exit /b %RC%

:download
del "%~2" 2>nul
where curl.exe >nul 2>nul
if not errorlevel 1 (
    curl.exe -fLsS --retry 2 -o "%~2" "%~1"
    if not errorlevel 1 exit /b 0
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri '%~1' -OutFile '%~2' } catch { Write-Host ('  ' + $_.Exception.Message); exit 1 }"
if errorlevel 1 exit /b 1
if not exist "%~2" exit /b 1
exit /b 0

:failed
if exist "%FRONTDOOR%" del "%FRONTDOOR%" 2>nul
echo.
echo  Could not fetch Orca-Plugins.bat, so nothing was installed.
echo  Download it yourself from:
echo    https://raw.githubusercontent.com/%REPO%/%FWD_REF%/Orca-Plugins.bat
pause
exit /b 1
