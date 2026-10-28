@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Spatial HUD installer

rem ===========================================================================
rem  Update-SpatialHUD.bat -- the Spatial HUD installer. One file: the menu,
rem  the build picker, the Modrinth folder picker, the download check, and the
rem  install itself. The same shape as Orca-Plugins.bat.
rem
rem  Double-click it and press Enter. It installs the newest Spatial HUD build
rem  into your Modrinth F5W mods folder. Press 2 to install a test branch
rem  instead, for example arena/b4016c28-tests.
rem
rem  It checks the download before it changes anything: the jar must be the
rem  size GitHub reported and must be the Spatial HUD mod. Your old jar is
rem  moved to a backup folder, not deleted, and is put back if the copy fails.
rem
rem  It remembers the build and the folder, and only saves them after an
rem  install has worked, so a dead branch name is never remembered.
rem
rem  Usage:
rem    Update-SpatialHUD.bat                install with the menu
rem    Update-SpatialHUD.bat --no-self-update   never hand over to a newer copy
rem    Update-SpatialHUD.bat --help
rem
rem  This file must keep CRLF line endings. *.bat -text in .gitattributes
rem  keeps git from re-normalising them. Windows shows an "Unknown Publisher"
rem  prompt the first time a downloaded .bat runs: right-click the file,
rem  Properties, tick Unblock, and it never appears again.
rem ===========================================================================

rem UPDATER_VERSION 3 end
set "UPDATER_VERSION=3"

set "REPO=ajani190819-ops/Tests"
set "SELF_REF=arena/b4016c28-tests"
set "BUILD_PREFIX=spatial-hud-build-"
set "LATEST_TAG=spatial-hud-latest"
set "JAR_NAME=spatial-hud-1.0.0.jar"
set "DEFAULT_MODS=%APPDATA%\ModrinthApp\profiles\F5W\mods"
set "STATE_DIR=%LOCALAPPDATA%\SpatialHudUpdater"
set "BUILD_STATE=%STATE_DIR%\build.txt"
set "MODS_STATE=%STATE_DIR%\mods-folder.txt"
set "BACKUP_DIR=%STATE_DIR%\backup"
set "BRANCHES=%TEMP%\spatial-hud-builds-%RANDOM%.txt"
set "JAR_TMP=%TEMP%\spatial-hud-download-%RANDOM%.jar"

if /i "%~1"=="--help" goto :help
if /i "%~1"=="-h" goto :help
set "NO_SELF_UPDATE="
if /i "%~1"=="--no-self-update" set "NO_SELF_UPDATE=1"

call :self_update
if defined SELF_UPDATED goto :end

call :load_state
goto :menu

:help
echo.
echo Spatial HUD installer. Double-click it and press Enter to install the
echo newest build into your Modrinth F5W mods folder.
echo.
echo   --no-self-update   do not look for a newer copy of this file
echo.
exit /b 0

rem ---------------------------------------------------------------------------
rem  Remembered choices. The defaults are the newest build and the default
rem  Modrinth folder.
rem ---------------------------------------------------------------------------
:load_state
set "BUILD=latest"
if exist "%BUILD_STATE%" set /p BUILD=<"%BUILD_STATE%"
set "MODS_DIR=%DEFAULT_MODS%"
if exist "%MODS_STATE%" set /p MODS_DIR=<"%MODS_STATE%"
goto :eof

:menu
echo.
echo ===============================================================
echo  Spatial HUD -- install or update
echo ===============================================================
if /i "%BUILD%"=="latest" (echo  Build:   Newest build, any branch) else echo  Build:   %BUILD% - TEST BUILD
echo  Folder:  %MODS_DIR%
echo ===============================================================
echo.
echo   [1] Install or update Spatial HUD now
echo   [2] Choose the build - main or one of the five newest branches
echo   [3] Change the Modrinth mods folder
echo   [4] Forget my choices - newest build and the default folder
echo   [Q] Quit
echo.
echo   Just press Enter to do [1].
echo.
set "PICK="
set /p "PICK=Choice, or Enter to install: "
if not defined PICK goto :install
if "%PICK%"=="1" goto :install
if "%PICK%"=="2" goto :pick_build
if "%PICK%"=="3" goto :pick_folder
if "%PICK%"=="4" goto :forget
if /i "%PICK%"=="Q" goto :end
echo That is not a menu choice.
goto :menu

