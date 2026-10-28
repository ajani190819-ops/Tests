# =====================================================================
#  Bluetooth Guard - Setup & Updater
#
#  Downloads any newer files from GitHub (trying main first and falling
#  back to arena/e895b94f-tests until merged, or keeping local copies if
#  offline), builds BluetoothCore.dll + BluetoothGuard.exe, enables
#  Windows startup protection, launches Bluetooth Guard, and verifies
#  that it started cleanly.
# =====================================================================

param(
    [switch]$NoLaunch,
    [switch]$NoAutostart,
    [switch]$QuietConsole
)

$ErrorActionPreference = 'Continue'
$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path
$CfgDir    = Join-Path $env:LOCALAPPDATA 'BluetoothGuard'
$ReadyFile = Join-Path $CfgDir 'window.ready'
$CrashFile = Join-Path $CfgDir 'crash.log'
$Exe       = Join-Path $Here 'BluetoothGuard.exe'
$Dll       = Join-Path $Here 'BluetoothCore.dll'
$Script    = Join-Path $Here 'BluetoothGuard.ps1'
$CoreCs    = Join-Path $Here 'BluetoothCore.cs'
$TaskName  = 'BluetoothGuard'
$Files     = @('BluetoothGuard.ps1', 'BluetoothCore.cs', 'Setup.ps1', 'Setup.bat', 'README.md')
$RepoSlug  = 'ajani190819-ops/Tests'
$Fallback  = 'arena/e895b94f-tests'

Write-Host ''
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host '   BLUETOOTH GUARD - SETUP & UPDATE' -ForegroundColor Cyan
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host ''

try { if (-not (Test-Path $CfgDir)) { New-Item -ItemType Directory -Force -Path $CfgDir | Out-Null } } catch { }
try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }

$Base = ''
$ChosenBranch = ''
if ($env:BLUETOOTH_GUARD_BASE) {
    $Base = $env:BLUETOOTH_GUARD_BASE
    $ChosenBranch = 'custom'
} else {
    foreach ($br in @('main', $Fallback)) {
        $probe = "https://raw.githubusercontent.com/$RepoSlug/$br/bluetooth-guard/BluetoothGuard.ps1"
        try {
            $resp = Invoke-WebRequest -Uri $probe -Method Head -UseBasicParsing -TimeoutSec 12 -ErrorAction Stop
            if ($resp.StatusCode -eq 200) {
                $Base = "https://raw.githubusercontent.com/$RepoSlug/$br/bluetooth-guard"
                $ChosenBranch = $br
                break
            }
        } catch { }
    }
}

Write-Host '   [1/4] Checking Bluetooth Guard files...' -ForegroundColor Gray
$updated = 0
if ($Base) {
    foreach ($f in $Files) {
        $dest = Join-Path $Here $f
        $tmp  = Join-Path $env:TEMP ('btg_' + $f)
        try {
            Invoke-WebRequest -Uri "$Base/$f" -OutFile $tmp -UseBasicParsing -TimeoutSec 20 -ErrorAction Stop
            $head = ''
            try { $head = [string](Get-Content $tmp -TotalCount 3 -ErrorAction SilentlyContinue) } catch { }
            if ((Get-Item $tmp).Length -lt 16 -or $head -match '(?i)^\s*<(?:!doctype|html)') {
                Remove-Item $tmp -Force -ErrorAction SilentlyContinue
                continue
            }
            $nh = (Get-FileHash $tmp -Algorithm SHA256).Hash
            $oh = if (Test-Path $dest) { (Get-FileHash $dest -Algorithm SHA256).Hash } else { '' }
            if ($nh -ne $oh) {
                Copy-Item -LiteralPath $tmp -Destination $dest -Force
                Unblock-File $dest -ErrorAction SilentlyContinue
                $updated++
            }
            Remove-Item $tmp -Force -ErrorAction SilentlyContinue
        } catch { }
    }
}

