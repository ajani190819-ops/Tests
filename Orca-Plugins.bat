@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Orca-Plugins.ps1" %*
exit /b %ERRORLEVEL%
