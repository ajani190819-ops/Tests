@echo off
setlocal
cd /d "%~dp0"
title Project Hub Setup

set "BRANCH=%PROJECT_HUB_BRANCH%"
if not defined BRANCH set "BRANCH=main"
set "BASE=https://raw.githubusercontent.com/ajani190819-ops/Tests/%BRANCH%/project-hub"

if not exist "Setup.ps1" (
    echo.
    echo   Downloading Project Hub Setup.ps1 from GitHub...
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol='Tls12'; Invoke-WebRequest '%BASE%/Setup.ps1' -OutFile 'Setup.ps1.new' -UseBasicParsing -TimeoutSec 20"
    if exist "Setup.ps1.new" (
        findstr /i /r "^.!doctype ^.html" "Setup.ps1.new" >nul 2>nul && del /f /q "Setup.ps1.new"
    )
    if exist "Setup.ps1.new" move /y "Setup.ps1.new" "Setup.ps1" >nul
)

if not exist "Setup.ps1" (
    echo.
    echo   [ERROR] Could not find or download Setup.ps1.
    echo           Check your internet connection and try again.
    echo.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup.ps1"
if errorlevel 1 pause