if (-not (Test-Path $Script) -or -not (Test-Path $CoreCs)) {
    Write-Host ''
    Write-Host '   [ERROR] Required files (BluetoothGuard.ps1 / BluetoothCore.cs) could not be found or downloaded.' -ForegroundColor Red
    Write-Host '           Check your internet connection and run Setup.bat again.' -ForegroundColor Yellow
    exit 1
}

if ($updated -gt 0) {
    Write-Host ("   [OK]   Downloaded {0} file(s) from GitHub ({1})." -f $updated, $ChosenBranch) -ForegroundColor Green
} else {
    Write-Host '   [OK]   All Bluetooth Guard files verified on disk.' -ForegroundColor Green
}

try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Script -Stop
    Start-Sleep -Milliseconds 300
} catch { }

Write-Host '   [2/4] Building fast native helpers (BluetoothCore.dll + BluetoothGuard.exe)...' -ForegroundColor Gray
$csc = $null
foreach ($cand in @(
    "$env:WinDir\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
    "$env:WinDir\Microsoft.NET\Framework\v4.0.30319\csc.exe")) {
    if (Test-Path $cand) { $csc = $cand; break }
}

if ($csc) {
    $dllErr = Join-Path $env:TEMP 'btg_dll_err.txt'
    $dllOut = Join-Path $env:TEMP 'btg_dll_out.txt'
    $dllArgs = @(
        '/nologo', '/target:library', '/optimize+',
        '/reference:System.dll', '/reference:System.Drawing.dll', '/reference:System.Windows.Forms.dll',
        ('/out:"{0}"' -f $Dll), ('"{0}"' -f $CoreCs)
    )
    $p1 = Start-Process -FilePath $csc -ArgumentList $dllArgs -NoNewWindow -Wait -PassThru `
          -RedirectStandardOutput $dllOut -RedirectStandardError $dllErr
    if ($p1.ExitCode -eq 0 -and (Test-Path $Dll)) {
        (Get-Item $Dll).LastWriteTime = (Get-Date).AddSeconds(3)
        Write-Host '   [OK]   Compiled BluetoothCore.dll.' -ForegroundColor Green
    } else {
        $msg = (Get-Content $dllOut, $dllErr -ErrorAction SilentlyContinue | Out-String).Trim()
        Write-Host ('   [WARN] BluetoothCore.dll compile note: {0}' -f $msg) -ForegroundColor Yellow
    }

    $launcherCs = Join-Path $env:TEMP 'btg_launcher.cs'
    $code = @'
using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

class Launcher {
    [STAThread]
    static int Main(string[] args) {
        string dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        string script = Path.Combine(dir, "BluetoothGuard.ps1");
        if (!File.Exists(script)) {
            System.Windows.Forms.MessageBox.Show(
                "BluetoothGuard.ps1 is missing from:\n" + dir + "\n\nRun Setup.bat to repair.",
                "Bluetooth Guard");
            return 1;
        }
        string extra = "";
        for (int i = 0; i < args.Length; i++) extra += " " + args[i];
        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = "powershell.exe";
        psi.Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + script + "\"" + extra;
        psi.WorkingDirectory = dir;
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        psi.WindowStyle = ProcessWindowStyle.Hidden;
        try { Process.Start(psi); }
        catch (Exception ex) {
            System.Windows.Forms.MessageBox.Show("Could not start:\n" + ex.Message, "Bluetooth Guard");
            return 2;
        }
        return 0;
    }
}
'@
    Set-Content -Path $launcherCs -Value $code -Encoding UTF8
    $exeErr = Join-Path $env:TEMP 'btg_exe_err.txt'
    $exeOut = Join-Path $env:TEMP 'btg_exe_out.txt'
    $exeArgs = @(
        '/nologo', '/target:winexe', '/optimize+',
        '/reference:System.dll', '/reference:System.Windows.Forms.dll',
        ('/out:"{0}"' -f $Exe), ('"{0}"' -f $launcherCs)
    )
    $p2 = Start-Process -FilePath $csc -ArgumentList $exeArgs -NoNewWindow -Wait -PassThru `
          -RedirectStandardOutput $exeOut -RedirectStandardError $exeErr
    Remove-Item $launcherCs -Force -ErrorAction SilentlyContinue
    if ($p2.ExitCode -eq 0 -and (Test-Path $Exe)) {
        Write-Host '   [OK]   Built BluetoothGuard.exe.' -ForegroundColor Green
    } else {
        $msg2 = (Get-Content $exeOut, $exeErr -ErrorAction SilentlyContinue | Out-String).Trim()
        Write-Host ('   [WARN] BluetoothGuard.exe compile note: {0}' -f $msg2) -ForegroundColor Yellow
    }
}

