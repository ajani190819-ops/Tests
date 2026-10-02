@echo off
setlocal EnableExtensions EnableDelayedExpansion
title OrcaSlicer Plugins

rem ===========================================================================
rem  Orca-Plugins.bat -- the one file you need.
rem
rem  Double-click it and press Enter. It installs or updates every OrcaSlicer
rem  plugin in this repo into Orca's own plugin folder.
rem
rem  It remembers the two things you would otherwise retype every time: which
rem  build you want -- released or a test branch -- and which OrcaSlicer data
rem  folder to install into. Both are shown at the top of the menu, and both
rem  can be changed from it.
rem
rem  The heavy lifting is done by Update-Orca-Plugins.bat, which this script
rem  downloads fresh on every run. That file keeps its old name on purpose:
rem  copies people already have check that exact URL for their own updates,
rem  and renaming it would strand them on an old version with no warning.
rem
rem  Usage:
rem    Orca-Plugins.bat [data_dir] [--local] [--no-self-update] [--help]
rem ===========================================================================

rem The line below is read by older copies of this file to decide whether a
rem newer one exists. Keep both forms in step; tests/test_installer.py checks.
rem FRONTDOOR_VERSION 1.0.1 end
set "FRONTDOOR_VERSION=1.0.1"

set "REPO=ajani190819-ops/Tests"
set "STATE_DIR=%LOCALAPPDATA%\OrcaPluginUpdater"
set "BRANCH_STATE=%STATE_DIR%\branch.txt"
set "DATADIR_STATE=%STATE_DIR%\datadir.txt"
set "ENGINE=%TEMP%\orca_engine_%RANDOM%.bat"
set "BRANCHES=%TEMP%\orca_branches_%RANDOM%.txt"
set "ENGINE_NAME=Update-Orca-Plugins.bat"

set "REMEMBERED=main"
if exist "%BRANCH_STATE%" set /p REMEMBERED=<"%BRANCH_STATE%"
if not defined REMEMBERED set "REMEMBERED=main"
set "REMEMBERED_DIR="
if exist "%DATADIR_STATE%" set /p REMEMBERED_DIR=<"%DATADIR_STATE%"

set "NO_SELF_UPDATE="
if defined ORCA_NO_SELF_UPDATE set "NO_SELF_UPDATE=1"
set "LOCAL_MODE="
set "DATA_DIR_ARG="
set "ORIG_ARGS=%*"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--local" ( set "LOCAL_MODE=1" ) else if /i "%~1"=="--no-self-update" ( set "NO_SELF_UPDATE=1" ) else if /i "%~1"=="--help" ( goto :help ) else if not defined DATA_DIR_ARG set "DATA_DIR_ARG=%~1"
shift
goto :parse_args
:args_done

call :self_update
if defined SELF_UPDATED goto :child_done

:menu
echo.
echo ===============================================================
echo  OrcaSlicer Plugins
echo ===============================================================
if /i "%REMEMBERED%"=="main" (echo  Version:  main - the released build) else echo  Version:  %REMEMBERED% - a TEST BUILD
if defined REMEMBERED_DIR (echo  Folder:   %REMEMBERED_DIR%) else echo  Folder:   not chosen yet - you will be asked
echo ===============================================================
echo.
echo   [1] Install or update the plugins now
echo   [2] Choose a different version - advanced
echo   [3] Forget my remembered choices
echo   [Q] Quit
echo.
echo   Just press Enter to do [1].
echo.
set "PICK="
set /p PICK=Choice, or Enter to install: 
if not defined PICK goto :install_remembered
if "%PICK%"=="1" goto :install_remembered
if "%PICK%"=="2" goto :pick_version
if "%PICK%"=="3" goto :forget
if /i "%PICK%"=="Q" goto :cancel
echo That is not a menu choice.
goto :menu

