@echo off
setlocal

rem Update-SpatialHUD.bat
rem Keep this one file anywhere convenient and double-click it to install the
rem newest successful Spatial HUD build directly into the remembered Modrinth
rem profile mods folder. It refreshes its maintained PowerShell helper from
rem GitHub before every run, so the menu and safety checks stay current.

set "SPATIALHUD_UPDATER_URL=https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/c83497e6-tests/spatial-hud-template-26.3/Update-SpatialHUD.ps1"
set "SPATIALHUD_UPDATER_PS=%TEMP%\Update-SpatialHUD.ps1"
set "RESULT=1"

echo.
echo Spatial HUD -- Modrinth install / update
echo.
echo Fetching the current updater...
del /f /q "%SPATIALHUD_UPDATER_PS%" >nul 2>&1

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri $env:SPATIALHUD_UPDATER_URL -OutFile $env:SPATIALHUD_UPDATER_PS"
if errorlevel 1 goto :helper_failed

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SPATIALHUD_UPDATER_PS%"
set "RESULT=%ERRORLEVEL%"
del /f /q "%SPATIALHUD_UPDATER_PS%" >nul 2>&1

if not "%RESULT%"=="0" goto :update_failed
goto :done

:helper_failed
del /f /q "%SPATIALHUD_UPDATER_PS%" >nul 2>&1
echo.
echo Could not fetch the updater. Your Modrinth profile was not changed.
goto :failed

:update_failed
echo.
echo Spatial HUD was not changed. See the updater message above for details.
goto :failed

:failed
echo Check your Internet connection and try again.

:done
echo.
pause
exit /b %RESULT%
