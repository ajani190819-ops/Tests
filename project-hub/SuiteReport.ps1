# =====================================================================
#  Suite Report - one double-click, one file to send the agent.
#
#  The agent cannot see the machine: it cannot tell which build is on
#  disk, which is running, whether they match the released commit, or
#  what the lighting engine said last. This gathers all of that into
#  %LOCALAPPDATA%\TestsSuite\SuiteReport.txt and opens it. Nothing is
#  changed, nothing is uploaded - the file goes wherever you send it.
# =====================================================================
$ErrorActionPreference = 'Continue'
try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }

$la    = $env:LOCALAPPDATA
$suite = Join-Path $la 'Programs\TestsSuite'
if (-not (Test-Path (Join-Path $suite 'project-hub'))) {
    # Not an installed suite (the agent's sandbox, a raw clone): report on
    # the copy the script itself lives in.
    $suite = Split-Path -Parent $PSScriptRoot
}

$branch = 'main'
$ini = Join-Path $la 'ProjectHub\config.ini'
if (Test-Path $ini) {
    foreach ($l in Get-Content $ini -ErrorAction SilentlyContinue) {
        if ($l -match '^\s*Branch\s*=\s*(.+)$') { $branch = $Matches[1].Trim() }
    }
}

$root   = "https://raw.githubusercontent.com/ajani190819-ops/Tests/$branch"
$commit = $null
try {
    $t = (Invoke-WebRequest "$root/.agent/RELEASED" -UseBasicParsing -TimeoutSec 20).Content
    if ($t -match '(?m)^commit=([0-9a-f]{7,40})') { $commit = $Matches[1] }
} catch { }

$out = @()
$out += 'SUITE REPORT  ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
$out += 'os          : ' + [Environment]::OSVersion.VersionString
$out += 'suite dir   : ' + $suite
$out += 'hub branch  : ' + $branch
$out += 'released at : ' + $(if ($commit) { $commit } else { 'UNREADABLE - offline or branch predates the marker' })
$out += ''
$out += '--- on disk vs running vs released ---'

$apps = @(
    @{ rel = 'project-hub/ProjectHub.exe';             proc = 'ProjectHub' },
    @{ rel = 'keyboard-lighting/KeyboardLighting.exe'; proc = 'KeyboardLighting' },
    @{ rel = 'keyboard-lighting/Aura-Background.ps1';  proc = '' },
    @{ rel = 'keyboard-lighting/Tray.ps1';             proc = '' },
    @{ rel = 'bluetooth-guard/BluetoothGuard.exe';     proc = 'BluetoothGuard' },
    @{ rel = 'dns-pilot/DnsPilot.exe';                 proc = 'DnsPilot' },
    @{ rel = 'audio-switcher/AudioSwitcher.exe';       proc = 'AudioSwitcher' },
    @{ rel = 'agent-notes/AgentNotes.exe';             proc = 'AgentNotes' }
)
foreach ($a in $apps) {
    $lp = Join-Path $suite $a.rel
    $line = ('{0,-38} ' -f $a.rel)
    if (Test-Path $lp) {
        $li = Get-Item $lp
        $lh = (Get-FileHash $lp -Algorithm SHA256).Hash.Substring(0, 8)
        $line += ('disk {0} {1} ' -f $lh, $li.LastWriteTime.ToString('yyyy-MM-dd HH:mm'))
    } else {
        $line += 'disk MISSING '
    }
    if ($a.proc) {
        $rp = Get-Process -Name $a.proc -ErrorAction SilentlyContinue
        if ($rp) {
            foreach ($p in ($rp | Select-Object -First 2)) {
                try {
                    $line += ('| running pid {0} since {1} at {2} ' -f $p.Id, $p.StartTime.ToString('HH:mm:ss'), $p.Path)
                } catch { $line += ('| running pid {0} (details unavailable) ' -f $p.Id) }
            }
        } else {
            $line += '| not running '
        }
    }
    if ($commit -and (Test-Path $lp)) {
        try {
            $tmp = Join-Path $env:TEMP ('rep_' + [IO.Path]::GetFileName($a.rel))
            Invoke-WebRequest ("$root/" + $a.rel) -OutFile $tmp -UseBasicParsing -TimeoutSec 30
            $rh = (Get-FileHash $tmp -Algorithm SHA256).Hash.Substring(0, 8)
            Remove-Item $tmp -Force -ErrorAction SilentlyContinue
            $line += $(if ($rh -eq ((Get-FileHash $lp -Algorithm SHA256).Hash.Substring(0, 8))) { '| released MATCH' } else { '| released DIFFERS (' + $rh + ')' })
        } catch { $line += '| released UNCHECKED' }
    }
    $out += $line
}

$out += ''
$out += '--- keyboard lighting state ---'
$kdir = Join-Path $la 'KeyboardLighting'
$hm = Join-Path $kdir 'hold_mode.txt'
$out += 'hold mode   : ' + $(if (Test-Path $hm) { (Get-Content $hm -TotalCount 1) } else { 'unset (default tap)' })
$th = Join-Path $kdir 'theme.json'
if (Test-Path $th) {
    $j = Get-Content $th -Raw
    if ($j -match '"Smooth"\s*:\s*(true|false)') { $out += 'smooth      : ' + $Matches[1] }
}
$pi = Join-Path $kdir 'presets.ini'
if (Test-Path $pi) {
    foreach ($l in Get-Content $pi) {
        if ($l -match '^\s*(UpMod|UpKey|DownMod|DownKey)\s*=') { $out += ('hotkey      : ' + $l.Trim()) }
    }
}
$log = $null
foreach ($cand in @((Join-Path $kdir 'logs\engine.log'), (Join-Path $kdir 'engine.log'))) {
    if (Test-Path $cand) { $log = $cand; break }
}
if ($log) {
    $out += 'engine log  : ' + $log
    $tail = Get-Content $log -Tail 6
    foreach ($l in $tail) { $out += ('  | ' + $l) }
    $lockline = Select-String -Path $log -Pattern 'lock ' -SimpleMatch | Select-Object -Last 1
    if ($lockline) { $out += ('last lock   : ' + $lockline.Line) }
} else {
    $out += 'engine log  : NOT FOUND (engine never ran here?)'
}

$out += ''
$out += '--- rival lighting processes ---'
$rivals = @('ArmouryCrate', 'ArmouryCrate.Service', 'LightingService', 'ASUSLiveUpdate', 'ATKOSD2', 'GHelper', 'OpenRGB', 'SignalRGB')
$seen = $false
foreach ($r in $rivals) {
    $p = Get-Process -Name $r -ErrorAction SilentlyContinue
    if ($p) { $out += ('RUNNING     : ' + $r); $seen = $true }
}
if (-not $seen) { $out += 'none running' }

$dest = Join-Path $la 'TestsSuite\SuiteReport.txt'
try {
    if (-not (Test-Path (Split-Path -Parent $dest))) { New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null }
    Set-Content -Path $dest -Value $out -Encoding ASCII
    Write-Host ''
    Write-Host ('  Report written to ' + $dest) -ForegroundColor Green
    Write-Host '  Send that file to the agent.' -ForegroundColor Green
    Start-Process notepad.exe $dest
} catch {
    Write-Host ('  Could not write the report: ' + $_.Exception.Message) -ForegroundColor Red
    $out | ForEach-Object { Write-Host $_ }
    exit 1
}
