@echo off
title Suite Report
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0SuiteReport.ps1"
if errorlevel 1 pause