:forget
del "%BUILD_STATE%" 2>nul
del "%MODS_STATE%" 2>nul
set "BUILD=latest"
set "MODS_DIR=%DEFAULT_MODS%"
echo.
echo Forgotten. The newest build and the default folder are back.
goto :menu

:pick_folder
call :choose_folder
goto :menu

rem ---------------------------------------------------------------------------
rem  Folder picker. Enter keeps the current folder. A typed folder is checked
rem  and remembered only if it exists or you agree to create it.
rem ---------------------------------------------------------------------------
:choose_folder
echo.
echo Modrinth mods folder: %MODS_DIR%
echo Press Enter to keep it, or paste another folder.
set "NEW_DIR="
set /p "NEW_DIR=Folder: "
if not defined NEW_DIR goto :eof
set "NEW_DIR=%NEW_DIR:"=%"
if not exist "%NEW_DIR%" (
    set "CREATE="
    set /p "CREATE=That folder does not exist. Create it? [Y/n]: "
    if /i "!CREATE!"=="n" (
        echo Folder not changed.
        goto :eof
    )
    mkdir "%NEW_DIR%" 2>nul
    if not exist "%NEW_DIR%" (
        echo Could not create that folder. Folder not changed.
        goto :eof
    )
)
set "MODS_DIR=%NEW_DIR%"
if not exist "%STATE_DIR%" mkdir "%STATE_DIR%" 2>nul
>"%MODS_STATE%" echo %MODS_DIR%
echo Folder saved: %MODS_DIR%
goto :eof

rem ---------------------------------------------------------------------------
rem  Build picker. The list comes from GitHub's published builds. Each row is
rem  numbered as it is printed, and the typed number is checked against the
rem  same list, so the numbers on screen are the ones that work.
rem ---------------------------------------------------------------------------
:pick_build
set "OLD_BUILD=%BUILD%"
del "%BRANCHES%" 2>nul
echo.
echo Fetching the list of published builds from GitHub...
powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $ErrorActionPreference='Stop'; try { $h=@{'User-Agent'='SpatialHUD-Updater'}; $rs=@(Invoke-RestMethod -Headers $h -Uri ('https://api.github.com/repos/%REPO%/releases?per_page=100')); $rows=@(); foreach($r in $rs){ $tag=[string]$r.tag_name; if(-not $tag.StartsWith('%BUILD_PREFIX%')){continue}; $j=@($r.assets | Where-Object { $_.name -like 'spatial-hud-*.jar' })[0]; if($j -eq $null){continue}; $m=[regex]::Match([string]$r.body,'(?m)^Branch:\s*(\S.*?)\s*$'); if($m.Success){$b=$m.Groups[1].Value}else{$b=$tag.Substring('%BUILD_PREFIX%'.Length)}; $rows+=[pscustomobject]@{Name=$b;When=[DateTimeOffset]::Parse([string]$j.updated_at)} }; $main=@($rows | Where-Object { $_.Name -eq 'main' }); $others=@($rows | Where-Object { $_.Name -ne 'main' } | Sort-Object When -Descending); $all=@($main) + @($others); $lines=@($all | ForEach-Object { $age=[DateTimeOffset]::UtcNow-$_.When; if($age.TotalHours -lt 1){$ago=[string][math]::Max(1,[math]::Floor($age.TotalMinutes))+' min ago'}elseif($age.TotalDays -lt 1){$ago=[string][math]::Floor($age.TotalHours)+' h ago'}else{$ago=[string][math]::Floor($age.TotalDays)+' days ago'}; $_.Name+'|built '+$ago }); [IO.File]::WriteAllLines('%BRANCHES%', [string[]]$lines, [Text.Encoding]::ASCII) } catch { Write-Host ('GitHub build list failed: '+$_.Exception.Message); exit 1 }"
if errorlevel 1 goto :build_offline
if not exist "%BRANCHES%" goto :build_offline