:forget
del "%BRANCH_STATE%" 2>nul
del "%DATADIR_STATE%" 2>nul
set "REMEMBERED=main"
set "REMEMBERED_DIR="
echo.
echo Forgotten. Back to the released build, and you will be asked for the
echo OrcaSlicer folder on the next install.
goto :menu

:install_remembered
set "CHOSEN=%REMEMBERED%"
goto :install

rem ---------------------------------------------------------------------------
rem  Version picker. Lists live branches from GitHub so a typo cannot silently
rem  install nothing. Never falls back to another branch's files.
rem ---------------------------------------------------------------------------
:pick_version
del "%BRANCHES%" 2>nul
echo.
echo Fetching the live branch list from GitHub...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { $h=@{'User-Agent'='OrcaPlugins'}; $bs=Invoke-RestMethod -Headers $h -Uri 'https://api.github.com/repos/%REPO%/branches?per_page=100'; $now=[DateTimeOffset]::UtcNow; $rows=@(); foreach($b in $bs){ if($b.name -notmatch '^[A-Za-z0-9][A-Za-z0-9._/-]*$' -or $b.name.Contains('..')){continue}; $c=Invoke-RestMethod -Headers $h -Uri ([string]$b.commit.url); $d=[DateTimeOffset]::Parse($c.commit.committer.date); $age=$now-$d; if($age.TotalMinutes -lt 2){$a='updated just now'}elseif($age.TotalHours -lt 1){$a=('updated {0} minutes ago' -f [math]::Floor($age.TotalMinutes))}elseif($age.TotalDays -lt 1){$a=('updated {0} hours ago' -f [math]::Floor($age.TotalHours))}else{$a=('updated {0} days ago' -f [math]::Floor($age.TotalDays))}; $rows += [pscustomobject]@{Name=$b.name; Date=$d; Age=$a} }; $ordered=@($rows|Where-Object Name -eq 'main')+@($rows|Where-Object Name -ne 'main'|Sort-Object Date -Descending); [IO.File]::WriteAllLines('%BRANCHES%', @($ordered|ForEach-Object{$_.Name+'|'+$_.Age}), [Text.Encoding]::ASCII) } catch { Write-Host ('GitHub branch list failed: '+$_.Exception.Message); exit 1 }"
if errorlevel 1 goto :offline
if not exist "%BRANCHES%" goto :offline

:version_menu
echo.
echo ---------------------------------------------------------------
echo  Which build do you want?
echo ---------------------------------------------------------------
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCHES%") do if !COUNT! LSS 6 (
  set /a COUNT+=1
  set "MENU_!COUNT!=%%A"
  if /i "%%A"=="main" (echo   [!COUNT!] main - RELEASED - %%B) else echo   [!COUNT!] %%A - TEST BUILD - %%B
)
echo.
echo   [A] Show all branches
echo   [T] Type a branch name myself
echo   [R] Go back to the released build
echo   [M] Back to the main menu
echo.
set "PICK="
set /p PICK=Choice: 
if not defined PICK goto :version_menu
if /i "%PICK%"=="A" goto :all_branches
if /i "%PICK%"=="T" goto :type_branch
if /i "%PICK%"=="R" (set "CHOSEN=main"&goto :install)
if /i "%PICK%"=="M" (del "%BRANCHES%" 2>nul&goto :menu)
set "CHOSEN="
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (echo That is not a menu choice.&goto :version_menu)
goto :install

:all_branches
echo.
echo All live branches:
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCHES%") do (
  set /a COUNT+=1
  set "MENU_!COUNT!=%%A"
  if /i "%%A"=="main" (echo   [!COUNT!] main - RELEASED - %%B) else echo   [!COUNT!] %%A - TEST BUILD - %%B
)
echo.
set "PICK="
set /p PICK=Enter a number, or M for the short list: 
if /i "%PICK%"=="M" goto :version_menu
set "CHOSEN="
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (echo That is not a menu choice.&goto :all_branches)
goto :install

