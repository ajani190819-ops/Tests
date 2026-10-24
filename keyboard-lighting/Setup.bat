@echo off
REM ===================================================================
REM  Keyboard Lighting - SETUP
REM  Double-click this file and approve the Administrator prompt.
REM ===================================================================
setlocal
cd /d "%~dp0"
title Keyboard Lighting - Setup

net session >nul 2>&1
if %errorlevel%==0 goto :elevated

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
exit /b 0

:elevated
echo.
echo   Getting the latest setup...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol='Tls12'; $br='arena/e895b94f-tests'; try { if((Invoke-WebRequest 'https://raw.githubusercontent.com/ajani190819-ops/Tests/main/project-hub/Setup.ps1' -Method Head -UseBasicParsing -TimeoutSec 8 -ErrorAction Stop).StatusCode -eq 200){ $br='main' } } catch {}; try { Invoke-WebRequest ('https://raw.githubusercontent.com/ajani190819-ops/Tests/'+$br+'/keyboard-lighting/Setup.ps1') -OutFile 'Setup.ps1.new' -UseBasicParsing -TimeoutSec 25 -ErrorAction Stop; if((Get-Item 'Setup.ps1.new').Length -gt 500){ Move-Item -LiteralPath 'Setup.ps1.new' -Destination 'Setup.ps1' -Force; Unblock-File 'Setup.ps1' } } catch {}"

if exist "Setup.ps1" goto :run_kbl
echo.
echo   Could not download Setup.ps1.
echo   Check your internet connection and run this again.
echo.
pause
exit /b 1

:run_kbl
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Setup.ps1" -NoElevate
exit /b 0