:build_menu
echo.
echo ---------------------------------------------------------------
echo  Which build do you want to install?
echo  Released main first, then the five newest branches.
echo ---------------------------------------------------------------
set /a COUNT=0
for /f "usebackq tokens=1,* delims=|" %%A in ("%BRANCHES%") do if !COUNT! LSS 6 (
    set /a COUNT+=1
    set "MENU_!COUNT!=%%A"
    echo   [!COUNT!] %%A - %%B
)
if !COUNT! EQU 0 echo   No branch builds are published yet.
echo.
echo   [L] The newest build, any branch
echo   [T] Type a branch name myself
echo   [M] Back to the main menu
echo.
set "PICK="
set /p "PICK=Choice: "
if not defined PICK goto :build_menu
if /i "%PICK%"=="L" (
    set "BUILD=latest"
    goto :install
)
if /i "%PICK%"=="T" goto :type_branch
if /i "%PICK%"=="M" (
    set "BUILD=%OLD_BUILD%"
    goto :menu
)
set "CHOSEN="
for /L %%I in (1,1,%COUNT%) do if "%PICK%"=="%%I" set "CHOSEN=!MENU_%%I!"
if not defined CHOSEN (
    echo That is not a menu choice.
    goto :build_menu
)
set "BUILD=%CHOSEN%"
goto :install

