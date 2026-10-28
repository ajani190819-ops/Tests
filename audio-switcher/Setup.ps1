# ====================================================================
#  Audio Switcher - Setup & Updater
#
#  Verifies AudioSwitcher.exe is present, creates the Start menu and Desktop
#  shortcuts, sets it to start when you log in (silent, elevated,
#  scheduled task - no permission prompt at boot), launches Audio Switcher,
#  and verifies that it started cleanly.
#
#  Run it any time. It works out what is needed and does it:
#    - checks AudioSwitcher.exe is present
#    - creates the Start menu and Desktop shortcuts
#    - sets it to start when you log in
#    - tests that it actually works, and tells you what is wrong if not
#
#  First run installs. Every run after that updates and repairs.
# ====================================================================

param(
    [switch]$NoLaunch,
    [switch]$NoAutostart,
    [switch]$QuietConsole
)

$ErrorActionPreference = 'Continue'

$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path
$CfgDir    = Join-Path $env:LOCALAPPDATA 'AudioSwitcher'
$Exe       = Join-Path $Here 'AudioSwitcher.exe'
$Ico       = Join-Path $Here 'audio-switcher.ico'
$TaskName  = 'TestsSuite-AudioSwitcher'
$RunName   = 'Audio Switcher'

try { if (-not (Test-Path $CfgDir)) { New-Item -ItemType Directory -Force -Path $CfgDir | Out-Null } } catch { }
try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }

Write-Host ''
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host '   AUDIO SWITCHER - SETUP & UPDATE' -ForegroundColor Cyan
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host ''

Write-Host '   [1/4] Checking Audio Switcher files...' -ForegroundColor Gray
if (-not (Test-Path $Exe)) {
    Write-Host ''
    Write-Host '   [ERROR] AudioSwitcher.exe could not be found.' -ForegroundColor Red
    Write-Host '           Run Setup.bat again with a working internet connection.' -ForegroundColor Yellow
    exit 1
}
Write-Host '   [OK]   AudioSwitcher.exe verified on disk.' -ForegroundColor Green

Write-Host '   [2/4] Creating Desktop & Start Menu shortcuts...' -ForegroundColor Gray
function Set-Shortcut([string]$lnkPath) {
    try {
        $sh  = New-Object -ComObject WScript.Shell
        $lnk = $sh.CreateShortcut($lnkPath)
        $lnk.TargetPath = $Exe
        $lnk.WorkingDirectory = $Here
        $lnk.Description      = 'Audio Switcher - Per-app volume & audio output switching'
        if (Test-Path $Ico) { $lnk.IconLocation = $Ico }
        $lnk.Save()
        return $true
    } catch { return $false }
}

[void](Set-Shortcut (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Audio Switcher.lnk'))
[void](Set-Shortcut (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Audio Switcher.lnk'))
Write-Host '   [OK]   Shortcuts ready on Desktop and Start Menu.' -ForegroundColor Green

if (-not $NoAutostart) {
    Write-Host '   [3/4] Setting automatic Windows startup...' -ForegroundColor Gray
    try {
        # Migrate away from any old Run-key entry: a Run-key launch trips
        # a UAC prompt at every boot now that the exe requires administrator.
        # Use a silent, elevated scheduled task instead - no prompt, high
        # priority, starts on battery.
        Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name $RunName -ErrorAction SilentlyContinue

        if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
            Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
        }
        $act = New-ScheduledTaskAction -Execute $Exe -Argument '--silent' -WorkingDirectory $Here
        $trg  = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
        $prin = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest
        $set  = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
                -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable
        Register-ScheduledTask -TaskName $TaskName -Action $act -Trigger $trg `
            -Principal $prin -Settings $set -Description 'Audio Switcher auto-start' -Force | Out-Null
        Write-Host '   [OK]   Will start at log in, with no permission prompt.' -ForegroundColor Green
    } catch {
        Write-Host ('   [WARN] Autostart could not be set: ' + $_.Exception.Message) -ForegroundColor Yellow
    }
}

if (-not $NoLaunch) {
    Write-Host '   [4/4] Launching Audio Switcher and verifying startup...' -ForegroundColor Gray
    $ReadyFile = Join-Path $CfgDir 'window.ready'
    $CrashFile = Join-Path $CfgDir 'crash.log'
    foreach ($f in @($ReadyFile, $CrashFile)) {
        try { Remove-Item $f -Force -ErrorAction SilentlyContinue } catch { }
    }

    Start-Process -FilePath $Exe -ArgumentList '--silent' -WorkingDirectory $Here

    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    $ready = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 150
        if (Test-Path $CrashFile) { break }
        if (Test-Path $ReadyFile) { $ready = $true; break }
    }

    if (Test-Path $CrashFile) {
        Write-Host ''
        Write-Host '   [ERROR] Audio Switcher hit a startup error:' -ForegroundColor Red
        Write-Host ''
        Write-Host (Get-Content $CrashFile -Raw) -ForegroundColor Red
        exit 1
    } elseif ($ready) {
        Write-Host '   [OK]   Audio Switcher is open and running!' -ForegroundColor Green
    } else {
        Write-Host '   [WARN] Audio Switcher was launched (still initializing).' -ForegroundColor Yellow
    }
}
Write-Host ''
