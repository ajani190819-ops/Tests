@echo off
setlocal

rem Get-Latest-SpatialHUD.bat
rem Double-click this file to replace the Downloads copy with the latest
rem successful Spatial HUD release. This batch file downloads the maintained
rem PowerShell helper to a temporary file, runs it, and then deletes it.

set "SPATIALHUD_PS_URL=https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/c83497e6-tests/spatial-hud-template-26.3/Get-Latest-SpatialHUD.ps1"
set "SPATIALHUD_PS_SCRIPT=%TEMP%\Get-Latest-SpatialHUD.ps1"
set "RESULT=1"

echo.
echo Spatial HUD - latest build downloader
echo.
echo Fetching the update helper...
del /f /q "%SPATIALHUD_PS_SCRIPT%" >nul 2>&1

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri $env:SPATIALHUD_PS_URL -OutFile $env:SPATIALHUD_PS_SCRIPT"
if errorlevel 1 goto :helper_failed

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SPATIALHUD_PS_SCRIPT%" -OpenFolder
set "RESULT=%ERRORLEVEL%"
del /f /q "%SPATIALHUD_PS_SCRIPT%" >nul 2>&1

if not "%RESULT%"=="0" goto :download_failed

echo.
echo Finished. The current Spatial HUD jar is selected in Downloads.
echo Install that one file through Modrinth when you are ready.
goto :done

:helper_failed
del /f /q "%SPATIALHUD_PS_SCRIPT%" >nul 2>&1
echo.
echo Could not fetch the update helper. Your existing Spatial HUD download was not changed.
goto :failed

:download_failed
echo.
echo Spatial HUD was not changed. See the message above for details.
goto :failed

:failed
echo Check your Internet connection and try again.

:done
echo.
pause
exit /b %RESULT%