:offline
echo.
echo GitHub's live branch list could not be loaded.
echo Nothing will silently switch to another build.
echo   [T] Type a branch name you already know
echo   [R] Retry the live list
echo   [M] Back to the main menu
set "PICK="
set /p PICK=Enter T, R, or M: 
if /i "%PICK%"=="T" goto :type_branch
if /i "%PICK%"=="R" goto :pick_version
if /i "%PICK%"=="M" goto :menu
echo That is not a menu choice.
goto :offline

:type_branch
set "CHOSEN="
set /p CHOSEN=Type the exact branch name: 
if not defined CHOSEN goto :version_menu
echo(!CHOSEN!| findstr /r /x "[A-Za-z0-9][A-Za-z0-9._/-]*" >nul
if errorlevel 1 (echo Invalid branch name. Use letters, numbers, dot, dash, underscore, or slash.&goto :type_branch)
echo(!CHOSEN!| findstr /c:".." >nul
if not errorlevel 1 (echo Invalid branch name: two dots are not allowed.&goto :type_branch)
goto :install

rem ---------------------------------------------------------------------------
rem  Install. Fetch the engine for the chosen build, verify it is really the
rem  engine, then hand over. Remember the build only after the download works,
rem  so a dead branch name is never written to disk.
rem ---------------------------------------------------------------------------
:install
if not defined CHOSEN set "CHOSEN=main"
call :get_engine
if errorlevel 1 goto :engine_failed
if not exist "%STATE_DIR%" mkdir "%STATE_DIR%" 2>nul
>"%BRANCH_STATE%" echo %CHOSEN%
echo.
echo ===============================================================
echo  INSTALLING BUILD: %CHOSEN%
if /i "%CHOSEN%"=="main" (echo  This is the RELEASED build.) else echo  This is a TEST BUILD. It will not borrow files from main.
echo ===============================================================
echo.
set "PLUGIN_BRANCH=%CHOSEN%"
call "%ENGINE%" %DATA_DIR_ARG% --no-self-update
set "RC=%ERRORLEVEL%"
del "%ENGINE%" 2>nul
del "%BRANCHES%" 2>nul
echo.
echo Remembered for next time: %CHOSEN%
exit /b %RC%

:get_engine
del "%ENGINE%" 2>nul
if defined LOCAL_MODE (
  if exist "%~dp0%ENGINE_NAME%" (
    echo Using the engine next to this file - offline mode.
    copy /y "%~dp0%ENGINE_NAME%" "%ENGINE%" >nul
    goto :verify_engine
  )
  echo --local was given but %ENGINE_NAME% is not next to this file.
  exit /b 1
)
echo Downloading the installer for build %CHOSEN%...
call :download "https://raw.githubusercontent.com/%REPO%/%CHOSEN%/%ENGINE_NAME%" "%ENGINE%"
if errorlevel 1 exit /b 1
:verify_engine
if not exist "%ENGINE%" exit /b 1
rem A 404 page or a wifi login portal must fail here, never be executed.
for %%A in ("%ENGINE%") do if %%~zA LSS 2000 exit /b 1
findstr /b /c:"set UPDATER_VERSION=" "%ENGINE%" >nul 2>nul
if errorlevel 1 exit /b 1
findstr /c:"if defined PLUGIN_BRANCH set" "%ENGINE%" >nul 2>nul
if errorlevel 1 exit /b 1
exit /b 0

:engine_failed
del "%ENGINE%" 2>nul
del "%BRANCHES%" 2>nul
echo.
echo NOTHING WAS INSTALLED.
echo The installer could not be downloaded, or it is missing on build %CHOSEN%.
echo Your remembered build was left as it was, and no other build's files
echo were used instead.
echo.
echo If you are offline, put %ENGINE_NAME% next to this file and run:
echo   Orca-Plugins.bat --local
pause
exit /b 1

rem ---------------------------------------------------------------------------
rem  self_update -- run the newest front door without rewriting this file while
rem  it is running.
rem
rem  cmd.exe streams a .bat from disk by byte offset as it executes, so a file
rem  that overwrites itself mid-run can jump into garbage. This fetches the
rem  latest copy to a temp file and, if it is a different version, hands the
rem  run over to it. The file you double-click never changes, so a bad
rem  download cannot leave you without a working launcher.
rem
rem  It updates from the build you last chose, not always from main, so that
rem  testing a branch tests that branch's launcher too.
rem ---------------------------------------------------------------------------
:self_update
if defined ORCA_FRONTDOOR_CHILD exit /b 0
if defined NO_SELF_UPDATE exit /b 0
if defined LOCAL_MODE exit /b 0
echo Checking for a newer version of this launcher...
set "NEWBAT=%TEMP%\orca_frontdoor_%RANDOM%.bat"
call :download "https://raw.githubusercontent.com/%REPO%/%REMEMBERED%/Orca-Plugins.bat" "%NEWBAT%"
if errorlevel 1 goto :su_skip
findstr /b /c:"rem FRONTDOOR_VERSION " "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
findstr /c:"FRONTDOOR_VERSION=" "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
findstr /b /c:"rem FRONTDOOR_VERSION %FRONTDOOR_VERSION% end" "%NEWBAT%" >nul 2>nul
if not errorlevel 1 goto :su_skip
set "NEW_FV=?"
for /f "usebackq tokens=3" %%V in (`findstr /b /c:"rem FRONTDOOR_VERSION " "%NEWBAT%"`) do set "NEW_FV=%%V"
echo  This copy is v%FRONTDOOR_VERSION%, v%NEW_FV% is available.
echo  Running the newer one. Your file is left exactly as it is -- it will
echo  keep fetching the newest version every time you run it.
echo.
set "ORCA_FRONTDOOR_CHILD=1"
set "CHILD_RC=1"
call "%NEWBAT%" %ORIG_ARGS%
set "CHILD_RC=%ERRORLEVEL%"
set "SELF_UPDATED=1"
del "%NEWBAT%" 2>nul
exit /b 0
:su_skip
if exist "%NEWBAT%" del "%NEWBAT%" 2>nul
exit /b 0

:child_done
exit /b %CHILD_RC%

rem ---------------------------------------------------------------------------
rem  download -- curl if present, PowerShell otherwise. Same pair the engine
rem  uses, so a machine that can run one can run the other.
rem ---------------------------------------------------------------------------
:download
del "%~2" 2>nul
where curl.exe >nul 2>nul
if not errorlevel 1 (
    curl.exe -fLsS --retry 2 -o "%~2" "%~1"
    if not errorlevel 1 exit /b 0
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; try { [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri '%~1' -OutFile '%~2' } catch { Write-Host ('  ' + $_.Exception.Message); exit 1 }"
if errorlevel 1 exit /b 1
if not exist "%~2" exit /b 1
exit /b 0

:help
echo.
echo Orca-Plugins.bat -- installs the OrcaSlicer plugins from this repo.
echo.
echo   Orca-Plugins.bat [data_dir] [--local] [--no-self-update] [--help]
echo.
echo     [data_dir]         Orca's data directory. Found automatically if
echo                        you leave it out.
echo     --local            install using the %ENGINE_NAME% sitting next
echo                        to this file, with no downloads
echo     --no-self-update   do not hand over to a newer copy of this file
echo     --help             this text
echo.
echo   Environment: ORCA_DATA_DIR, PLUGIN_BRANCH, PLUGIN_ONLY,
echo   ORCA_NO_SELF_UPDATE.
echo.
echo   Your choices are remembered in:
echo     %STATE_DIR%
exit /b 0

:cancel
del "%ENGINE%" 2>nul
del "%BRANCHES%" 2>nul
exit /b 0
