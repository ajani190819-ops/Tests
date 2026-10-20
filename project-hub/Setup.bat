@echo off
setlocal
cd /d "%~dp0"
title Project Hub - Setup

if exist "Setup.ps1" goto :have_setup

echo.
echo   Downloading Project Hub Setup from GitHub...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol='Tls12'; foreach($b in @('main','arena/e895b94f-tests')){ try { Invoke-WebRequest ('https://raw.githubusercontent.com/ajani190819-ops/Tests/'+$b+'/project-hub/Setup.ps1') -OutFile 'Setup.ps1.new' -UseBasicParsing -TimeoutSec 20 -ErrorAction Stop; if((Get-Item 'Setup.ps1.new').Length -gt 200){ Move-Item -LiteralPath 'Setup.ps1.new' -Destination 'Setup.ps1' -Force; break } } catch {} }"

:have_setup
if exist "Setup.ps1" goto :run_setup

echo.
echo   [ERROR] Could not find or download Setup.ps1.
echo           Check your internet connection and run Setup.bat again.
echo.
pause
exit /b 1

:run_setup
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Setup.ps1"
echo.
pause