:type_branch
set "CHOSEN="
echo.
echo Type the branch name exactly, for example arena/b4016c28-tests
set /p "CHOSEN=Branch: "
if not defined CHOSEN goto :build_menu
echo(%CHOSEN%| findstr /r /x "[A-Za-z0-9][A-Za-z0-9._/-]*" >nul
if errorlevel 1 (
    echo Invalid branch name. Use letters, numbers, dot, dash, underscore, or slash.
    goto :type_branch
)
echo(%CHOSEN%| findstr /c:".." >nul
if not errorlevel 1 (
    echo Invalid branch name: two dots are not allowed.
    goto :type_branch
)
set "BUILD=%CHOSEN%"
goto :install

:build_offline
echo.
echo GitHub's build list could not be loaded. Nothing was changed.
echo   [L] Install the newest build, any branch
echo   [T] Type a branch name you already know
echo   [R] Retry the list
echo   [M] Back to the main menu
set "PICK="
set /p "PICK=Choice: "
if /i "%PICK%"=="L" (
    set "BUILD=latest"
    goto :install
)
if /i "%PICK%"=="T" goto :type_branch
if /i "%PICK%"=="R" goto :pick_build
if /i "%PICK%"=="M" goto :menu
echo That is not a menu choice.
goto :build_offline

rem ---------------------------------------------------------------------------
rem  Install. Step 1 downloads and checks the jar. Nothing on disk changes
rem  until that has passed. Step 2 swaps the jar in, keeping the old one as a
rem  backup. Step 3 saves the choices.
rem ---------------------------------------------------------------------------
:install
if not defined BUILD set "BUILD=latest"
if /i "%BUILD%"=="latest" (set "RELEASE_TAG=%LATEST_TAG%") else (set "RELEASE_TAG=%BUILD_PREFIX%%BUILD:/=-%")

echo.
echo ===============================================================
echo  Spatial HUD -- install
echo ===============================================================
if /i "%BUILD%"=="latest" (echo  Build:   Newest build, any branch) else echo  Build:   %BUILD% - TEST BUILD
echo  Release: %RELEASE_TAG%
echo  Folder:  %MODS_DIR%
echo ===============================================================
echo.
echo Step 1 of 3: downloading and checking the jar...

powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $ErrorActionPreference='Stop'; try { $h=@{'User-Agent'='SpatialHUD-Updater'}; $r=Invoke-RestMethod -Headers $h -Uri ('https://api.github.com/repos/%REPO%/releases/tags/%RELEASE_TAG%'); $j=@($r.assets | Where-Object { $_.name -like 'spatial-hud-*.jar' })[0]; if($j -eq $null){ throw 'that build has no Spatial HUD jar published yet' }; Invoke-WebRequest -UseBasicParsing -Headers $h -Uri $j.browser_download_url -OutFile '%JAR_TMP%'; $n=(Get-Item '%JAR_TMP%').Length; if($n -lt 1024 -or $n -ne [int64]$j.size){ throw 'the download was incomplete' }; Add-Type -AssemblyName System.IO.Compression.FileSystem; $z=[IO.Compression.ZipFile]::OpenRead('%JAR_TMP%'); try { $e=$z.GetEntry('fabric.mod.json'); if($e -eq $null){ throw 'the file is not a Fabric mod' }; $sr=New-Object IO.StreamReader($e.Open()); $txt=$sr.ReadToEnd(); $sr.Close() } finally { $z.Dispose() }; $meta=ConvertFrom-Json $txt; if($meta.id -ne 'spatialhud'){ throw 'the file is not the Spatial HUD mod' }; Write-Host ('  Checked: '+$n+' bytes, built '+([DateTimeOffset]::Parse([string]$j.updated_at)).ToString('u')) } catch { Write-Host ('  Download check failed: '+$_.Exception.Message); exit 1 }"
if errorlevel 1 goto :install_failed

if not exist "%MODS_DIR%" (
    echo.
    echo The mods folder does not exist yet: %MODS_DIR%
    call :choose_folder
    if not exist "%MODS_DIR%" goto :install_failed
)

echo.
echo Step 2 of 3: putting the jar in your mods folder...
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%" 2>nul
move /y "%MODS_DIR%\spatial-hud-*.jar" "%BACKUP_DIR%\" >nul 2>nul
copy /y "%JAR_TMP%" "%MODS_DIR%\%JAR_NAME%" >nul
if errorlevel 1 goto :install_restore
if not exist "%MODS_DIR%\%JAR_NAME%" goto :install_restore

echo.
echo Step 3 of 3: saving your choices...
if not exist "%STATE_DIR%" mkdir "%STATE_DIR%" 2>nul
>"%BUILD_STATE%" echo %BUILD%
>"%MODS_STATE%" echo %MODS_DIR%
del "%JAR_TMP%" 2>nul

echo.
echo Spatial HUD installed: %BUILD%
echo Your old jar, if there was one, is in: %BACKUP_DIR%
echo Start the game from Modrinth.
goto :finish

:install_restore
echo.
echo Could not copy the new jar into the mods folder. Putting the old one back.
move /y "%BACKUP_DIR%\spatial-hud-*.jar" "%MODS_DIR%\" >nul 2>nul
goto :install_failed

:install_failed
echo.
echo Spatial HUD was NOT changed. See the message above.
echo Check your Internet connection and try again.
del "%JAR_TMP%" 2>nul
rem Put the remembered choices back, in case this run changed them on screen.
call :load_state
goto :finish

:finish
echo.
pause
exit /b 0

:end
exit /b 0

rem ---------------------------------------------------------------------------
rem  Self-update. A newer copy of this file on the session branch takes over
rem  the run. It is run only if it is a unified installer with a higher
rem  UPDATER_VERSION, so a 404 page or an old file can never run.
rem ---------------------------------------------------------------------------
:self_update
if defined NO_SELF_UPDATE goto :eof
if defined SPATIALHUD_CHILD goto :eof
set "NEWBAT=%TEMP%\spatial-hud-installer-%RANDOM%.bat"
set "NEW_VER="
powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $ErrorActionPreference='Stop'; try { Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/%REPO%/%SELF_REF%/spatial-hud-template-26.3/Update-SpatialHUD.bat' -OutFile '%NEWBAT%' } catch { exit 1 }" >nul 2>nul
if errorlevel 1 goto :su_skip
findstr /b /c:"rem UPDATER_VERSION " "%NEWBAT%" >nul 2>nul
if errorlevel 1 goto :su_skip
for /f "usebackq tokens=3" %%V in (`findstr /b /c:"rem UPDATER_VERSION " "%NEWBAT%"`) do set "NEW_VER=%%V"
echo(%NEW_VER%| findstr /r /x "[0-9][0-9]*" >nul
if errorlevel 1 goto :su_skip
if %NEW_VER% LEQ %UPDATER_VERSION% goto :su_skip
echo A newer installer is available (version %NEW_VER%). Running it for this update.
echo.
set "SPATIALHUD_CHILD=1"
call "%NEWBAT%" %*
set "SELF_UPDATED=1"
del "%NEWBAT%" 2>nul
goto :eof
:su_skip
if exist "%NEWBAT%" del "%NEWBAT%" 2>nul
goto :eof
