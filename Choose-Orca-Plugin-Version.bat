@echo off
setlocal EnableExtensions
title OrcaSlicer Plugins

rem ===========================================================================
rem  This file was renamed to Orca-Plugins.bat.
rem
rem  It is kept as a forwarder so shortcuts, and copies already sitting in
rem  people's Downloads folders, keep working instead of silently doing
rem  nothing. It does no installing of its own: it finds or fetches
rem  Orca-Plugins.bat and hands the whole run over, arguments and all.
rem
rem  There is nothing to maintain here. Everything -- the menu, the version
rem  picker, the remembered folder, self-update -- lives in Orca-Plugins.bat.
rem ===========================================================================

set "REPO=ajani190819-ops/Tests"
set "TEMPCOPY="

echo.
echo  ---------------------------------------------------------------
echo   This is now called Orca-Plugins.bat
echo  ---------------------------------------------------------------
echo   Same job, and now one file does all of it: install, pick a
echo   version, and set the OrcaSlicer folder. Handing over now.
echo.

set "FRONTDOOR=%~dp0Orca-Plugins.bat"
if exist "%FRONTDOOR%" goto :run

set "FRONTDOOR=%TEMP%\Orca-Plugins_%RANDOM%.bat"
echo  Fetching Orca-Plugins.bat...
call :download "https://raw.githubusercontent.com/%REPO%/main/Orca-Plugins.bat" "%FRONTDOOR%"
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
echo    https://raw.githubusercontent.com/%REPO%/main/Orca-Plugins.bat
pause
exit /b 1
