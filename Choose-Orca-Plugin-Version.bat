@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Choose OrcaSlicer plugin version

set "REPO=ajani190819-ops/Tests"
set "UPDATER=%~dp0Update-Orca-Plugins.bat"
set "STATE_DIR=%LOCALAPPDATA%\OrcaPluginUpdater"
set "STATE_FILE=%STATE_DIR%\branch.txt"
set "BRANCH_FILE=%TEMP%\orca_branches_%RANDOM%.txt"
set "REMEMBERED=main"
if exist "%STATE_FILE%" set /p REMEMBERED=<"%STATE_FILE%"

if not exist "%UPDATER%" (
  echo ERROR: Update-Orca-Plugins.bat is not beside this chooser.
  echo Put both .bat files in the same folder, then try again.
  pause
  exit /b 1
)

:refresh
del "%BRANCH_FILE%" 2>nul
echo Fetching the live branch list from public GitHub...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { $h=@{'User-Agent'='OrcaPluginChooser'}; $bs=@(Invoke-RestMethod -Headers $h -Uri 'https://api.github.com/repos/ajani190819-ops/Tests/branches?per_page=100'); $now=[DateTimeOffset]::UtcNow; $rows=@(); foreach($b in $bs){ if($b.name -notmatch '^[A-Za-z0-9][A-Za-z0-9._/-]*$' -or $b.name.Contains('..')){continue}; $c=Invoke-RestMethod -Headers $h -Uri $b.commit.url; $d=[DateTimeOffset]::Parse($c.commit.committer.date); $age=$now-$d; if($age.TotalMinutes -lt 2){$a='updated just now'}elseif($age.TotalHours -lt 1){$a=('updated {0} minutes ago' -f [math]::Floor($age.TotalMinutes))}elseif($age.TotalDays -lt 1){$a=('updated {0} hours ago' -f [math]::Floor($age.TotalHours))}else{$a=('updated {0} days ago' -f [math]::Floor($age.TotalDays))}; $rows += [pscustomobject]@{Name=$b.name; Date=$d; Age=$a} }; $ordered=@($rows|Where-Object Name -eq 'main')+@($rows|Where-Object Name -ne 'main'|Sort-Object Date -Descending); [IO.File]::WriteAllLines('%BRANCH_FILE%', @($ordered|ForEach-Object{$_.Name+'|'+$_.Age}), [Text.Encoding]::ASCII) } catch { Write-Host ('GitHub branch list failed: '+$_.Exception.Message); exit 1 }"
if errorlevel 1 goto :offline
if not exist "%BRANCH_FILE%" goto :offline

:menu
echo.
echo ===============================================================
echo  Choose which plugin build to install
echo  Remembered choice: %REMEMBERED%
echo ===============================================================
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCH_FILE%") do if !COUNT! LSS 6 (
  set /a COUNT+=1
  set "MENU_!COUNT!=%%A"
  if /i "%%A"=="main" (echo   [!COUNT!] main - RELEASED - %%B) else echo   [!COUNT!] %%A - TEST BUILD - %%B
)
echo.
echo   [A] Show all branches
echo   [T] Type a branch name myself
echo   [R] Return to released main
echo   [Q] Cancel
echo.
set "PICK="
set /p PICK=Enter a number or letter: 
if /i "%PICK%"=="A" goto :all
if /i "%PICK%"=="T" goto :type
if /i "%PICK%"=="R" (set "CHOSEN=main"&goto :chosen)
if /i "%PICK%"=="Q" goto :cancel
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (echo That is not a menu choice.&goto :menu)
goto :chosen

:all
echo.
echo All live branches:
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCH_FILE%") do (
  set /a COUNT+=1
  set "MENU_!COUNT!=%%A"
  if /i "%%A"=="main" (echo   [!COUNT!] main - RELEASED - %%B) else echo   [!COUNT!] %%A - TEST BUILD - %%B
)
echo.
set "PICK="
set /p PICK=Enter a number, or M for the short menu: 
if /i "%PICK%"=="M" goto :menu
set "CHOSEN="
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (echo That is not a menu choice.&goto :all)
goto :chosen

:offline
echo.
echo GitHub's live branch list could not be loaded.
echo Nothing will silently switch to another branch.
echo   [T] Type a branch name you already know
echo   [R] Retry the live list
echo   [Q] Cancel
set "PICK="
set /p PICK=Enter T, R, or Q: 
if /i "%PICK%"=="T" goto :type
if /i "%PICK%"=="R" goto :refresh
if /i "%PICK%"=="Q" goto :cancel
echo That is not a menu choice.
goto :offline

:type
set "CHOSEN="
set /p CHOSEN=Type the exact branch name: 
if not defined CHOSEN goto :menu
echo(!CHOSEN!| findstr /r /x "[A-Za-z0-9][A-Za-z0-9._/-]*" >nul
if errorlevel 1 (echo Invalid branch name. Use letters, numbers, dot, dash, underscore, or slash.&goto :type)
echo(!CHOSEN!| findstr /c:".." >nul
if not errorlevel 1 (echo Invalid branch name: two dots are not allowed.&goto :type)

:chosen
if not exist "%STATE_DIR%" mkdir "%STATE_DIR%" 2>nul
>"%STATE_FILE%" echo %CHOSEN%
set "PLUGIN_BRANCH=%CHOSEN%"
echo.
echo ===============================================================
echo  SELECTED BRANCH: %CHOSEN%
if /i "%CHOSEN%"=="main" (echo  This is the RELEASED build.) else echo  This is a TEST BUILD. It will never borrow files from main.
echo ===============================================================
echo.
call "%UPDATER%"
set "RC=%ERRORLEVEL%"
del "%BRANCH_FILE%" 2>nul
echo.
echo Chooser remembered: %CHOSEN%
exit /b %RC%

:cancel
del "%BRANCH_FILE%" 2>nul
exit /b 0
