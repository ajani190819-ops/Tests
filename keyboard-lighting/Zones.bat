@echo off
REM Zone checker - work out which light is which, and correct it.
setlocal
cd /d "%~dp0"
title Keyboard Lighting - Zone Checker

net session >nul 2>&1
if %errorlevel%==0 goto :elevated

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
exit /b 0

:elevated
echo   Getting the latest zone checker...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol='Tls12'; foreach($b in @('main','arena/e895b94f-tests')){ try { Invoke-WebRequest ('https://raw.githubusercontent.com/ajani190819-ops/Tests/'+$b+'/keyboard-lighting/Zones.ps1') -OutFile 'Zones.ps1.new' -UseBasicParsing -TimeoutSec 25 -ErrorAction Stop; if((Get-Item 'Zones.ps1.new').Length -gt 200){ Move-Item -LiteralPath 'Zones.ps1.new' -Destination 'Zones.ps1' -Force; Unblock-File 'Zones.ps1'; break } } catch {} }"

if exist "Zones.ps1" goto :run_zones
echo   Could not download Zones.ps1 and no local copy was found.
pause
exit /b 1

:run_zones
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Zones.ps1"
pause
