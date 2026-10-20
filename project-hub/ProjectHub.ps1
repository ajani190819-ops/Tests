# =====================================================================
#  Project Hub
#
#  Clean, fast Windows dashboard for checking live status, launching,
#  restarting, and downloading updates across every project in this
#  repository:
#    1. Keyboard Lighting  (ROG Strix G16 RGB + tray recovery)
#    2. Bluetooth Guard    (stops Windows stealing phone earbuds)
#    3. Spatial HUD        (Minecraft 26.3 Modrinth mod updater)
#    4. OrcaSlicer Plugins (Wave Overhangs + Unlayered Infill)
#    5. Arena Link         (local repo bridge desktop app)
# =====================================================================

param(
    [switch]$Silent,
    [switch]$Stop
)

$ErrorActionPreference = 'Stop'

$Here        = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoGuess   = Split-Path -Parent $Here
$CfgDir      = Join-Path $env:LOCALAPPDATA 'ProjectHub'
$CfgFile     = Join-Path $CfgDir 'settings.json'
$LockFile    = Join-Path $CfgDir 'running.lock'
$ShowFile    = Join-Path $CfgDir 'show.flag'
$ReadyFile   = Join-Path $CfgDir 'window.ready'
$CrashFile   = Join-Path $CfgDir 'crash.log'
$ProgramsDir = Join-Path $env:LOCALAPPDATA 'Programs\TestsSuite'
$RepoOwner   = 'ajani190819-ops/Tests'
$FallbackBr  = 'arena/e895b94f-tests'

foreach ($d in @($CfgDir, $ProgramsDir)) {
    try { if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force -Path $d | Out-Null } } catch { }
}

function Read-PidFile([string]$path) {
    if (-not (Test-Path $path)) { return 0 }
    try {
        $raw = Get-Content $path -Raw -ErrorAction SilentlyContinue
        if (-not $raw) { return 0 }
        $id = 0
        if ([int]::TryParse($raw.Trim(), [ref]$id)) { return $id }
    } catch { }
    return 0
}

function Test-LockAlive([string]$path, [double]$maxAgeSec = 10.0) {
    if (-not (Test-Path $path)) { return 'stopped' }
    try {
        $id  = Read-PidFile $path
        $age = ([DateTime]::UtcNow - (Get-Item $path).LastWriteTimeUtc).TotalSeconds
        if ($id -le 0) { return 'stopped' }
        $p = $null
        try { $p = [System.Diagnostics.Process]::GetProcessById($id) } catch { return 'stopped' }
        if (-not $p -or $p.HasExited) { return 'stopped' }
        if ($age -ge $maxAgeSec) { return 'hung' }
        return 'running'
    } catch { return 'stopped' }
}

if ($Stop) {
    $id = Read-PidFile $LockFile
    if ($id -gt 0 -and $id -ne $PID) {
        try {
            $p = [System.Diagnostics.Process]::GetProcessById($id)
            if ($p -and -not $p.HasExited) { $p.Kill() }
        } catch { }
    }
    foreach ($f in @($LockFile, $ShowFile, $ReadyFile)) {
        try { Remove-Item $f -Force -ErrorAction SilentlyContinue } catch { }
    }
    return
}