Write-Host '   [3/4] Creating Desktop & Start Menu shortcuts...' -ForegroundColor Gray
function Set-Shortcut([string]$lnkPath) {
    try {
        $sh  = New-Object -ComObject WScript.Shell
        $lnk = $sh.CreateShortcut($lnkPath)
        if (Test-Path $Exe) {
            $lnk.TargetPath = $Exe
        } else {
            $lnk.TargetPath = 'powershell.exe'
            $lnk.Arguments  = ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}"' -f $Script)
        }
        $lnk.WorkingDirectory = $Here
        $lnk.Description      = 'Bluetooth Guard - Stop Windows stealing audio from your phone'
        $lnk.Save()
        return $true
    } catch { return $false }
}

[void](Set-Shortcut (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Bluetooth Guard.lnk'))
[void](Set-Shortcut (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Bluetooth Guard.lnk'))
Write-Host '   [OK]   Shortcuts ready on Desktop and Start Menu.' -ForegroundColor Green

if (-not $NoAutostart) {
    try {
        # Migrate away from the old HKCU Run entry: a Run-key launch trips
        # a UAC prompt at every boot now that the exe requires administrator.
        # Use a silent, elevated scheduled task instead - no prompt, high
        # priority, starts on battery, zero post-boot delay.
        Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name 'BluetoothGuard' -ErrorAction SilentlyContinue

        $usePs = -not (Test-Path $Exe)
        if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
            Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
        }
        if ($usePs) {
            $act = New-ScheduledTaskAction -Execute 'powershell.exe' `
                   -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}" --silent' -f $Script) `
                   -WorkingDirectory $Here
        } else {
            $act = New-ScheduledTaskAction -Execute $Exe -Argument '--silent' -WorkingDirectory $Here
        }
        $trg  = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
        $prin = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest
        $set  = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
                -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable
        Register-ScheduledTask -TaskName $TaskName -Action $act -Trigger $trg `
            -Principal $prin -Settings $set -Description 'Bluetooth Guard' -Force | Out-Null
        Write-Host '   [OK]   Automatic Windows startup enabled (elevated, no prompt).' -ForegroundColor Green
    } catch {
        Write-Host ('   [WARN] Autostart could not be set: ' + $_.Exception.Message) -ForegroundColor Yellow
    }
}

if (-not $NoLaunch) {
    Write-Host '   [4/4] Launching Bluetooth Guard and verifying startup...' -ForegroundColor Gray
    foreach ($f in @($ReadyFile, $CrashFile)) {
        try { Remove-Item $f -Force -ErrorAction SilentlyContinue } catch { }
    }

    if (Test-Path $Exe) {
        Start-Process -FilePath $Exe -WorkingDirectory $Here
    } else {
        Start-Process -FilePath 'powershell.exe' -WorkingDirectory $Here -ArgumentList @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', ('"{0}"' -f $Script)
        )
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    $ready = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 150
        if (Test-Path $CrashFile) { break }
        if (Test-Path $ReadyFile) { $ready = $true; break }
    }

    if (Test-Path $CrashFile) {
        Write-Host ''
        Write-Host '   [ERROR] Bluetooth Guard hit a startup error:' -ForegroundColor Red
        Write-Host ''
        Write-Host (Get-Content $CrashFile -Raw) -ForegroundColor Red
        exit 1
    } elseif ($ready) {
        Write-Host '   [OK]   Bluetooth Guard is open and running!' -ForegroundColor Green
    } else {
        Write-Host '   [WARN] Bluetooth Guard was launched (still initializing).' -ForegroundColor Yellow
    }
}
Write-Host ''
