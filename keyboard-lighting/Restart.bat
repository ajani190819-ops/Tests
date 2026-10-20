@echo off
REM ===================================================================
REM  Keyboard Lighting - One-Click Recovery / Restart
REM  Stops any stuck tray icon or engine process and starts cleanly.
REM ===================================================================
setlocal
cd /d "%~dp0"
title Keyboard Lighting - Restart

net session >nul 2>&1
if %errorlevel%==0 goto :run

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
exit /b 0

:run
powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ".\Tray.ps1" -Restart -NoElevate
exit /b 0
