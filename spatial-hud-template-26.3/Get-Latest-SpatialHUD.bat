@echo off
setlocal

rem Get-Latest-SpatialHUD.bat
rem Double-click this file to replace the Downloads copy with the latest
rem successful Spatial HUD release. This batch file downloads the maintained
rem PowerShell helper to a temporary file, runs it, and then deletes it.

rem Same two-candidate fetch as Update-SpatialHUD.bat: the higher
rem "SpatialHUD-Helper-Version: N" marker wins, so a saved copy of this file
rem keeps working after the branch it was downloaded from is merged.
set "SPATIALHUD_PS_URL=https://raw.githubusercontent.com/ajani190819-ops/Tests/main/spatial-hud-template-26.3/Get-Latest-SpatialHUD.ps1"
set "SPATIALHUD_PS_URLS=%SPATIALHUD_PS_URL% https://raw.githubusercontent.com/ajani190819-ops/Tests/arena/b4016c28-tests/spatial-hud-template-26.3/Get-Latest-SpatialHUD.ps1"
set "SPATIALHUD_PS_SCRIPT=%TEMP%\Get-Latest-SpatialHUD.ps1"
set "RESULT=1"

echo.
echo Spatial HUD - latest build downloader
echo.
echo Fetching the update helper...
del /f /q "%SPATIALHUD_PS_SCRIPT%" >nul 2>&1

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'SilentlyContinue'; $ProgressPreference = 'SilentlyContinue'; $best = -1; $target = $env:SPATIALHUD_PS_SCRIPT; Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue; foreach ($url in ($env:SPATIALHUD_PS_URLS -split '\s+')) { $candidate = $target + '.candidate'; try { Invoke-WebRequest -Uri $url -OutFile $candidate -ErrorAction Stop; $version = 0; $line = @(Get-Content -LiteralPath $candidate -TotalCount 40 | Where-Object { $_ -match 'SpatialHUD-Helper-Version:\s*(\d+)' } | Select-Object -First 1); if ($line -and ($line[0] -match 'SpatialHUD-Helper-Version:\s*(\d+)')) { $version = [int]$Matches[1] }; if ($version -gt $best) { $best = $version; Move-Item -LiteralPath $candidate -Destination $target -Force } } catch { } finally { Remove-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue } }"
if not exist "%SPATIALHUD_PS_SCRIPT%" powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri $env:SPATIALHUD_PS_URL -OutFile $env:SPATIALHUD_PS_SCRIPT"
if not exist "%SPATIALHUD_PS_SCRIPT%" goto :helper_failed
for %%F in ("%SPATIALHUD_PS_SCRIPT%") do if %%~zF LSS 1024 goto :helper_failed

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
