# =====================================================================
#  Project Hub - Setup & Updater
#
#  Downloads any newer Hub files from GitHub (automatically falling back
#  from main to the active PR branch if not merged yet, or using local
#  files if offline), compiles HubCore.dll + ProjectHub.exe, creates
#  Desktop & Start Menu shortcuts, launches Project Hub, and verifies
#  that the window came up cleanly.
# =====================================================================

param(
    [switch]$NoLaunch,
    [switch]$QuietConsole
)

$ErrorActionPreference = 'Continue'
$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path
$CfgDir    = Join-Path $env:LOCALAPPDATA 'ProjectHub'
$ReadyFile = Join-Path $CfgDir 'window.ready'
$CrashFile = Join-Path $CfgDir 'crash.log'
$Exe       = Join-Path $Here 'ProjectHub.exe'
$Dll       = Join-Path $Here 'HubCore.dll'
$Script    = Join-Path $Here 'ProjectHub.ps1'
$CoreCs    = Join-Path $Here 'HubCore.cs'
$Files     = @('ProjectHub.ps1', 'HubCore.cs', 'Setup.ps1', 'Setup.bat', 'README.md')
$RepoSlug  = 'ajani190819-ops/Tests'
$Fallback  = 'arena/e895b94f-tests'

Write-Host ''
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host '   PROJECT HUB - SETUP & UPDATE' -ForegroundColor Cyan
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host ''

try { if (-not (Test-Path $CfgDir)) { New-Item -ItemType Directory -Force -Path $CfgDir | Out-Null } } catch { }
try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }

# Pick 'main' once merged, or seamlessly fall back to the active branch.
$Candidates = @()
if ($env:PROJECT_HUB_BRANCH) { $Candidates += $env:PROJECT_HUB_BRANCH }
$Candidates += 'main'
$Candidates += $Fallback

$Base = ''
$ChosenBranch = ''
foreach ($br in $Candidates) {
    $probe = "https://raw.githubusercontent.com/$RepoSlug/$br/project-hub/ProjectHub.ps1"
    try {
        $resp = Invoke-WebRequest -Uri $probe -Method Head -UseBasicParsing -TimeoutSec 12 -ErrorAction Stop
        if ($resp.StatusCode -eq 200) {
            $Base = "https://raw.githubusercontent.com/$RepoSlug/$br/project-hub"
            $ChosenBranch = $br
            break
        }
    } catch { }
}

Write-Host '   [1/4] Checking Project Hub files...' -ForegroundColor Gray
$updated = 0
if ($Base) {
    foreach ($f in $Files) {
        $dest = Join-Path $Here $f
        $tmp  = Join-Path $env:TEMP ('phub_' + $f)
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
    Write-Host '   [ERROR] Required files (ProjectHub.ps1 / HubCore.cs) could not be found or downloaded.' -ForegroundColor Red
    Write-Host '           Check your internet connection and run Setup.bat again.' -ForegroundColor Yellow
    exit 1
}

if ($updated -gt 0) {
    Write-Host ("   [OK]   Downloaded {0} file(s) from GitHub ({1})." -f $updated, $ChosenBranch) -ForegroundColor Green
} else {
    Write-Host '   [OK]   All Project Hub files verified on disk.' -ForegroundColor Green
}

# Stop any running instance cleanly before recompiling DLL/EXE.
try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Script -Stop
    Start-Sleep -Milliseconds 300
} catch { }

Write-Host '   [2/4] Building fast native helpers (HubCore.dll + ProjectHub.exe)...' -ForegroundColor Gray
$csc = $null
foreach ($cand in @(
    "$env:WinDir\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
    "$env:WinDir\Microsoft.NET\Framework\v4.0.30319\csc.exe")) {
    if (Test-Path $cand) { $csc = $cand; break }
}

