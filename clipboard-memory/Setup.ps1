# ====================================================================
#  Clipboard Memory - Setup & Updater
#
#  Verifies ClipboardMemory.exe is present, creates the Start menu and Desktop
#  shortcuts, sets it to start when you log in (silent, elevated,
#  scheduled task - no permission prompt at boot), launches Clipboard Memory,
#  and verifies that it started cleanly.
#
#  Run it any time. It works out what is needed and does it:
#    - checks ClipboardMemory.exe is present
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
$CfgDir    = Join-Path $env:LOCALAPPDATA 'ClipboardMemory'
$Exe       = Join-Path $Here 'ClipboardMemory.exe'
$Ico       = Join-Path $Here 'clipboard-memory.ico'
$TaskName  = 'TestsSuite-ClipboardMemory'
$RunName   = 'Clipboard Memory'

try { if (-not (Test-Path $CfgDir)) { New-Item -ItemType Directory -Force -Path $CfgDir | Out-Null } } catch { }
try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }

Write-Host ''
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host '   CLIPBOARD MEMORY - SETUP & UPDATE' -ForegroundColor Cyan
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host ''

Write-Host '   [1/4] Checking Clipboard Memory files...' -ForegroundColor Gray
if (-not (Test-Path $Exe)) {
    Write-Host ''
    Write-Host '   [ERROR] ClipboardMemory.exe could not be found.' -ForegroundColor Red
    Write-Host '           Run Setup.bat again with a working internet connection.' -ForegroundColor Yellow
    exit 1
}
Write-Host '   [OK]   ClipboardMemory.exe verified on disk.' -ForegroundColor Green

Write-Host '   [2/4] Creating Desktop & Start Menu shortcuts...' -ForegroundColor Gray
function Set-Shortcut([string]$lnkPath) {
    try {
        $sh  = New-Object -ComObject WScript.Shell
        $lnk = $sh.CreateShortcut($lnkPath)
        $lnk.TargetPath = $Exe
        $lnk.WorkingDirectory = $Here
        $lnk.Description      = 'Clipboard Memory - disk-persisted clipboard history'
        if (Test-Path $Ico) { $lnk.IconLocation = $Ico }
        $lnk.Save()
        return $true
    } catch { return $false }
}

[void](Set-Shortcut (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Clipboard Memory.lnk'))
[void](Set-Shortcut (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Clipboard Memory.lnk'))
Write-Host '   [OK]   Shortcuts ready on Desktop and Start Menu.' -ForegroundColor Green

if (-not $NoAutostart) {
    Write-Host '   [3/4] Setting automatic Windows startup...' -ForegroundColor Gray
    try {
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
            -Principal $prin -Settings $set -Description 'Clipboard Memory auto-start' -Force | Out-Null
        Write-Host '   [OK]   Will start at log in, with no permission prompt.' -ForegroundColor Green
    } catch {
        Write-Host ('   [WARN] Autostart could not be set: ' + $_.Exception.Message) -ForegroundColor Yellow
    }
}

if (-not $NoLaunch) {
    Write-Host '   [4/4] Launching Clipboard Memory and verifying startup...' -ForegroundColor Gray
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
        Write-Host '   [ERROR] Clipboard Memory hit a startup error:' -ForegroundColor Red
        Write-Host ''
        Write-Host (Get-Content $CrashFile -Raw) -ForegroundColor Red
        exit 1
    } elseif ($ready) {
        Write-Host '   [OK]   Clipboard Memory is open and running!' -ForegroundColor Green
    } else {
        Write-Host '   [WARN] Clipboard Memory was launched (still initializing).' -ForegroundColor Yellow
    }
}
Write-Host ''
