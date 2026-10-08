@echo off
setlocal

rem Update-SpatialHUD.bat
rem Keep this one file anywhere convenient and double-click it to install a
rem Spatial HUD build directly into the remembered Modrinth profile mods folder.
rem It refreshes its maintained PowerShell helper from GitHub before every run,
rem so the menu and safety checks stay current. The menu can then install the
rem newest build on any branch, or a build of one branch you pick by name.

rem Two candidate copies of the helper are tried. Each one starts with a
rem "SpatialHUD-Helper-Version: N" line; the higher version wins, and this order
rem breaks a tie, so the released main copy takes over once main is current.
rem A .bat saved on your computer therefore keeps working while a test branch
rem holds the newest menu.
set "SPATIALHUD_UPDATER_URL=https://raw.githubusercontent.com/ajani190819-ops/Tests/main/spatial-hud-template-26.3/Update-SpatialHUD.ps1"
set "SPATIALHUD_UPDATER_URLS=%SPATIALHUD_UPDATER_URL% https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/b4016c28-tests/spatial-hud-template-26.3/Update-SpatialHUD.ps1"
set "SPATIALHUD_UPDATER_PS=%TEMP%\Update-SpatialHUD.ps1"
set "RESULT=1"

echo.
echo Spatial HUD -- Modrinth install / update
echo.
echo Fetching the current updater...
del /f /q "%SPATIALHUD_UPDATER_PS%" >nul 2>&1

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'SilentlyContinue'; $ProgressPreference = 'SilentlyContinue'; $best = -1; $target = $env:SPATIALHUD_UPDATER_PS; Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue; foreach ($url in ($env:SPATIALHUD_UPDATER_URLS -split '\s+')) { $candidate = $target + '.candidate'; try { Invoke-WebRequest -Uri $url -OutFile $candidate -ErrorAction Stop; $version = 0; $line = @(Get-Content -LiteralPath $candidate -TotalCount 40 | Where-Object { $_ -match 'SpatialHUD-Helper-Version:\s*(\d+)' } | Select-Object -First 1); if ($line -and ($line[0] -match 'SpatialHUD-Helper-Version:\s*(\d+)')) { $version = [int]$Matches[1] }; if ($version -gt $best) { $best = $version; Move-Item -LiteralPath $candidate -Destination $target -Force } } catch { } finally { Remove-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue } }"

rem Belt and braces: if the version check produced no helper at all, fetch the
rem single released copy directly, exactly as earlier versions of this file did.
if not exist "%SPATIALHUD_UPDATER_PS%" powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri $env:SPATIALHUD_UPDATER_URL -OutFile $env:SPATIALHUD_UPDATER_PS"
if not exist "%SPATIALHUD_UPDATER_PS%" goto :helper_failed
for %%F in ("%SPATIALHUD_UPDATER_PS%") do if %%~zF LSS 1024 goto :helper_failed

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