if ($csc) {
    $dllErr = Join-Path $env:TEMP 'phub_dll_err.txt'
    $dllOut = Join-Path $env:TEMP 'phub_dll_out.txt'
    $dllArgs = @(
        '/nologo', '/target:library', '/optimize+',
        '/reference:System.dll', '/reference:System.Drawing.dll', '/reference:System.Windows.Forms.dll',
        ('/out:"{0}"' -f $Dll), ('"{0}"' -f $CoreCs)
    )
    $p1 = Start-Process -FilePath $csc -ArgumentList $dllArgs -NoNewWindow -Wait -PassThru `
          -RedirectStandardOutput $dllOut -RedirectStandardError $dllErr
    if ($p1.ExitCode -eq 0 -and (Test-Path $Dll)) {
        (Get-Item $Dll).LastWriteTime = (Get-Date).AddSeconds(3)
        Write-Host '   [OK]   Compiled HubCore.dll.' -ForegroundColor Green
    } else {
        $msg = (Get-Content $dllOut, $dllErr -ErrorAction SilentlyContinue | Out-String).Trim()
        Write-Host ('   [WARN] HubCore.dll compile note: {0}' -f $msg) -ForegroundColor Yellow
    }

    $launcherCs = Join-Path $env:TEMP 'phub_launcher.cs'
    $code = @'
using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

class Launcher {
    [STAThread]
    static int Main(string[] args) {
        string dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        string script = Path.Combine(dir, "ProjectHub.ps1");
        if (!File.Exists(script)) {
            System.Windows.Forms.MessageBox.Show(
                "ProjectHub.ps1 is missing from:\n" + dir + "\n\nRun Setup.bat to repair.",
                "Project Hub");
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
            System.Windows.Forms.MessageBox.Show("Could not start:\n" + ex.Message, "Project Hub");
            return 2;
        }
        return 0;
    }
}
'@
    Set-Content -Path $launcherCs -Value $code -Encoding UTF8
    $exeErr = Join-Path $env:TEMP 'phub_exe_err.txt'
    $exeOut = Join-Path $env:TEMP 'phub_exe_out.txt'
    $exeArgs = @(
        '/nologo', '/target:winexe', '/optimize+',
        '/reference:System.dll', '/reference:System.Windows.Forms.dll',
        ('/out:"{0}"' -f $Exe), ('"{0}"' -f $launcherCs)
    )
    $p2 = Start-Process -FilePath $csc -ArgumentList $exeArgs -NoNewWindow -Wait -PassThru `
          -RedirectStandardOutput $exeOut -RedirectStandardError $exeErr
    Remove-Item $launcherCs -Force -ErrorAction SilentlyContinue
    if ($p2.ExitCode -eq 0 -and (Test-Path $Exe)) {
        Write-Host '   [OK]   Built ProjectHub.exe.' -ForegroundColor Green
    } else {
        $msg2 = (Get-Content $exeOut, $exeErr -ErrorAction SilentlyContinue | Out-String).Trim()
        Write-Host ('   [WARN] ProjectHub.exe compile note: {0}' -f $msg2) -ForegroundColor Yellow
    }
}

Write-Host '   [3/4] Creating Desktop & Start Menu shortcuts...' -ForegroundColor Gray
function Set-HubShortcut([string]$lnkPath) {
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
        $lnk.Description      = 'Project Hub - Status & Updates for all Tests projects'
        $lnk.Save()
        return $true
    } catch { return $false }
}

[void](Set-HubShortcut (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Project Hub.lnk'))
[void](Set-HubShortcut (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Project Hub.lnk'))
Write-Host '   [OK]   Shortcuts ready on Desktop and Start Menu.' -ForegroundColor Green

if (-not $NoLaunch) {
    Write-Host '   [4/4] Launching Project Hub and verifying window startup...' -ForegroundColor Gray
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
        Write-Host '   [ERROR] Project Hub hit a startup error:' -ForegroundColor Red
        Write-Host ''
        Write-Host (Get-Content $CrashFile -Raw) -ForegroundColor Red
        exit 1
    } elseif ($ready) {
        Write-Host '   [OK]   Project Hub is open and running!' -ForegroundColor Green
    } else {
        Write-Host '   [WARN] Project Hub was launched (still initializing).' -ForegroundColor Yellow
    }
}
Write-Host ''