try {

if ((Test-LockAlive $LockFile 10.0) -eq 'running') {
    try { Set-Content -Path $ShowFile -Value ([DateTime]::UtcNow.Ticks) -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
    $dl = [DateTime]::UtcNow.AddMilliseconds(1400)
    $ok = $false
    while ([DateTime]::UtcNow -lt $dl) {
        Start-Sleep -Milliseconds 75
        if (-not (Test-Path $ShowFile)) { $ok = $true; break }
    }
    if ($ok) {
        try { Set-Content -Path $ReadyFile -Value 'reused' -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
        return
    }
}
try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }

function Update-Heartbeat {
    try { Set-Content -Path $LockFile -Value $PID -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
}
Update-Heartbeat

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
try { Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue } catch { }
try {
    [System.Windows.Forms.Application]::SetUnhandledExceptionMode(
        [System.Windows.Forms.UnhandledExceptionMode]::CatchException)
} catch { }
[System.Windows.Forms.Application]::EnableVisualStyles()

$coreDll  = Join-Path $Here 'HubCore.dll'
$coreFile = Join-Path $Here 'HubCore.cs'
$loaded   = $false
if (Test-Path $coreDll) {
    try {
        $srcF = Get-Item $coreFile -ErrorAction SilentlyContinue
        $dllF = Get-Item $coreDll
        if (-not $srcF -or $dllF.LastWriteTime -ge $srcF.LastWriteTime) {
            Add-Type -Path $coreDll -ErrorAction Stop
            $loaded = $true
        }
    } catch { }
}
if (-not $loaded) {
    Add-Type -TypeDefinition (Get-Content $coreFile -Raw) `
        -ReferencedAssemblies 'System.Windows.Forms','System.Drawing','System' -ErrorAction Stop
}

try { [HubUi.Dpi]::Enable() } catch { }
try { [HubUi.Win]::SetAppId('Tests.ProjectHub') } catch { }

# ---------------------------------------------------------------- smart folder resolution
function Resolve-DefaultFolder([string]$repoSub, [string]$markerFile, [string]$managedName) {
    $sibling = Join-Path $RepoGuess $repoSub
    if (Test-Path (Join-Path $sibling $markerFile)) { return $sibling }
    $managed = Join-Path $ProgramsDir $managedName
    if (Test-Path (Join-Path $managed $markerFile)) { return $managed }
    if ($managedName -eq 'KeyboardLighting') {
        try {
            $t = Get-ScheduledTask -TaskName 'KeyboardLighting' -ErrorAction SilentlyContinue
            if ($t -and $t.Actions -and $t.Actions[0].WorkingDirectory) {
                $wd = $t.Actions[0].WorkingDirectory
                if (Test-Path (Join-Path $wd $markerFile)) { return $wd }
            }
        } catch { }
    }
    return $managed
}

function Resolve-DefaultModsDir {
    $remembered = Join-Path $env:LOCALAPPDATA 'SpatialHudUpdater\target-directory.txt'
    if (Test-Path $remembered) {
        try {
            $v = (Get-Content $remembered -Raw -ErrorAction SilentlyContinue).Trim()
            if ($v) { return $v }
        } catch { }
    }
    return (Join-Path $env:APPDATA 'ModrinthApp\profiles\F5W\mods')
}

$PrefBranch = if ($env:PROJECT_HUB_BRANCH) { $env:PROJECT_HUB_BRANCH } else { '' }

$script:Cfg = [pscustomobject]@{
    MinimizeToTray = $false
    Paths          = [pscustomobject]@{
        KeyboardLighting = (Resolve-DefaultFolder 'keyboard-lighting' 'Tray.ps1' 'KeyboardLighting')
        BluetoothGuard   = (Resolve-DefaultFolder 'bluetooth-guard' 'BluetoothGuard.ps1' 'BluetoothGuard')
        SpatialHudMods   = (Resolve-DefaultModsDir)
        OrcaPluginsDir   = (Join-Path $env:APPDATA 'OrcaSlicer\plugins')
        OrcaBatDir       = (Join-Path $ProgramsDir 'OrcaPlugins')
        ArenaLinkDir     = (Resolve-DefaultFolder 'arena-link-windows' 'package.json' 'ArenaLink')
    }
}

if (Test-Path (Join-Path $RepoGuess 'Orca-Plugins.bat')) {
    $script:Cfg.Paths.OrcaBatDir = $RepoGuess
}

function Load-HubCfg {
    if (-not (Test-Path $CfgFile)) { return }
    try {
        $o = Get-Content $CfgFile -Raw | ConvertFrom-Json
        if ($null -ne $o.MinimizeToTray) { $script:Cfg.MinimizeToTray = [bool]$o.MinimizeToTray }
        if ($o.Paths) {
            foreach ($p in 'KeyboardLighting','BluetoothGuard','SpatialHudMods','OrcaPluginsDir','OrcaBatDir','ArenaLinkDir') {
                if ($o.Paths.$p) { $script:Cfg.Paths.$p = [string]$o.Paths.$p }
            }
        }
    } catch { }
}

function Save-HubCfg {
    try {
        ($script:Cfg | ConvertTo-Json -Depth 6) | Set-Content -Path $CfgFile -Encoding UTF8 -ErrorAction Stop
    } catch { }
}
Load-HubCfg

# ---------------------------------------------------------------- local inspection helpers
function Read-VersionAssignment([string]$filePath, [string]$varName) {
    if (-not (Test-Path $filePath)) { return '' }
    try {
        $text = Get-Content $filePath -TotalCount 90 -ErrorAction SilentlyContinue | Out-String
        if ($text -match ("(?m)^\s*{0}\s*=\s*['""]([^'""]+)['""]" -f [regex]::Escape($varName))) {
            return $Matches[1]
        }
    } catch { }
    return ''
}

function Get-SpatialHudJar([string]$modsDir) {
    if (-not (Test-Path $modsDir)) { return $null }
    try {
        foreach ($jar in (Get-ChildItem -LiteralPath $modsDir -File -Filter '*.jar' -ErrorAction SilentlyContinue)) {
            try {
                $zip = [System.IO.Compression.ZipFile]::OpenRead($jar.FullName)
                try {
                    $entry = $zip.GetEntry('fabric.mod.json')
                    if ($entry) {
                        $sr = New-Object System.IO.StreamReader($entry.Open())
                        try {
                            $meta = $sr.ReadToEnd() | ConvertFrom-Json
                            if ([string]$meta.id -eq 'spatialhud') { return $jar }
                        } finally { $sr.Dispose() }
                    }
                } finally { $zip.Dispose() }
            } catch { }
        }
    } catch { }
    return $null
}

# ---------------------------------------------------------------- icon
function New-HubIcon {
    $bmp = New-Object System.Drawing.Bitmap 32, 32
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([System.Drawing.Color]::Transparent)
    $bgPath = [HubUi.Theme]::Round((New-Object System.Drawing.Rectangle 2, 2, 28, 28), 8)
    $grad   = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.Rectangle 0, 0, 32, 32),
        [HubUi.Theme]::Accent2, [HubUi.Theme]::Accent,
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal)
    $g.FillPath($grad, $bgPath)
    $b = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    foreach ($pt in @(@(8,8), @(18,8), @(8,18), @(18,18))) {
        $tile = [HubUi.Theme]::Round((New-Object System.Drawing.Rectangle $pt[0], $pt[1], 6, 6), 2)
        $g.FillPath($b, $tile)
        $tile.Dispose()
    }
    $b.Dispose(); $grad.Dispose(); $bgPath.Dispose(); $g.Dispose()
    return [System.Drawing.Icon]::FromHandle($bmp.GetHicon())
}
$AppIcon = New-HubIcon

# ---------------------------------------------------------------- remote state cache + worker
$script:RemoteSync = [hashtable]::Synchronized(@{
    Checking      = $false
    ActiveBranch  = ''
    LastChecked   = ''
    Error         = ''
    OrcaWaveVer   = ''
    OrcaInfillVer = ''
    OrcaBatVer    = ''
    ArenaLinkVer  = ''
    HudAssetName  = ''
    HudAssetUrl   = ''
    HudAssetSize  = [int64]0
    HudUpdatedAt  = ''
    KblTrayHash   = ''
    BtgGuardHash  = ''
    ActionStatus  = ''
    Dirty         = $true
    WorkQueue     = (New-Object System.Collections.Queue)
})

$script:WorkerRunspace = [runspacefactory]::CreateRunspace()
$script:WorkerRunspace.Open()
$script:WorkerPs = [powershell]::Create()
$script:WorkerPs.Runspace = $script:WorkerRunspace
[void]$script:WorkerPs.AddScript({
    param($Sync, $RepoSlug, $PreferredBranch, $FallbackBranch)
    $ErrorActionPreference = 'Continue'
    try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12' } catch { }

    $resolvedRawBase = ''
    function Get-RawBase {
        if ($script:resolvedRawBase) { return $script:resolvedRawBase }
        $candidates = @()
        if ($PreferredBranch) { $candidates += $PreferredBranch }
        $candidates += 'main'
        $candidates += $FallbackBranch
        foreach ($br in $candidates) {
            $testUrl = "https://raw.githubusercontent.com/$RepoSlug/$br/project-hub/Setup.ps1"
            try {
                $r = Invoke-WebRequest -Uri $testUrl -Method Head -UseBasicParsing -TimeoutSec 10 -ErrorAction Stop
                if ($r.StatusCode -eq 200) {
                    $Sync.ActiveBranch = $br
                    $script:resolvedRawBase = "https://raw.githubusercontent.com/$RepoSlug/$br"
                    return $script:resolvedRawBase
                }
            } catch { }
        }
        $Sync.ActiveBranch = 'main'
        $script:resolvedRawBase = "https://raw.githubusercontent.com/$RepoSlug/main"
        return $script:resolvedRawBase
    }

    function Download-CheckedFile([string]$url, [string]$dest) {
        $dir = Split-Path -Parent $dest
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        $tmp = $dest + '.hubtmp'
        Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing -TimeoutSec 25 -ErrorAction Stop
        $len = (Get-Item $tmp).Length
        if ($len -lt 16) {
            Remove-Item $tmp -Force -ErrorAction SilentlyContinue
            throw "Empty file from $url"
        }
        $head = [string](Get-Content $tmp -TotalCount 3 -ErrorAction SilentlyContinue)
        if ($head -match '(?i)^\s*<(?:!doctype|html)') {
            Remove-Item $tmp -Force -ErrorAction SilentlyContinue
            throw "HTML error response for $url"
        }
        Move-Item -LiteralPath $tmp -Destination $dest -Force
        Unblock-File $dest -ErrorAction SilentlyContinue
    }

    while ($true) {
        $cmd = $null
        if ($Sync.WorkQueue.Count -gt 0) {
            try { $cmd = $Sync.WorkQueue.Dequeue() } catch { $cmd = $null }
        }
        if (-not $cmd) {
            Start-Sleep -Milliseconds 140
            continue
        }
        if ($cmd.Op -eq 'stop') { return }

        $Sync.Checking = $true
        $Sync.Error    = ''
        try {
            $RawBase = Get-RawBase
            if ($cmd.Op -eq 'check-remote') {
                $Sync.ActionStatus = 'Checking GitHub for updates...'
                $headers = @{ 'User-Agent' = 'Tests-ProjectHub' }
                try {
                    $manifest = Invoke-RestMethod -Headers $headers -Uri "$RawBase/plugins.json" -TimeoutSec 15 -ErrorAction Stop
                    foreach ($pl in $manifest.plugins) {
                        if ($pl.id -eq 'wave-overhangs')   { $Sync.OrcaWaveVer   = [string]$pl.version }
                        if ($pl.id -eq 'unlayered-infill') { $Sync.OrcaInfillVer = [string]$pl.version }
                    }
                } catch { }

                try {
                    $batRaw = (Invoke-WebRequest -Headers $headers -Uri "$RawBase/Orca-Plugins.bat" -UseBasicParsing -TimeoutSec 15 -ErrorAction Stop).Content
                    if ($batRaw -match 'set "UPDATER_VERSION=([^"]+)"') { $Sync.OrcaBatVer = $Matches[1] }
                } catch { }

                try {
                    $pkg = Invoke-RestMethod -Headers $headers -Uri "$RawBase/arena-link-windows/package.json" -TimeoutSec 15 -ErrorAction Stop
                    if ($pkg.version) { $Sync.ArenaLinkVer = [string]$pkg.version }
                } catch { }

                try {
                    $kblTmp = Join-Path $env:TEMP 'hub_kbl_tray.tmp'
                    Invoke-WebRequest -Headers $headers -Uri "$RawBase/keyboard-lighting/Tray.ps1" -OutFile $kblTmp -UseBasicParsing -TimeoutSec 15 -ErrorAction Stop
                    $Sync.KblTrayHash = (Get-FileHash $kblTmp -Algorithm SHA256).Hash
                    Remove-Item $kblTmp -Force -ErrorAction SilentlyContinue
                } catch { }

                try {
                    $btgTmp = Join-Path $env:TEMP 'hub_btg_guard.tmp'
                    Invoke-WebRequest -Headers $headers -Uri "$RawBase/bluetooth-guard/BluetoothGuard.ps1" -OutFile $btgTmp -UseBasicParsing -TimeoutSec 15 -ErrorAction Stop
                    $Sync.BtgGuardHash = (Get-FileHash $btgTmp -Algorithm SHA256).Hash
                    Remove-Item $btgTmp -Force -ErrorAction SilentlyContinue
                } catch { }

                try {
                    $rel = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/$RepoSlug/releases/tags/spatial-hud-latest" -TimeoutSec 15 -ErrorAction Stop
                    $asset = @($rel.assets | Where-Object { $_.name -match '^spatial-hud-.*\.jar$' }) | Select-Object -First 1
                    if ($asset) {
                        $Sync.HudAssetName = [string]$asset.name
                        $Sync.HudAssetUrl  = [string]$asset.browser_download_url
                        $Sync.HudAssetSize = [int64]$asset.size
                        $Sync.HudUpdatedAt = [string]$asset.updated_at
                    }
                } catch { }

                $Sync.LastChecked  = (Get-Date).ToString('h:mm:ss tt')
                $Sync.ActionStatus = ('Checked GitHub ({0}) at {1}' -f $Sync.ActiveBranch, $Sync.LastChecked)
            }
            elseif ($cmd.Op -eq 'install-kbl') {
                $target = [string]$cmd.Target
                $Sync.ActionStatus = 'Downloading Keyboard Lighting files...'
                foreach ($f in @('Aura-Background.ps1','Tray.ps1','ui_controls.cs.txt','app.ico','Setup.ps1','Setup.bat','Restart.bat','Zones.ps1','Zones.bat','MyEffect.ps1','README.md')) {
                    try { Download-CheckedFile "$RawBase/keyboard-lighting/$f" (Join-Path $target $f) } catch { }
                }
                $setupBat = Join-Path $target 'Setup.bat'
                if (Test-Path $setupBat) {
                    $psi = New-Object System.Diagnostics.ProcessStartInfo
                    $psi.FileName = $setupBat
                    $psi.WorkingDirectory = $target
                    $psi.UseShellExecute = $true
                    $psi.EnvironmentVariables['KEYBOARD_LIGHTING_BASE'] = "$RawBase/keyboard-lighting"
                    [void][System.Diagnostics.Process]::Start($psi)
                    $Sync.ActionStatus = 'Launched Keyboard Lighting Setup (approve Administrator prompt).'
                }
            }
            elseif ($cmd.Op -eq 'install-btg') {
                $target = [string]$cmd.Target
                $Sync.ActionStatus = 'Installing / updating Bluetooth Guard...'
                foreach ($f in @('BluetoothGuard.ps1','BluetoothCore.cs','Setup.ps1','Setup.bat','README.md')) {
                    Download-CheckedFile "$RawBase/bluetooth-guard/$f" (Join-Path $target $f)
                }
                $setupPs = Join-Path $target 'Setup.ps1'
                $env:BLUETOOTH_GUARD_BASE = "$RawBase/bluetooth-guard"
                powershell.exe -NoProfile -ExecutionPolicy Bypass -File $setupPs -QuietConsole
                $Sync.ActionStatus = 'Bluetooth Guard installed, updated, and running.'
            }
            elseif ($cmd.Op -eq 'install-hud') {
                $modsDir = [string]$cmd.Target
                $Sync.ActionStatus = 'Downloading latest Spatial HUD JAR...'
                $headers = @{ 'User-Agent' = 'Tests-ProjectHub' }
                $rel = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/$RepoSlug/releases/tags/spatial-hud-latest" -TimeoutSec 20 -ErrorAction Stop
                $asset = @($rel.assets | Where-Object { $_.name -match '^spatial-hud-.*\.jar$' }) | Select-Object -First 1
                if (-not $asset) { throw 'No Spatial HUD release JAR found on GitHub.' }
                if (-not (Test-Path $modsDir)) { New-Item -ItemType Directory -Force -Path $modsDir | Out-Null }
                $tmpJar = Join-Path $env:TEMP ('spatialhud_' + [Guid]::NewGuid().ToString('N') + '.jar')
                Invoke-WebRequest -Headers $headers -Uri $asset.browser_download_url -OutFile $tmpJar -UseBasicParsing -TimeoutSec 45 -ErrorAction Stop
                try { Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue } catch { }
                $zip = [System.IO.Compression.ZipFile]::OpenRead($tmpJar)
                $okMod = $false
                try {
                    $entry = $zip.GetEntry('fabric.mod.json')
                    if ($entry) {
                        $sr = New-Object System.IO.StreamReader($entry.Open())
                        try {
                            $m = $sr.ReadToEnd() | ConvertFrom-Json
                            if ([string]$m.id -eq 'spatialhud') { $okMod = $true }
                        } finally { $sr.Dispose() }
                    }
                } finally { $zip.Dispose() }
                if (-not $okMod) {
                    Remove-Item $tmpJar -Force -ErrorAction SilentlyContinue
                    throw 'Downloaded file did not pass Spatial HUD JAR verification.'
                }
                foreach ($existing in (Get-ChildItem -LiteralPath $modsDir -File -Filter 'spatial-hud-*.jar' -ErrorAction SilentlyContinue)) {
                    Remove-Item $existing.FullName -Force -ErrorAction SilentlyContinue
                }
                $finalJar = Join-Path $modsDir ([string]$asset.name)
                Move-Item -LiteralPath $tmpJar -Destination $finalJar -Force
                $Sync.ActionStatus = ('Installed {0} into Modrinth mods folder.' -f $asset.name)
            }
            elseif ($cmd.Op -eq 'install-orca') {
                $batDir  = [string]$cmd.BatDir
                $plugDir = [string]$cmd.Target
                $Sync.ActionStatus = 'Updating OrcaSlicer plugins & Orca-Plugins.bat...'
                Download-CheckedFile "$RawBase/Orca-Plugins.bat" (Join-Path $batDir 'Orca-Plugins.bat')
                if (-not (Test-Path $plugDir)) { New-Item -ItemType Directory -Force -Path $plugDir | Out-Null }
                Download-CheckedFile "$RawBase/plugins/wave-overhangs/wave_overhangs_orca.py" (Join-Path $plugDir 'wave_overhangs_orca.py')
                Download-CheckedFile "$RawBase/plugins/unlayered-infill/unlayered_infill_orca.py" (Join-Path $plugDir 'unlayered_infill_orca.py')
                $Sync.ActionStatus = 'Updated Orca-Plugins.bat + Wave Overhangs + Unlayered Infill.'
            }
            elseif ($cmd.Op -eq 'install-arena') {
                $target = [string]$cmd.Target
                $Sync.ActionStatus = 'Refreshing Arena Link source files...'
                foreach ($f in @('package.json','main.js','core.js','preload.js','README.md','renderer/index.html','renderer/app.js','renderer/styles.css')) {
                    try { Download-CheckedFile "$RawBase/arena-link-windows/$f" (Join-Path $target ($f -replace '/', '\')) } catch { }
                }
                $Sync.ActionStatus = 'Arena Link source files synced from GitHub.'
            }
        } catch {
            $Sync.Error        = $_.Exception.Message
            $Sync.ActionStatus = ('Error: {0}' -f $_.Exception.Message)
        }
        $Sync.Checking = $false
        $Sync.Dirty    = $true
    }
}).AddArgument($script:RemoteSync).AddArgument($RepoOwner).AddArgument($PrefBranch).AddArgument($FallbackBr)
$script:WorkerHandle = $script:WorkerPs.BeginInvoke()

function Queue-HubWork($payload) {
    $script:RemoteSync.WorkQueue.Enqueue([pscustomobject]$payload)
}

# ---------------------------------------------------------------- UI layout
$Bg  = [HubUi.Theme]::Bg
$Mut = [HubUi.Theme]::Muted
$Txt = [HubUi.Theme]::Text

$fontH1   = New-Object System.Drawing.Font('Segoe UI Semibold', 15.5, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontCard = New-Object System.Drawing.Font('Segoe UI Semibold', 11.25, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontPill = New-Object System.Drawing.Font('Segoe UI Semibold', 8.25, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontBd   = New-Object System.Drawing.Font('Segoe UI', 9.25, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontSm   = New-Object System.Drawing.Font('Segoe UI', 8.5, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)

$form = New-Object System.Windows.Forms.Form
$form.Text                = 'Project Hub'
$form.FormBorderStyle     = 'None'
$form.StartPosition       = 'CenterScreen'
$form.BackColor           = $Bg
$form.ForeColor           = $Txt
$form.Font                = $fontBd
$form.Icon                = $AppIcon
$form.ShowInTaskbar       = $true
$form.AutoScaleDimensions = New-Object System.Drawing.SizeF(96, 96)
$form.AutoScaleMode       = 'Dpi'
$form.ClientSize          = New-Object System.Drawing.Size(650, 745)

$bar = New-Object HubUi.TitleBar $form
$bar.Text = 'Project Hub'
$bar.Font = $fontBd
try { $bar.Logo = $AppIcon.ToBitmap() } catch { }
$form.Controls.Add($bar)

$body = New-Object System.Windows.Forms.Panel
$body.Dock       = 'Fill'
$body.BackColor  = $Bg
$body.AutoScroll = $true
$form.Controls.Add($body)
$body.BringToFront()

$M  = 20
$CW = 610
$y  = 12

# Top command card
$hero = New-Object HubUi.Card
$hero.Location = New-Object System.Drawing.Point($M, $y)
$hero.Size     = New-Object System.Drawing.Size($CW, 96)
$body.Controls.Add($hero)

$lblHero = New-Object System.Windows.Forms.Label
$lblHero.Text      = 'All Projects Control Center'
$lblHero.Location  = New-Object System.Drawing.Point(16, 10)
$lblHero.Size      = New-Object System.Drawing.Size(340, 28)
$lblHero.ForeColor = $Txt
$lblHero.Font      = $fontH1
$lblHero.BackColor = [System.Drawing.Color]::Transparent
$hero.Controls.Add($lblHero)

$btnCheckAll = New-Object HubUi.FlatBtn
$btnCheckAll.Text     = 'Check Status'
$btnCheckAll.Location = New-Object System.Drawing.Point(($CW - 246), 12)
$btnCheckAll.Size     = New-Object System.Drawing.Size(110, 32)
$btnCheckAll.Font     = $fontSm
$hero.Controls.Add($btnCheckAll)

$btnUpdateAll = New-Object HubUi.FlatBtn
$btnUpdateAll.Text     = 'Update All'
$btnUpdateAll.Primary  = $true
$btnUpdateAll.Location = New-Object System.Drawing.Point(($CW - 126), 12)
$btnUpdateAll.Size     = New-Object System.Drawing.Size(110, 32)
$btnUpdateAll.Font     = $fontSm
$hero.Controls.Add($btnUpdateAll)

$lblGlobal = New-Object System.Windows.Forms.Label
$lblGlobal.Text      = 'Checking local installations and GitHub updates...'
$lblGlobal.Location  = New-Object System.Drawing.Point(16, 42)
$lblGlobal.Size      = New-Object System.Drawing.Size(578, 18)
$lblGlobal.ForeColor = $Mut
$lblGlobal.Font      = $fontSm
$lblGlobal.BackColor = [System.Drawing.Color]::Transparent
$hero.Controls.Add($lblGlobal)

$chkTray = New-Object HubUi.Toggle
$chkTray.Text     = 'Minimize Project Hub to system tray when closed'
$chkTray.Location = New-Object System.Drawing.Point(16, 65)
$chkTray.Size     = New-Object System.Drawing.Size(420, 24)
$chkTray.Font     = $fontSm
$chkTray.SetQuiet([bool]$script:Cfg.MinimizeToTray)
$hero.Controls.Add($chkTray)

$y += 96 + 12

# Reusable helper to construct uniform, clean project cards
function New-ProjectCard([string]$title, [string]$subtitle, [System.Drawing.Color]$stripeColor, [string[]]$buttonLabels) {
    $card = New-Object HubUi.Card
    $card.Location     = New-Object System.Drawing.Point($M, $script:y)
    $card.Size         = New-Object System.Drawing.Size($CW, 108)
    $card.AccentStripe = $stripeColor
    $body.Controls.Add($card)

    $lblT = New-Object System.Windows.Forms.Label
    $lblT.Text      = $title
    $lblT.Location  = New-Object System.Drawing.Point(18, 10)
    $lblT.Size      = New-Object System.Drawing.Size(340, 22)
    $lblT.ForeColor = $Txt
    $lblT.Font      = $fontCard
    $lblT.BackColor = [System.Drawing.Color]::Transparent
    $card.Controls.Add($lblT)

    $pill = New-Object HubUi.PillBadge
    $pill.Text      = 'Checking...'
    $pill.PillColor = $Mut
    $pill.Font      = $fontPill
    $pill.Location  = New-Object System.Drawing.Point(($CW - 206), 10)
    $pill.Size      = New-Object System.Drawing.Size(190, 22)
    $card.Controls.Add($pill)

    $lblS = New-Object System.Windows.Forms.Label
    $lblS.Text      = $subtitle
    $lblS.Location  = New-Object System.Drawing.Point(18, 36)
    $lblS.Size      = New-Object System.Drawing.Size(($CW - 34), 20)
    $lblS.ForeColor = $Mut
    $lblS.Font      = $fontSm
    $lblS.BackColor = [System.Drawing.Color]::Transparent
    $card.Controls.Add($lblS)

    $btns = @()
    $bx = 18
    for ($i = 0; $i -lt $buttonLabels.Count; $i++) {
        $b = New-Object HubUi.FlatBtn
        $b.Text     = $buttonLabels[$i]
        $b.Primary  = ($i -eq 0)
        $b.Font     = $fontSm
        $b.Location = New-Object System.Drawing.Point($bx, 64)
        $b.Size     = New-Object System.Drawing.Size(136, 32)
        $card.Controls.Add($b)
        $btns += $b
        $bx += 145
    }

    $script:y += 108 + 10
    return [pscustomobject]@{ Card = $card; Pill = $pill; Detail = $lblS; Buttons = $btns }
}

$uiKbl   = New-ProjectCard 'Keyboard Lighting' 'ROG Strix G16 per-zone keyboard & chassis light bar' ([HubUi.Theme]::Accent)  @('Open / Start', 'Restart / Unfreeze', 'Update / Repair', 'Open Folder')
$uiBtg   = New-ProjectCard 'Bluetooth Guard'   'Prevents Windows stealing phone earbuds on boot/wake' ([HubUi.Theme]::Accent2) @('Open / Start', 'Restart Guard', 'Install / Update', 'Open Folder')
$uiHud   = New-ProjectCard 'Spatial HUD (Minecraft 26.3)' 'Waist-height lower HUD mod for Modrinth F5W' ([HubUi.Theme]::Good)    @('Install / Update', 'Pick Mods Folder', 'Open Mods Folder')
$uiOrca  = New-ProjectCard 'OrcaSlicer Plugins' 'Wave Overhangs + Unlayered Infill + Orca-Plugins.bat' ([HubUi.Theme]::Warn)    @('Update Plugins', 'Open Updater Menu', 'Plugins Folder', 'Logs Folder')
$uiArena = New-ProjectCard 'Arena Link Windows' 'Local Git repository bridge desktop app'              ([HubUi.Theme]::Accent)  @('Launch App', 'Sync Latest Files', 'Open Folder')

# ---------------------------------------------------------------- card button actions
# 1. Keyboard Lighting
$uiKbl.Buttons[0].Add_Click({
    $dir  = $script:Cfg.Paths.KeyboardLighting
    $exe  = Join-Path $dir 'KeyboardLighting.exe'
    $tray = Join-Path $dir 'Tray.ps1'
    if (Test-Path $exe) { Start-Process -FilePath $exe -WorkingDirectory $dir }
    elseif (Test-Path $tray) { Start-Process -FilePath 'powershell.exe' -Verb RunAs -WorkingDirectory $dir -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-WindowStyle','Hidden','-File',('"{0}"' -f $tray),'-NoElevate') }
    else { Queue-HubWork @{ Op = 'install-kbl'; Target = $dir } }
})
$uiKbl.Buttons[1].Add_Click({
    $dir = $script:Cfg.Paths.KeyboardLighting
    $rst = Join-Path $dir 'Restart.bat'
    $tray= Join-Path $dir 'Tray.ps1'
    if (Test-Path $rst) { Start-Process -FilePath $rst -WorkingDirectory $dir }
    elseif (Test-Path $tray) { Start-Process -FilePath 'powershell.exe' -Verb RunAs -WorkingDirectory $dir -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-WindowStyle','Hidden','-File',('"{0}"' -f $tray),'-Restart','-NoElevate') }
    $lblGlobal.Text = 'Sent clean restart signal to Keyboard Lighting.'
})
$uiKbl.Buttons[2].Add_Click({ Queue-HubWork @{ Op = 'install-kbl'; Target = $script:Cfg.Paths.KeyboardLighting } })
$uiKbl.Buttons[3].Add_Click({
    $dir = $script:Cfg.Paths.KeyboardLighting
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Start-Process explorer.exe $dir
})

# 2. Bluetooth Guard
$uiBtg.Buttons[0].Add_Click({
    $dir = $script:Cfg.Paths.BluetoothGuard
    $exe = Join-Path $dir 'BluetoothGuard.exe'
    $ps  = Join-Path $dir 'BluetoothGuard.ps1'
    if (Test-Path $exe) { Start-Process -FilePath $exe -WorkingDirectory $dir }
    elseif (Test-Path $ps) { Start-Process -FilePath 'powershell.exe' -WorkingDirectory $dir -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-WindowStyle','Hidden','-File',('"{0}"' -f $ps)) }
    else { Queue-HubWork @{ Op = 'install-btg'; Target = $dir } }
})
$uiBtg.Buttons[1].Add_Click({
    $dir = $script:Cfg.Paths.BluetoothGuard
    $ps  = Join-Path $dir 'BluetoothGuard.ps1'
    if (Test-Path $ps) {
        Start-Process -FilePath 'powershell.exe' -WorkingDirectory $dir -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-WindowStyle','Hidden','-File',('"{0}"' -f $ps),'-Restart')
        $lblGlobal.Text = 'Restarted Bluetooth Guard.'
    } else {
        Queue-HubWork @{ Op = 'install-btg'; Target = $dir }
    }
})
$uiBtg.Buttons[2].Add_Click({ Queue-HubWork @{ Op = 'install-btg'; Target = $script:Cfg.Paths.BluetoothGuard } })
$uiBtg.Buttons[3].Add_Click({
    $dir = $script:Cfg.Paths.BluetoothGuard
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Start-Process explorer.exe $dir
})

# 3. Spatial HUD
$uiHud.Buttons[0].Add_Click({ Queue-HubWork @{ Op = 'install-hud'; Target = $script:Cfg.Paths.SpatialHudMods } })
$uiHud.Buttons[1].Add_Click({
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description  = 'Select your Minecraft / Modrinth mods folder'
    $dlg.SelectedPath = $script:Cfg.Paths.SpatialHudMods
    if ($dlg.ShowDialog() -eq 'OK' -and $dlg.SelectedPath) {
        $script:Cfg.Paths.SpatialHudMods = $dlg.SelectedPath
        Save-HubCfg
        $stateDir = Join-Path $env:LOCALAPPDATA 'SpatialHudUpdater'
        try {
            if (-not (Test-Path $stateDir)) { New-Item -ItemType Directory -Force -Path $stateDir | Out-Null }
            [System.IO.File]::WriteAllText((Join-Path $stateDir 'target-directory.txt'), $dlg.SelectedPath, [System.Text.Encoding]::UTF8)
        } catch { }
        Refresh-StatusCards
    }
    $dlg.Dispose()
})
$uiHud.Buttons[2].Add_Click({
    $dir = $script:Cfg.Paths.SpatialHudMods
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Start-Process explorer.exe $dir
})

# 4. OrcaSlicer Plugins
$uiOrca.Buttons[0].Add_Click({
    Queue-HubWork @{
        Op     = 'install-orca'
        BatDir = $script:Cfg.Paths.OrcaBatDir
        Target = $script:Cfg.Paths.OrcaPluginsDir
    }
})
$uiOrca.Buttons[1].Add_Click({
    $batDir = $script:Cfg.Paths.OrcaBatDir
    $bat    = Join-Path $batDir 'Orca-Plugins.bat'
    if (Test-Path $bat) {
        Start-Process -FilePath $bat -WorkingDirectory $batDir
    } else {
        Queue-HubWork @{ Op = 'install-orca'; BatDir = $batDir; Target = $script:Cfg.Paths.OrcaPluginsDir }
    }
})
$uiOrca.Buttons[2].Add_Click({
    $dir = $script:Cfg.Paths.OrcaPluginsDir
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Start-Process explorer.exe $dir
})
$uiOrca.Buttons[3].Add_Click({
    $logs = Join-Path $env:APPDATA 'OrcaSlicer\plugins'
    if (-not (Test-Path $logs)) { New-Item -ItemType Directory -Force -Path $logs | Out-Null }
    Start-Process explorer.exe $logs
})

# 5. Arena Link
$uiArena.Buttons[0].Add_Click({
    $dir = $script:Cfg.Paths.ArenaLinkDir
    $distExe = Get-ChildItem -Path (Join-Path $dir 'dist') -Filter '*Arena*Link*.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($distExe) {
        Start-Process -FilePath $distExe.FullName -WorkingDirectory $distExe.DirectoryName
    } elseif (Get-Command npm -ErrorAction SilentlyContinue) {
        Start-Process -FilePath 'cmd.exe' -ArgumentList '/c npm start' -WorkingDirectory $dir
    } else {
        Start-Process explorer.exe $dir
    }
})
$uiArena.Buttons[1].Add_Click({ Queue-HubWork @{ Op = 'install-arena'; Target = $script:Cfg.Paths.ArenaLinkDir } })
$uiArena.Buttons[2].Add_Click({
    $dir = $script:Cfg.Paths.ArenaLinkDir
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Start-Process explorer.exe $dir
})

# Global buttons
$btnCheckAll.Add_Click({ Queue-HubWork @{ Op = 'check-remote' } })
$btnUpdateAll.Add_Click({
    Queue-HubWork @{ Op = 'install-btg';   Target = $script:Cfg.Paths.BluetoothGuard }
    Queue-HubWork @{ Op = 'install-orca';  BatDir = $script:Cfg.Paths.OrcaBatDir; Target = $script:Cfg.Paths.OrcaPluginsDir }
    Queue-HubWork @{ Op = 'install-hud';   Target = $script:Cfg.Paths.SpatialHudMods }
    Queue-HubWork @{ Op = 'install-arena'; Target = $script:Cfg.Paths.ArenaLinkDir }
    Queue-HubWork @{ Op = 'check-remote' }
})
$chkTray.Add_CheckedChanged({
    $script:Cfg.MinimizeToTray = [bool]$chkTray.Checked
    Save-HubCfg
})

# ---------------------------------------------------------------- status refresh logic
function Refresh-StatusCards {
    $goodCol = [HubUi.Theme]::Good
    $warnCol = [HubUi.Theme]::Warn
    $badCol  = [HubUi.Theme]::Bad

    # 1. Keyboard Lighting
    $kblDir  = $script:Cfg.Paths.KeyboardLighting
    $kblTray = Join-Path $kblDir 'Tray.ps1'
    $kblLock = Join-Path $env:LOCALAPPDATA 'KeyboardLighting\running.lock'
    $kblState= Test-LockAlive $kblLock 10.0
    $kblUpd  = $false
    if ((Test-Path $kblTray) -and $script:RemoteSync.KblTrayHash) {
        try { $kblUpd = ((Get-FileHash $kblTray -Algorithm SHA256).Hash -ne $script:RemoteSync.KblTrayHash) } catch { }
    }
    if (-not (Test-Path $kblTray)) {
        $uiKbl.Pill.Text      = 'Not installed'
        $uiKbl.Pill.PillColor = $Mut
        $uiKbl.Detail.Text    = 'Click "Update / Repair" to download and set up Keyboard Lighting.'
    } elseif ($kblState -eq 'hung') {
        $uiKbl.Pill.Text      = 'Frozen - Click Restart'
        $uiKbl.Pill.PillColor = $badCol
        $uiKbl.Detail.Text    = 'Heartbeat stalled (>10s). Click "Restart / Unfreeze" to recover immediately.'
    } elseif ($kblState -eq 'running') {
        $uiKbl.Pill.Text      = if ($kblUpd) { 'Running - Update ready' } else { 'Running' }
        $uiKbl.Pill.PillColor = if ($kblUpd) { $warnCol } else { $goodCol }
        $cfgJson = Join-Path $env:LOCALAPPDATA 'KeyboardLighting\panel.json'
        $summary = 'Active in system tray'
        if (Test-Path $cfgJson) {
            try {
                $p = Get-Content $cfgJson -Raw | ConvertFrom-Json
                $summary = ('Keys: {0}  |  Light bar: {1}  |  Master {2}%' -f $p.Kbd.Effect, $p.Bar.Effect, $p.Brightness)
            } catch { }
        }
        $uiKbl.Detail.Text = $summary
    } else {
        $uiKbl.Pill.Text      = if ($kblUpd) { 'Update ready' } else { 'Stopped' }
        $uiKbl.Pill.PillColor = if ($kblUpd) { $warnCol } else { $Mut }
        $uiKbl.Detail.Text    = ('Installed at {0}' -f $kblDir)
    }
    $uiKbl.Pill.Invalidate()

    # 2. Bluetooth Guard
    $btgDir  = $script:Cfg.Paths.BluetoothGuard
    $btgPs   = Join-Path $btgDir 'BluetoothGuard.ps1'
    $btgLock = Join-Path $env:LOCALAPPDATA 'BluetoothGuard\running.lock'
    $btgState= Test-LockAlive $btgLock 10.0
    $btgUpd  = $false
    if ((Test-Path $btgPs) -and $script:RemoteSync.BtgGuardHash) {
        try { $btgUpd = ((Get-FileHash $btgPs -Algorithm SHA256).Hash -ne $script:RemoteSync.BtgGuardHash) } catch { }
    }
    if (-not (Test-Path $btgPs)) {
        $uiBtg.Pill.Text      = 'Not installed'
        $uiBtg.Pill.PillColor = $Mut
        $uiBtg.Detail.Text    = 'Click "Install / Update" to set up phone-earbud auto-connect protection.'
    } else {
        $blocked = 0; $total = 0
        $btgCfg = Join-Path $env:LOCALAPPDATA 'BluetoothGuard\settings.json'
        if (Test-Path $btgCfg) {
            try {
                $bc = Get-Content $btgCfg -Raw | ConvertFrom-Json
                if ($bc.Devices) {
                    foreach ($pr in $bc.Devices.PSObject.Properties) {
                        $total++
                        if (-not [bool]$pr.Value.AllowAutoConnect) { $blocked++ }
                    }
                }
            } catch { }
        }
        if ($btgState -eq 'running') {
            $uiBtg.Pill.Text      = if ($btgUpd) { 'Guarding - Update ready' } else { 'Guarding' }
            $uiBtg.Pill.PillColor = if ($btgUpd) { $warnCol } else { $goodCol }
            $uiBtg.Detail.Text    = ('Active in tray  |  {0} of {1} paired device(s) set to Manual Only' -f $blocked, $total)
        } else {
            $uiBtg.Pill.Text      = if ($btgUpd) { 'Update ready' } else { 'Stopped' }
            $uiBtg.Pill.PillColor = if ($btgUpd) { $warnCol } else { $Mut }
            $uiBtg.Detail.Text    = ('{0} blocked rule(s) saved  |  Click "Open / Start" to guard Bluetooth' -f $blocked)
        }
    }
    $uiBtg.Pill.Invalidate()

    # 3. Spatial HUD
    $modsDir = $script:Cfg.Paths.SpatialHudMods
    $jar     = Get-SpatialHudJar $modsDir
    if (-not $jar) {
        $uiHud.Pill.Text      = 'Not installed'
        $uiHud.Pill.PillColor = $Mut
        $uiHud.Detail.Text    = ('Mods folder: {0}' -f $modsDir)
    } else {
        $remoteSize = [int64]$script:RemoteSync.HudAssetSize
        $needsUpd   = ($remoteSize -gt 0 -and $jar.Length -ne $remoteSize)
        $uiHud.Pill.Text      = if ($needsUpd) { 'Update available' } else { 'Installed' }
        $uiHud.Pill.PillColor = if ($needsUpd) { $warnCol } else { $goodCol }
        $uiHud.Detail.Text    = ('{0} ({1} KB) in {2}' -f $jar.Name, [Math]::Round($jar.Length / 1KB, 1), $modsDir)
    }
    $uiHud.Pill.Invalidate()

    # 4. OrcaSlicer Plugins
    $plugDir   = $script:Cfg.Paths.OrcaPluginsDir
    $locWave   = Read-VersionAssignment (Join-Path $plugDir 'wave_overhangs_orca.py') 'PLUGIN_VERSION'
    $locInfill = Read-VersionAssignment (Join-Path $plugDir 'unlayered_infill_orca.py') 'PLUGIN_VERSION'
    $remWave   = $script:RemoteSync.OrcaWaveVer
    $remInfill = $script:RemoteSync.OrcaInfillVer
    if (-not $locWave -and -not $locInfill) {
        $uiOrca.Pill.Text      = 'Not installed'
        $uiOrca.Pill.PillColor = $Mut
        $uiOrca.Detail.Text    = ('Latest on GitHub: Wave Overhangs {0}  |  Unlayered Infill {1}' -f $(if ($remWave) { $remWave } else { '...' }), $(if ($remInfill) { $remInfill } else { '...' }))
    } else {
        $outdated = ($remWave -and $locWave -ne $remWave) -or ($remInfill -and $locInfill -ne $remInfill)
        $uiOrca.Pill.Text      = if ($outdated) { 'Update available' } else { 'Up to date' }
        $uiOrca.Pill.PillColor = if ($outdated) { $warnCol } else { $goodCol }
        $uiOrca.Detail.Text    = ('Installed: Wave Overhangs {0}  |  Unlayered Infill {1}' -f $(if ($locWave) { $locWave } else { 'none' }), $(if ($locInfill) { $locInfill } else { 'none' }))
    }
    $uiOrca.Pill.Invalidate()

    # 5. Arena Link Windows
    $arenaDir = $script:Cfg.Paths.ArenaLinkDir
    $pkgFile  = Join-Path $arenaDir 'package.json'
    $locVer   = ''
    if (Test-Path $pkgFile) {
        try { $locVer = [string]((Get-Content $pkgFile -Raw | ConvertFrom-Json).version) } catch { }
    }
    $stateJson = Join-Path $env:APPDATA 'arena-link-windows\arena-link-state.json'
    $linkedCnt = 0
    if (Test-Path $stateJson) {
        try { $linkedCnt = @((Get-Content $stateJson -Raw | ConvertFrom-Json).repositories).Count } catch { }
    }
    if (-not $locVer) {
        $uiArena.Pill.Text      = 'Not synced'
        $uiArena.Pill.PillColor = $Mut
        $uiArena.Detail.Text    = ('{0} linked repo(s) in AppData  |  Click "Sync Latest Files"' -f $linkedCnt)
    } else {
        $remVer = $script:RemoteSync.ArenaLinkVer
        $upd    = ($remVer -and $remVer -ne $locVer)
        $uiArena.Pill.Text      = if ($upd) { "Update ($remVer)" } else { "v$locVer Ready" }
        $uiArena.Pill.PillColor = if ($upd) { $warnCol } else { $goodCol }
        $uiArena.Detail.Text    = ('v{0}  |  {1} linked local Git repository(s)' -f $locVer, $linkedCnt)
    }
    $uiArena.Pill.Invalidate()
}

# ---------------------------------------------------------------- tray icon + loop
$icon = New-Object System.Windows.Forms.NotifyIcon
$icon.Icon    = $AppIcon
$icon.Text    = 'Project Hub'
$icon.Visible = $true

$trayMenu = New-Object System.Windows.Forms.ContextMenuStrip
$trayMenu.BackColor       = [HubUi.Theme]::Panel2
$trayMenu.ForeColor       = $Txt
$trayMenu.Font            = $fontBd
$trayMenu.ShowImageMargin = $false

$miOpenHub = New-Object System.Windows.Forms.ToolStripMenuItem
$miOpenHub.Text = 'Open Project Hub'
$miOpenHub.Add_Click({
    $form.Show()
    $form.WindowState = 'Normal'
    [void]$form.Activate()
})
[void]$trayMenu.Items.Add($miOpenHub)

$miRestartKbl = New-Object System.Windows.Forms.ToolStripMenuItem
$miRestartKbl.Text = 'Restart Keyboard Lighting'
$miRestartKbl.Add_Click({ $uiKbl.Buttons[1].PerformClick() })
[void]$trayMenu.Items.Add($miRestartKbl)

$miOpenBtg = New-Object System.Windows.Forms.ToolStripMenuItem
$miOpenBtg.Text = 'Open Bluetooth Guard'
$miOpenBtg.Add_Click({ $uiBtg.Buttons[0].PerformClick() })
[void]$trayMenu.Items.Add($miOpenBtg)

[void]$trayMenu.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator))

$script:Quitting = $false
function Quit-Hub {
    $script:Quitting = $true
    try { $icon.Visible = $false; $icon.Dispose() } catch { }
    try { Queue-HubWork @{ Op = 'stop' } } catch { }
    try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }
    try { [System.Windows.Forms.Application]::Exit() } catch { }
}

$miExitHub = New-Object System.Windows.Forms.ToolStripMenuItem
$miExitHub.Text = 'Exit Project Hub'
$miExitHub.Add_Click({ Quit-Hub })
[void]$trayMenu.Items.Add($miExitHub)
$icon.ContextMenuStrip = $trayMenu
$icon.Add_MouseDoubleClick({
    $form.Show()
    $form.WindowState = 'Normal'
    [void]$form.Activate()
})

$bar.Add_MinClicked({
    if ($script:Cfg.MinimizeToTray) { $form.Hide() } else { $form.WindowState = 'Minimized' }
})
$bar.Add_CloseClicked({
    if ($script:Cfg.MinimizeToTray) { $form.Hide() } else { Quit-Hub }
})
$form.Add_FormClosing({
    param($s, $e)
    if ($e.CloseReason -eq [System.Windows.Forms.CloseReason]::UserClosing -and -not $script:Quitting -and $script:Cfg.MinimizeToTray) {
        $e.Cancel = $true
        $form.Hide()
    }
})
$form.Add_Shown({
    try { [HubUi.Win]::RoundCorners($form.Handle) } catch { }
    try { [HubUi.Win]::SetIcon($form.Handle, $AppIcon.Handle) } catch { }
    try { Set-Content -Path $ReadyFile -Value $PID -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
})

$tickCount = 0
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 500
$timer.Add_Tick({
    Update-Heartbeat
    if (Test-Path $ShowFile) {
        try { Remove-Item $ShowFile -Force -ErrorAction SilentlyContinue } catch { }
        $form.Show()
        $form.WindowState = 'Normal'
        [void]$form.Activate()
    }
    $script:tickCount++
    if ($script:RemoteSync.ActionStatus -and $lblGlobal.Text -ne $script:RemoteSync.ActionStatus) {
        $lblGlobal.Text = [string]$script:RemoteSync.ActionStatus
    }
    if ($script:RemoteSync.Dirty -or ($script:tickCount % 4 -eq 0)) {
        $script:RemoteSync.Dirty = $false
        Refresh-StatusCards
    }
})
$timer.Start()

try {
    [AppDomain]::CurrentDomain.add_ProcessExit(
        [EventHandler]{
            param($s, $e)
            try { $icon.Visible = $false; $icon.Dispose() } catch { }
            try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }
        })
} catch { }

Refresh-StatusCards
Queue-HubWork @{ Op = 'check-remote' }

if (-not $Silent) {
    $form.Show()
    [void]$form.Activate()
} else {
    try { Set-Content -Path $ReadyFile -Value $PID -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
}
[System.Windows.Forms.Application]::Run()
Quit-Hub

} catch {
    $report = @(
        ('Project Hub crashed at {0}' -f (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')),
        ('Error: {0}' -f $_.Exception.Message),
        ('Where: {0}' -f $_.InvocationInfo.PositionMessage),
        ('Stack: {0}' -f $_.ScriptStackTrace)
    ) -join "`r`n`r`n"
    try { Set-Content -Path $CrashFile -Value $report -Encoding UTF8 -ErrorAction SilentlyContinue } catch { }
    try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }
    if (-not $Silent) {
        try {
            Add-Type -AssemblyName System.Windows.Forms -ErrorAction SilentlyContinue
            [void][System.Windows.Forms.MessageBox]::Show($report, 'Project Hub Error')
        } catch { }
    }
    exit 1
}
