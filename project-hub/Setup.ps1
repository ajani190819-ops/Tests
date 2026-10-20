# =====================================================================
#  Project Hub - Setup & Updater
#
#  Downloads any newer Hub files from GitHub (or uses the local copy if
#  offline), builds ProjectHub.exe + HubCore.dll for instant startup,
#  creates Desktop & Start Menu shortcuts, and opens Project Hub.
# =====================================================================

param(
    [switch]$NoLaunch
)

$ErrorActionPreference = 'Continue'
$Here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$Branch = if ($env:PROJECT_HUB_BRANCH) { $env:PROJECT_HUB_BRANCH } else { 'main' }
$Base   = "https://raw.githubusercontent.com/ajani190819-ops/Tests/$Branch/project-hub"
$Exe    = Join-Path $Here 'ProjectHub.exe'
$Dll    = Join-Path $Here 'HubCore.dll'
$Script = Join-Path $Here 'ProjectHub.ps1'
$CoreCs = Join-Path $Here 'HubCore.cs'
$Files  = @('ProjectHub.ps1', 'HubCore.cs', 'Setup.ps1', 'Setup.bat', 'README.md')

Write-Host ''
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host '   PROJECT HUB - SETUP & UPDATE' -ForegroundColor Cyan
Write-Host '  ===============================================================' -ForegroundColor Cyan
Write-Host ''

try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }
$updated = 0
foreach ($f in $Files) {
    $dest = Join-Path $Here $f
    $tmp  = Join-Path $env:TEMP ('phub_' + $f)
    try {
        Invoke-WebRequest "$Base/$f" -OutFile $tmp -UseBasicParsing -TimeoutSec 20
        $head = ''
        try { $head = [string](Get-Content $tmp -TotalCount 3 -ErrorAction SilentlyContinue) } catch { }
        if ((Get-Item $tmp).Length -lt 16 -or $head -match '(?i)^\s*<(?:!doctype|html)') {
            Remove-Item $tmp -Force -ErrorAction SilentlyContinue
            continue
        }
        $nh = (Get-FileHash $tmp -Algorithm SHA256).Hash
        $oh = if (Test-Path $dest) { (Get-FileHash $dest -Algorithm SHA256).Hash } else { '' }
        if ($nh -ne $oh) {
            Copy-Item $tmp $dest -Force
            Unblock-File $dest -ErrorAction SilentlyContinue
            $updated++
        }
        Remove-Item $tmp -Force -ErrorAction SilentlyContinue
    } catch { }
}
if ($updated -gt 0) {
    Write-Host ("   [OK]   Updated {0} file(s) from GitHub." -f $updated) -ForegroundColor Green
} else {
    Write-Host '   [OK]   Files ready.' -ForegroundColor Green
}

if (Test-Path $Script) {
    try {
        powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Script -Stop
        Start-Sleep -Milliseconds 300
    } catch { }
}

$csc = $null
foreach ($cand in @(
    "$env:WinDir\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
    "$env:WinDir\Microsoft.NET\Framework\v4.0.30319\csc.exe")) {
    if (Test-Path $cand) { $csc = $cand; break }
}

if ($csc -and (Test-Path $CoreCs)) {
    $dllArgs = @(
        '/nologo', '/target:library', '/optimize+',
        '/reference:System.dll', '/reference:System.Drawing.dll', '/reference:System.Windows.Forms.dll',
        ('/out:"{0}"' -f $Dll), ('"{0}"' -f $CoreCs)
    )
    $p1 = Start-Process -FilePath $csc -ArgumentList $dllArgs -NoNewWindow -Wait -PassThru
    if ($p1.ExitCode -eq 0 -and (Test-Path $Dll)) {
        (Get-Item $Dll).LastWriteTime = (Get-Date).AddSeconds(3)
        Write-Host '   [OK]   Compiled HubCore.dll for instant startup.' -ForegroundColor Green
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
    $exeArgs = @(
        '/nologo', '/target:winexe', '/optimize+',
        '/reference:System.dll', '/reference:System.Windows.Forms.dll',
        ('/out:"{0}"' -f $Exe), ('"{0}"' -f $launcherCs)
    )
    $p2 = Start-Process -FilePath $csc -ArgumentList $exeArgs -NoNewWindow -Wait -PassThru
    Remove-Item $launcherCs -Force -ErrorAction SilentlyContinue
    if ($p2.ExitCode -eq 0 -and (Test-Path $Exe)) {
        Write-Host '   [OK]   Built ProjectHub.exe.' -ForegroundColor Green
    }
}

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
Write-Host '   [OK]   Desktop & Start Menu shortcuts created.' -ForegroundColor Green

if (-not $NoLaunch) {
    if (Test-Path $Exe) {
        Start-Process -FilePath $Exe -WorkingDirectory $Here
    } else {
        Start-Process -FilePath 'powershell.exe' -WorkingDirectory $Here -ArgumentList @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', ('"{0}"' -f $Script)
        )
    }
    Write-Host '   [OK]   Project Hub launched.' -ForegroundColor Green
}
Write-Host ''
