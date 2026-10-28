# =====================================================================
#  Bluetooth Guard
#
#  Stops Windows from auto-connecting to chosen Bluetooth earbuds or
#  headphones at startup, lid wake, or when Bluetooth turns on - so your
#  laptop stops stealing audio from your phone.
#
#  How it works:
#    - Allowed devices behave normally and auto-connect as usual.
#    - Manual Only devices keep their pairing intact in Windows, have
#      their auto-connect profiles kept disabled when idle, and drop any
#      unwanted connection immediately.
#    - Clicking "Connect Now" (in the window or the system tray menu)
#      connects a Manual Only device for this session, then automatically
#      re-locks it as soon as you disconnect, put your earbuds away, or
#      sleep/shut down your PC.
# =====================================================================

param(
    [switch]$Silent,
    [switch]$Stop,
    [switch]$Restart,
    [switch]$StatusJson
)

$ErrorActionPreference = 'Continue'

$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path
$CfgDir    = Join-Path $env:LOCALAPPDATA 'BluetoothGuard'
$CfgFile   = Join-Path $CfgDir 'settings.json'
$LockFile  = Join-Path $CfgDir 'running.lock'
$ShowFile  = Join-Path $CfgDir 'show.flag'
$ReadyFile = Join-Path $CfgDir 'window.ready'
$CrashFile = Join-Path $CfgDir 'crash.log'
$LogDir    = Join-Path $CfgDir 'logs'
$LogFile   = Join-Path $LogDir 'guard.log'
$RunKey    = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$RunName   = 'BluetoothGuard'
$Base      = if ($env:BLUETOOTH_GUARD_BASE) { $env:BLUETOOTH_GUARD_BASE } else { 'https://raw.githubusercontent.com/ajani190819-ops/Tests/main/bluetooth-guard' }

foreach ($d in @($CfgDir, $LogDir)) {
    try { if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force -Path $d | Out-Null } } catch { }
}

function Log([string]$msg, [string]$level = 'INFO') {
    $line = '{0}  {1,-5}  {2}' -f (Get-Date).ToString('yyyy-MM-dd HH:mm:ss'), $level, $msg
    try {
        Add-Content -Path $LogFile -Value $line -Encoding UTF8 -ErrorAction SilentlyContinue
        $fi = Get-Item $LogFile -ErrorAction SilentlyContinue
        if ($fi -and $fi.Length -gt 262144) {
            $keep = Get-Content $LogFile -Tail 400 -ErrorAction SilentlyContinue
            Set-Content -Path $LogFile -Value $keep -Encoding UTF8 -ErrorAction SilentlyContinue
        }
    } catch { }
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

function Test-InstanceHealthy {
    if (-not (Test-Path $LockFile)) { return $false }
    try {
        $age = ([DateTime]::UtcNow - (Get-Item $LockFile).LastWriteTimeUtc).TotalSeconds
        $id  = Read-PidFile $LockFile
        if ($id -le 0 -or $id -eq $PID) { return $false }
        $p = [System.Diagnostics.Process]::GetProcessById($id)
        if (-not $p -or $p.HasExited) { return $false }
        return ($age -lt 10.0)
    } catch { return $false }
}

function Stop-RunningGuard {
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
}

if ($StatusJson) {
    $running = Test-InstanceHealthy
    $blockedCount = 0
    $knownCount   = 0
    $enabledGuard = $true
    if (Test-Path $CfgFile) {
        try {
            $c = Get-Content $CfgFile -Raw | ConvertFrom-Json
            if ($null -ne $c.GuardEnabled) { $enabledGuard = [bool]$c.GuardEnabled }
            if ($c.Devices) {
                foreach ($p in $c.Devices.PSObject.Properties) {
                    $knownCount++
                    if (-not [bool]$p.Value.AllowAutoConnect) { $blockedCount++ }
                }
            }
        } catch { }
    }
    [pscustomobject]@{
        Running      = $running
        GuardEnabled = $enabledGuard
        KnownDevices = $knownCount
        BlockedCount = $blockedCount
    } | ConvertTo-Json -Compress
    return
}

if ($Stop) {
    Log 'stopping Bluetooth Guard via -Stop'
    Stop-RunningGuard
    return
}

if ($Restart) {
    Log 'restarting Bluetooth Guard via -Restart'
    Stop-RunningGuard
}

try {

if (Test-InstanceHealthy) {
    try { Set-Content -Path $ShowFile -Value ([DateTime]::UtcNow.Ticks) -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
    $deadline = [DateTime]::UtcNow.AddMilliseconds(1500)
    $ack = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 75
        if (-not (Test-Path $ShowFile)) { $ack = $true; break }
    }
    if ($ack) {
        try { Set-Content -Path $ReadyFile -Value 'reused' -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
        return
    }
    Log 'hung instance detected - recovering' 'WARN'
    Stop-RunningGuard
} elseif (Test-Path $LockFile) {
    Stop-RunningGuard
}

function Update-Heartbeat {
    try { Set-Content -Path $LockFile -Value $PID -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
}
Update-Heartbeat

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
try {
    [System.Windows.Forms.Application]::SetUnhandledExceptionMode(
        [System.Windows.Forms.UnhandledExceptionMode]::CatchException)
} catch { }
[System.Windows.Forms.Application]::EnableVisualStyles()

# Load compiled core DLL or compile BluetoothCore.cs on demand.
$coreDll  = Join-Path $Here 'BluetoothCore.dll'
$coreFile = Join-Path $Here 'BluetoothCore.cs'
$loaded   = $false
if (Test-Path $coreDll) {
    try {
        $srcF = Get-Item $coreFile -ErrorAction SilentlyContinue
        $dllF = Get-Item $coreDll
        if (-not $srcF -or $dllF.LastWriteTime -ge $srcF.LastWriteTime) {
            Add-Type -Path $coreDll -ErrorAction Stop
            $loaded = $true
        }
    } catch { Log ("BluetoothCore.dll load fell back to source: {0}" -f $_.Exception.Message) 'WARN' }
}
if (-not $loaded) {
    Add-Type -TypeDefinition (Get-Content $coreFile -Raw) `
        -ReferencedAssemblies 'System.Windows.Forms','System.Drawing','System' -ErrorAction Stop
}

try { [BtGuard.Dpi]::Enable() } catch { }
try { [BtGuard.Win]::SetAppId('Tests.BluetoothGuard') } catch { }

# ---------------------------------------------------------------- configuration
# Default behaviour for newly seen devices:
#   - Audio devices default to AllowAutoConnect = $true UNTIL the user switches
#     the ones they share with their phone off (so nothing breaks surprisingly),
#     OR the user can toggle "Block new audio devices automatically".
$script:Cfg = [pscustomobject]@{
    GuardEnabled       = $true
    DefaultBlockAudio  = $false
    Devices            = [pscustomobject]@{}
}

function Load-Cfg {
    if (-not (Test-Path $CfgFile)) { return }
    try {
        $o = Get-Content $CfgFile -Raw | ConvertFrom-Json
        if ($null -ne $o.GuardEnabled)      { $script:Cfg.GuardEnabled      = [bool]$o.GuardEnabled }
        if ($null -ne $o.DefaultBlockAudio) { $script:Cfg.DefaultBlockAudio = [bool]$o.DefaultBlockAudio }
        if ($o.Devices)                     { $script:Cfg.Devices           = $o.Devices }
    } catch { Log ("config load warning: {0}" -f $_.Exception.Message) 'WARN' }
}

function Save-Cfg {
    try {
        $tmp = $CfgFile + '.tmp'
        ($script:Cfg | ConvertTo-Json -Depth 6) | Set-Content -Path $tmp -Encoding UTF8 -ErrorAction Stop
        Move-Item -LiteralPath $tmp -Destination $CfgFile -Force -ErrorAction Stop
    } catch {
        try { ($script:Cfg | ConvertTo-Json -Depth 6) | Set-Content -Path $CfgFile -Encoding UTF8 -ErrorAction SilentlyContinue } catch { }
    }
}

Load-Cfg

function Ensure-DeviceEntry($snap) {
    $mac = [string]$snap.Mac
    $prop = $script:Cfg.Devices.PSObject.Properties[$mac]
    if (-not $prop) {
        $allow = $true
        if ($snap.Kind -eq 'Audio' -and $script:Cfg.DefaultBlockAudio) { $allow = $false }
        $entry = [pscustomobject]@{
            Name             = [string]$snap.Name
            Kind             = [string]$snap.Kind
            Address          = [uint64]$snap.Address
            AllowAutoConnect = $allow
        }
        Add-Member -InputObject $script:Cfg.Devices -MemberType NoteProperty -Name $mac -Value $entry -Force
        Save-Cfg
        return $entry
    }
    $e = $prop.Value
    if ($snap.Name -and $snap.Name -ne $mac) { $e.Name = [string]$snap.Name }
    if ($snap.Kind) { $e.Kind = [string]$snap.Kind }
    $e.Address = [uint64]$snap.Address
    return $e
}

function Test-Autostart {
    try {
        $v = Get-ItemProperty -Path $RunKey -Name $RunName -ErrorAction SilentlyContinue
        return ($null -ne $v -and [bool]$v.$RunName)
    } catch { return $false }
}

function Set-Autostart([bool]$on) {
    try {
        if (-not $on) {
            Remove-ItemProperty -Path $RunKey -Name $RunName -ErrorAction SilentlyContinue
            return $true
        }
        $exe = Join-Path $Here 'BluetoothGuard.exe'
        if (Test-Path $exe) {
            $cmd = '"{0}" -Silent' -f $exe
        } else {
            $cmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}" -Silent' -f $PSCommandPath
        }
        Set-ItemProperty -Path $RunKey -Name $RunName -Value $cmd -ErrorAction Stop
        return $true
    } catch {
        Log ("autostart error: {0}" -f $_.Exception.Message) 'WARN'
        return $false
    }
}

# ---------------------------------------------------------------- icon
function New-BtIcon {
    $bmp = New-Object System.Drawing.Bitmap 32, 32
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([System.Drawing.Color]::Transparent)
    $path = [BtGuard.Theme]::Round((New-Object System.Drawing.Rectangle 2, 2, 28, 28), 8)
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.Rectangle 0, 0, 32, 32),
        [BtGuard.Theme]::Accent2, [BtGuard.Theme]::Accent,
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal)
    $g.FillPath($grad, $path)
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 2.3)
    $pen.StartCap = 'Round'; $pen.EndCap = 'Round'; $pen.LineJoin = 'Round'
    # Crisp Bluetooth rune
    $pts = @(
        (New-Object System.Drawing.Point 11, 11),
        (New-Object System.Drawing.Point 21, 21),
        (New-Object System.Drawing.Point 16, 26),
        (New-Object System.Drawing.Point 16, 6),
        (New-Object System.Drawing.Point 21, 11),
        (New-Object System.Drawing.Point 11, 21)
    )
    $g.DrawLines($pen, $pts)
    $pen.Dispose(); $grad.Dispose(); $path.Dispose(); $g.Dispose()
    return [System.Drawing.Icon]::FromHandle($bmp.GetHicon())
}
$AppIcon = New-BtIcon

# ---------------------------------------------------------------- async worker queue
# All Bluetooth radio/service calls happen off the WinForms UI thread so the
# window and system tray right-click menu respond instantaneously at all times.
$script:StateSync = [hashtable]::Synchronized(@{
    Snapshots     = @()
    RadioPresent  = $true
    Busy          = $false
    LastNote      = 'Scanning paired Bluetooth devices...'
    Dirty         = $true
    WorkQueue     = (New-Object System.Collections.Queue)
})

# Session-approved MACs: user clicked "Connect Now" on a Manual Only device.
# Once it finishes connecting and later disconnects (or laptop sleeps/exits),
# we automatically re-lock it so tomorrow's boot still won't steal phone audio!
$script:SessionAllowed = @{}
$script:SeenConnected  = @{}
$script:EnforcedIdle   = @{}

$script:WorkerRunspace = [runspacefactory]::CreateRunspace()
$script:WorkerRunspace.Open()
$script:WorkerPs = [powershell]::Create()
$script:WorkerPs.Runspace = $script:WorkerRunspace
[void]$script:WorkerPs.AddScript({
    param($Sync, $CoreDllPath, $CoreCsPath)
    if (Test-Path $CoreDllPath) {
        try { Add-Type -Path $CoreDllPath -ErrorAction Stop } catch { }
    }
    if (-not ('BtGuard.BtNative' -as [type])) {
        Add-Type -TypeDefinition (Get-Content $CoreCsPath -Raw) `
            -ReferencedAssemblies 'System.Windows.Forms','System.Drawing','System' -ErrorAction SilentlyContinue
    }
    $nextPoll = [DateTime]::MinValue
    while ($true) {
        $didWork = $false
        while ($Sync.WorkQueue.Count -gt 0) {
            $cmd = $null
            try { $cmd = $Sync.WorkQueue.Dequeue() } catch { $cmd = $null }
            if (-not $cmd) { break }
            $didWork = $true
            $Sync.Busy = $true
            try {
                if ($cmd.Op -eq 'stop-worker') { return }
                elseif ($cmd.Op -eq 'block') {
                    [void][BtGuard.BtNative]::BlockAndDisconnect([uint64]$cmd.Address, [string]$cmd.Kind)
                    $Sync.LastNote = ('Blocked auto-connect for {0}' -f $cmd.Name)
                }
                elseif ($cmd.Op -eq 'connect') {
                    [void][BtGuard.BtNative]::EnableAndConnect([uint64]$cmd.Address, [string]$cmd.Kind)
                    $Sync.LastNote = ('Enabled connection for {0}' -f $cmd.Name)
                }
                elseif ($cmd.Op -eq 'refresh') {
                    $Sync.LastNote = 'Refreshed Bluetooth devices'
                }
            } catch {
                $Sync.LastNote = ('Bluetooth action error: {0}' -f $_.Exception.Message)
            }
            $nextPoll = [DateTime]::MinValue
        }

        if ($didWork -or [DateTime]::UtcNow -ge $nextPoll) {
            try {
                $radio = [BtGuard.BtNative]::IsRadioPresent()
                $list  = @([BtGuard.BtNative]::ListPairedDevices())
                $Sync.RadioPresent = $radio
                $Sync.Snapshots    = $list
                $Sync.Busy         = $false
                $Sync.Dirty        = $true
            } catch {
                $Sync.Busy = $false
            }
            $nextPoll = [DateTime]::UtcNow.AddMilliseconds(2200)
        }
        Start-Sleep -Milliseconds 120
    }
}).AddArgument($script:StateSync).AddArgument($coreDll).AddArgument($coreFile)
$script:WorkerHandle = $script:WorkerPs.BeginInvoke()

function Queue-BtWork([string]$Op, [uint64]$Address = 0, [string]$Kind = 'Audio', [string]$Name = '') {
    $script:StateSync.WorkQueue.Enqueue([pscustomobject]@{
        Op      = $Op
        Address = $Address
        Kind    = $Kind
        Name    = $Name
    })
}

# ---------------------------------------------------------------- UI construction
$Bg  = [BtGuard.Theme]::Bg
$Mut = [BtGuard.Theme]::Muted
$Txt = [BtGuard.Theme]::Text

$fontH1  = New-Object System.Drawing.Font('Segoe UI Semibold', 15, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontSub = New-Object System.Drawing.Font('Segoe UI Semibold', 10.25, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontLb  = New-Object System.Drawing.Font('Segoe UI', 8.25, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Point)
$fontBd  = New-Object System.Drawing.Font('Segoe UI', 9.5, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fontSm  = New-Object System.Drawing.Font('Segoe UI', 8.5, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)

$form = New-Object System.Windows.Forms.Form
$form.Text                = 'Bluetooth Guard'
$form.FormBorderStyle     = 'None'
$form.StartPosition       = 'CenterScreen'
$form.BackColor           = $Bg
$form.ForeColor           = $Txt
$form.Font                = $fontBd
$form.Icon                = $AppIcon
$form.ShowInTaskbar       = $true
$form.KeyPreview          = $true
$form.AutoScaleDimensions = New-Object System.Drawing.SizeF(96, 96)
$form.AutoScaleMode       = 'Dpi'
$form.ClientSize          = New-Object System.Drawing.Size(540, 660)

$bar = New-Object BtGuard.TitleBar $form
$bar.Text = 'Bluetooth Guard'
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
$CW = 500
$y  = 12

# Hero summary card
$cardHero = New-Object BtGuard.Card
$cardHero.Location = New-Object System.Drawing.Point($M, $y)
$cardHero.Size     = New-Object System.Drawing.Size($CW, 118)
$body.Controls.Add($cardHero)

$lblHero = New-Object System.Windows.Forms.Label
$lblHero.Text      = 'Stop Audio Hijacking'
$lblHero.Location  = New-Object System.Drawing.Point(16, 10)
$lblHero.Size      = New-Object System.Drawing.Size(340, 28)
$lblHero.ForeColor = $Txt
$lblHero.Font      = $fontH1
$lblHero.BackColor = [System.Drawing.Color]::Transparent
$cardHero.Controls.Add($lblHero)

$btnScan = New-Object BtGuard.FlatBtn
$btnScan.Text     = 'Refresh'
$btnScan.Location = New-Object System.Drawing.Point(($CW - 114), 11)
$btnScan.Size     = New-Object System.Drawing.Size(98, 32)
$btnScan.Font     = $fontSm
$cardHero.Controls.Add($btnScan)

$dotHero = New-Object System.Windows.Forms.Label
$dotHero.Text      = [char]0x25CF
$dotHero.Location  = New-Object System.Drawing.Point(16, 42)
$dotHero.Size      = New-Object System.Drawing.Size(16, 18)
$dotHero.ForeColor = [BtGuard.Theme]::Good
$dotHero.Font      = $fontSm
$dotHero.BackColor = [System.Drawing.Color]::Transparent
$cardHero.Controls.Add($dotHero)

$lblSummary = New-Object System.Windows.Forms.Label
$lblSummary.Text      = 'Scanning paired Bluetooth devices...'
$lblSummary.Location  = New-Object System.Drawing.Point(34, 42)
$lblSummary.Size      = New-Object System.Drawing.Size(450, 18)
$lblSummary.ForeColor = $Mut
$lblSummary.Font      = $fontSm
$lblSummary.BackColor = [System.Drawing.Color]::Transparent
$cardHero.Controls.Add($lblSummary)

$chkGuard = New-Object BtGuard.Toggle
$chkGuard.Text     = 'Actively block auto-connect for Manual Only devices'
$chkGuard.Location = New-Object System.Drawing.Point(16, 66)
$chkGuard.Size     = New-Object System.Drawing.Size(468, 24)
$chkGuard.Font     = $fontSm
$chkGuard.SetQuiet([bool]$script:Cfg.GuardEnabled)
$cardHero.Controls.Add($chkGuard)

$chkAutoStart = New-Object BtGuard.Toggle
$chkAutoStart.Text     = 'Start quietly with Windows (recommended)'
$chkAutoStart.Location = New-Object System.Drawing.Point(16, 91)
$chkAutoStart.Size     = New-Object System.Drawing.Size(468, 22)
$chkAutoStart.Font     = $fontSm
$chkAutoStart.SetQuiet((Test-Autostart))
$cardHero.Controls.Add($chkAutoStart)

$y += 118 + 14

$lblSection = New-Object System.Windows.Forms.Label
$lblSection.Text      = 'PAIRED BLUETOOTH DEVICES  (TURN OFF AUTO-CONNECT FOR PHONE EARBUDS)'
$lblSection.Location  = New-Object System.Drawing.Point($M, $y)
$lblSection.Size      = New-Object System.Drawing.Size($CW, 18)
$lblSection.ForeColor = $Mut
$lblSection.Font      = $fontLb
$lblSection.BackColor = [System.Drawing.Color]::Transparent
$body.Controls.Add($lblSection)
$y += 22

$pnlList = New-Object System.Windows.Forms.Panel
$pnlList.Location  = New-Object System.Drawing.Point($M, $y)
$pnlList.Size      = New-Object System.Drawing.Size($CW, 420)
$pnlList.BackColor = [System.Drawing.Color]::Transparent
$body.Controls.Add($pnlList)

$lblFooter = New-Object System.Windows.Forms.Label
$lblFooter.Text      = 'Tip: "Connect Now" lets you use blocked earbuds on your laptop and auto-locks them again afterward.'
$lblFooter.Location  = New-Object System.Drawing.Point($M, ($y + 428))
$lblFooter.Size      = New-Object System.Drawing.Size($CW, 20)
$lblFooter.ForeColor = $Mut
$lblFooter.Font      = $fontSm
$body.Controls.Add($lblFooter)

# ---------------------------------------------------------------- tray icon + menu
$icon = New-Object System.Windows.Forms.NotifyIcon
$icon.Icon    = $AppIcon
$icon.Text    = 'Bluetooth Guard'
$icon.Visible = $true

$menu = New-Object System.Windows.Forms.ContextMenuStrip
$menu.BackColor       = [BtGuard.Theme]::Panel2
$menu.ForeColor       = $Txt
$menu.Font            = $fontBd
$menu.ShowImageMargin = $false
$icon.ContextMenuStrip = $menu
$menu.Add_Opening({
    param($s, $e)
    if ($script:MenuPending) {
        $script:MenuPending = $false
        Rebuild-TrayMenu $script:LatestSorted $true
    }
    $e.Cancel = $false
})

function Show-Window {
    $form.Show()
    $form.WindowState = 'Normal'
    [void]$form.Activate()
    $form.BringToFront()
}

$icon.Add_MouseDoubleClick({ Show-Window })

$script:Quitting = $false

function Lock-ManualDevicesOnExit {
    if (-not $script:Cfg.GuardEnabled) { return }
    foreach ($p in $script:Cfg.Devices.PSObject.Properties) {
        $d = $p.Value
        if (-not [bool]$d.AllowAutoConnect -and $d.Address) {
            try { [void][BtGuard.BtNative]::BlockAndDisconnect([uint64]$d.Address, [string]$d.Kind) } catch { }
        }
    }
}

function Quit-App {
    $script:Quitting = $true
    try { $icon.Visible = $false; $icon.Dispose() } catch { }
    try { Lock-ManualDevicesOnExit } catch { }
    try { [BtGuard.PowerEvents]::Stop() } catch { }
    try { Queue-BtWork 'stop-worker' } catch { }
    try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }
    try { [System.Windows.Forms.Application]::Exit() } catch { }
}

$script:LatestSorted = @()
$script:MenuPending  = $false

function Rebuild-TrayMenu($devices, [bool]$force = $false) {
    $script:LatestSorted = @($devices)
    if ($menu.Visible -and -not $force) {
        $script:MenuPending = $true
        return
    }
    $menu.Items.Clear()

    $miTitle = New-Object System.Windows.Forms.ToolStripMenuItem
    $miTitle.Text    = if ($script:Cfg.GuardEnabled) { 'Bluetooth Guard - Active' } else { 'Bluetooth Guard - Paused' }
    $miTitle.Enabled = $false
    [void]$menu.Items.Add($miTitle)
    [void]$menu.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator))

    $miOpen = New-Object System.Windows.Forms.ToolStripMenuItem
    $miOpen.Text = 'Open Bluetooth Guard'
    $miOpen.Font = New-Object System.Drawing.Font('Segoe UI', 9.5, [System.Drawing.FontStyle]::Bold)
    $miOpen.Add_Click({ Show-Window })
    [void]$menu.Items.Add($miOpen)

    if ($devices -and $devices.Count -gt 0) {
        [void]$menu.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator))
        foreach ($snap in $devices) {
            $mac  = [string]$snap.Mac
            $addr = [uint64]$snap.Address
            $kind = [string]$snap.Kind
            $name = [string]$snap.Name
            $conn = [bool]$snap.Connected
            $mi   = New-Object System.Windows.Forms.ToolStripMenuItem
            if ($conn) {
                $mi.Text = ('Disconnect {0}' -f $name)
                $mi.Add_Click({
                    $script:SessionAllowed.Remove($mac)
                    $script:EnforcedIdle[$mac] = $true
                    Queue-BtWork 'block' $addr $kind $name
                }.GetNewClosure())
            } else {
                $mi.Text = ('Connect {0}' -f $name)
                $mi.Add_Click({
                    $script:SessionAllowed[$mac] = [DateTime]::UtcNow
                    $script:EnforcedIdle.Remove($mac)
                    Queue-BtWork 'connect' $addr $kind $name
                }.GetNewClosure())
            }
            [void]$menu.Items.Add($mi)
        }
    }

    [void]$menu.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator))
    $miLogs = New-Object System.Windows.Forms.ToolStripMenuItem
    $miLogs.Text = 'Open logs folder'
    $miLogs.Add_Click({ try { Start-Process explorer.exe $LogDir } catch { } })
    [void]$menu.Items.Add($miLogs)

    $miExit = New-Object System.Windows.Forms.ToolStripMenuItem
    $miExit.Text = 'Exit'
    $miExit.Add_Click({ Quit-App })
    [void]$menu.Items.Add($miExit)
}

Rebuild-TrayMenu @()

function Render-Devices($devices) {
    $pnlList.SuspendLayout()
    $pnlList.Controls.Clear()

    if (-not $devices -or $devices.Count -eq 0) {
        $empty = New-Object BtGuard.Card
        $empty.Location = New-Object System.Drawing.Point(0, 0)
        $empty.Size     = New-Object System.Drawing.Size($CW, 84)

        $lblE = New-Object System.Windows.Forms.Label
        if (-not $script:StateSync.RadioPresent) {
            $lblE.Text = 'Bluetooth is currently turned off or no Bluetooth adapter was detected.'
        } else {
            $lblE.Text = 'No paired Classic Bluetooth devices found yet. Pair your earbuds/headphones in Windows Settings first, then click Refresh.'
        }
        $lblE.Location  = New-Object System.Drawing.Point(16, 16)
        $lblE.Size      = New-Object System.Drawing.Size(($CW - 32), 52)
        $lblE.ForeColor = $Mut
        $lblE.Font      = $fontSm
        $lblE.BackColor = [System.Drawing.Color]::Transparent
        $empty.Controls.Add($lblE)
        $pnlList.Controls.Add($empty)
        $pnlList.Height = 96
        $lblFooter.Location = New-Object System.Drawing.Point($M, ($pnlList.Top + 104))
        $pnlList.ResumeLayout()
        Rebuild-TrayMenu @()
        return
    }

    # Sort Audio devices first, then connected first, then alphabetically.
    $sorted = @($devices | Sort-Object `
        @{ Expression = { if ($_.Kind -eq 'Audio') { 0 } else { 1 } } }, `
        @{ Expression = { if ($_.Connected) { 0 } else { 1 } } }, `
        @{ Expression = { $_.Name } })

    $ry = 0
    foreach ($snap in $sorted) {
        $entry   = Ensure-DeviceEntry $snap
        $mac     = [string]$snap.Mac
        $addr    = [uint64]$snap.Address
        $kind    = [string]$snap.Kind
        $name    = [string]$snap.Name
        $allow   = [bool]$entry.AllowAutoConnect
        $inSess  = $script:SessionAllowed.ContainsKey($mac)

        $card = New-Object BtGuard.Card
        $card.Location = New-Object System.Drawing.Point(0, $ry)
        $card.Size     = New-Object System.Drawing.Size($CW, 90)

        $lblName = New-Object System.Windows.Forms.Label
        $lblName.Text      = ('{0}   [{1}]' -f $name, $kind)
        $lblName.Location  = New-Object System.Drawing.Point(16, 11)
        $lblName.Size      = New-Object System.Drawing.Size(340, 22)
        $lblName.ForeColor = $Txt
        $lblName.Font      = $fontSub
        $lblName.BackColor = [System.Drawing.Color]::Transparent
        $card.Controls.Add($lblName)

        $lblState = New-Object System.Windows.Forms.Label
        $lblState.Location  = New-Object System.Drawing.Point(16, 34)
        $lblState.Size      = New-Object System.Drawing.Size(340, 18)
        $lblState.Font      = $fontSm
        $lblState.BackColor = [System.Drawing.Color]::Transparent
        if ($snap.Connected -and -not $allow -and $inSess) {
            $lblState.Text      = 'Connected for this session (re-locks on disconnect)'
            $lblState.ForeColor = [BtGuard.Theme]::Warn
        } elseif ($snap.Connected) {
            $lblState.Text      = 'Connected'
            $lblState.ForeColor = [BtGuard.Theme]::Good
        } elseif (-not $allow) {
            $lblState.Text      = 'Manual Only - auto-connect blocked (phone safe)'
            $lblState.ForeColor = [BtGuard.Theme]::Accent
        } else {
            $lblState.Text      = 'Disconnected - allowed to auto-connect'
            $lblState.ForeColor = $Mut
        }
        $card.Controls.Add($lblState)

        $tog = New-Object BtGuard.Toggle
        $tog.Text     = 'Allow auto-connect on startup / wake'
        $tog.Location = New-Object System.Drawing.Point(16, 56)
        $tog.Size     = New-Object System.Drawing.Size(340, 24)
        $tog.Font     = $fontSm
        $tog.SetQuiet($allow)
        $tog.Add_CheckedChanged({
            $want = [bool]$tog.Checked
            $entry.AllowAutoConnect = $want
            Save-Cfg
            if ($want) {
                $script:SessionAllowed.Remove($mac)
                $script:EnforcedIdle.Remove($mac)
                if (-not $snap.Connected) {
                    $lblState.Text      = 'Allowed to auto-connect'
                    $lblState.ForeColor = $Mut
                }
                Log ("allowed auto-connect: {0} ({1})" -f $name, $mac)
                Queue-BtWork 'connect' $addr $kind $name
            } else {
                $script:SessionAllowed.Remove($mac)
                $script:EnforcedIdle[$mac] = $true
                $lblState.Text      = 'Manual Only - auto-connect blocked (phone safe)'
                $lblState.ForeColor = [BtGuard.Theme]::Accent
                Log ("blocked auto-connect: {0} ({1})" -f $name, $mac)
                if ($script:Cfg.GuardEnabled) {
                    Queue-BtWork 'block' $addr $kind $name
                }
            }
        }.GetNewClosure())
        $card.Controls.Add($tog)

        $btn = New-Object BtGuard.FlatBtn
        $btn.Location = New-Object System.Drawing.Point(($CW - 134), 26)
        $btn.Size     = New-Object System.Drawing.Size(118, 36)
        $btn.Font     = $fontSm
        if ($snap.Connected) {
            $btn.Text   = 'Disconnect'
            $btn.Danger = $true
            $btn.Add_Click({
                $btn.Text           = 'Disconnecting...'
                $lblState.Text      = 'Disconnecting and locking auto-connect...'
                $lblState.ForeColor = [BtGuard.Theme]::Warn
                $script:SessionAllowed.Remove($mac)
                $script:EnforcedIdle[$mac] = $true
                Log ("manual disconnect: {0}" -f $name)
                Queue-BtWork 'block' $addr $kind $name
            }.GetNewClosure())
        } else {
            $btn.Text    = 'Connect Now'
            $btn.Primary = $true
            $btn.Add_Click({
                $btn.Text           = 'Connecting...'
                $lblState.Text      = 'Connecting for this session...'
                $lblState.ForeColor = [BtGuard.Theme]::Warn
                $script:SessionAllowed[$mac] = [DateTime]::UtcNow
                $script:EnforcedIdle.Remove($mac)
                Log ("session connect requested: {0}" -f $name)
                Queue-BtWork 'connect' $addr $kind $name
            }.GetNewClosure())
        }
        $card.Controls.Add($btn)

        $pnlList.Controls.Add($card)
        $ry += 98
    }

    $pnlList.Height = [Math]::Max(98, $ry)
    $lblFooter.Location = New-Object System.Drawing.Point($M, ($pnlList.Top + $pnlList.Height + 6))
    $pnlList.ResumeLayout()
    Rebuild-TrayMenu $sorted
}

function Enforce-Policies($devices, [bool]$ForceIdleRelock) {
    if (-not $script:Cfg.GuardEnabled) { return }
    foreach ($snap in $devices) {
        $entry = Ensure-DeviceEntry $snap
        $mac   = [string]$snap.Mac
        $allow = [bool]$entry.AllowAutoConnect
        if ($allow) { continue }

        if ($snap.Connected) {
            if ($script:SessionAllowed.ContainsKey($mac)) {
                # User intentionally clicked "Connect Now" on this laptop session.
                $script:SeenConnected[$mac] = $true
            } else {
                # Unsolicited auto-connection (boot, wake, radio on) -> drop immediately!
                Log ("dropping unsolicited auto-connect from {0} ({1})" -f $snap.Name, $mac) 'WARN'
                $script:EnforcedIdle[$mac] = $true
                Queue-BtWork 'block' ([uint64]$snap.Address) ([string]$snap.Kind) ([string]$snap.Name)
            }
        } else {
            # If user had connected for this session and has now turned off /
            # disconnected the earbuds (or >45s elapsed without ever linking),
            # re-lock the profiles automatically so next boot stays phone-safe!
            if ($script:SessionAllowed.ContainsKey($mac)) {
                $elapsed = ([DateTime]::UtcNow - $script:SessionAllowed[$mac]).TotalSeconds
                if ($script:SeenConnected.ContainsKey($mac) -or $elapsed -gt 45) {
                    Log ("session ended for {0} - re-locking auto-connect" -f $snap.Name)
                    $script:SessionAllowed.Remove($mac)
                    $script:SeenConnected.Remove($mac)
                    $script:EnforcedIdle[$mac] = $false
                }
            }
            if (-not $script:SessionAllowed.ContainsKey($mac) -and ($ForceIdleRelock -or -not $script:EnforcedIdle.ContainsKey($mac) -or -not $script:EnforcedIdle[$mac])) {
                $script:EnforcedIdle[$mac] = $true
                Queue-BtWork 'block' ([uint64]$snap.Address) ([string]$snap.Kind) ([string]$snap.Name)
            }
        }
    }
}

$btnScan.Add_Click({
    $lblSummary.Text = 'Refreshing paired Bluetooth devices...'
    Queue-BtWork 'refresh'
})

$chkGuard.Add_CheckedChanged({
    $script:Cfg.GuardEnabled = [bool]$chkGuard.Checked
    Save-Cfg
    if ($script:Cfg.GuardEnabled) {
        $dotHero.ForeColor = [BtGuard.Theme]::Good
        Enforce-Policies $script:StateSync.Snapshots $true
    } else {
        $dotHero.ForeColor = [BtGuard.Theme]::Warn
        $lblSummary.Text   = 'Guard paused - auto-connect blocking is temporarily disabled'
    }
})

$chkAutoStart.Add_CheckedChanged({
    [void](Set-Autostart ([bool]$chkAutoStart.Checked))
})

$script:HintShown = $false
function Hide-ToTray {
    $form.Hide()
    if (-not $script:HintShown) {
        $script:HintShown = $true
        $icon.BalloonTipTitle = 'Bluetooth Guard is running'
        $icon.BalloonTipText  = 'Right-click the tray icon any time to quick-connect or disconnect your earbuds.'
        $icon.ShowBalloonTip(3000)
    }
}

$bar.Add_CloseClicked({ Hide-ToTray })
$bar.Add_MinClicked({ Hide-ToTray })
$form.Add_FormClosing({
    param($s, $e)
    if ($e.CloseReason -eq [System.Windows.Forms.CloseReason]::UserClosing -and -not $script:Quitting) {
        $e.Cancel = $true
        Hide-ToTray
    }
})
$form.Add_Shown({
    try { [BtGuard.Win]::RoundCorners($form.Handle) } catch { }
    try { [BtGuard.Win]::SetIcon($form.Handle, $AppIcon.Handle) } catch { }
    try { Set-Content -Path $ReadyFile -Value $PID -Encoding ASCII -ErrorAction SilentlyContinue } catch { }
})

try { [BtGuard.PowerEvents]::Start() } catch { }

$script:LastSig = ''
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 450
$timer.Add_Tick({
    Update-Heartbeat
    if (Test-Path $ShowFile) {
        try { Remove-Item $ShowFile -Force -ErrorAction SilentlyContinue } catch { }
        Show-Window
    }

    $forceRelock = $false
    try {
        if ([BtGuard.PowerEvents]::ConsumeSuspends() -gt 0) {
            # Laptop sleeping: clear temporary session overrides so waking up
            # later never steals earbuds from your phone!
            $script:SessionAllowed.Clear()
            $script:SeenConnected.Clear()
            $forceRelock = $true
        }
        if ([BtGuard.PowerEvents]::ConsumeResumes() -gt 0) {
            $forceRelock = $true
            Queue-BtWork 'refresh'
        }
    } catch { }

    $snaps = @($script:StateSync.Snapshots)
    if ($script:StateSync.Dirty -or $forceRelock) {
        $script:StateSync.Dirty = $false
        Enforce-Policies $snaps $forceRelock

        $blockedTotal = 0
        foreach ($s in $snaps) {
            $e = Ensure-DeviceEntry $s
            if (-not [bool]$e.AllowAutoConnect) { $blockedTotal++ }
        }
        if (-not $script:Cfg.GuardEnabled) {
            $dotHero.ForeColor = [BtGuard.Theme]::Warn
            $lblSummary.Text   = 'Guard paused - Windows may auto-connect any device'
        } elseif (-not $script:StateSync.RadioPresent) {
            $dotHero.ForeColor = $Mut
            $lblSummary.Text   = 'Bluetooth radio is off - watching for Bluetooth to turn on'
        } else {
            $dotHero.ForeColor = [BtGuard.Theme]::Good
            $lblSummary.Text   = ('Guarding {0} paired device(s) - {1} set to Manual Only (phone safe)' -f $snaps.Count, $blockedTotal)
        }

        $sigParts = @()
        foreach ($s in $snaps) {
            $e = Ensure-DeviceEntry $s
            $sigParts += ('{0}:{1}:{2}:{3}' -f $s.Mac, $s.Connected, $e.AllowAutoConnect, $script:SessionAllowed.ContainsKey($s.Mac))
        }
        $sig = ($sigParts -join '|') + '|radio=' + $script:StateSync.RadioPresent
        if ($sig -ne $script:LastSig) {
            $script:LastSig = $sig
            Render-Devices $snaps
        }
    }
})
$timer.Start()

try {
    [System.Windows.Forms.Application]::add_ThreadException(
        [System.Threading.ThreadExceptionEventHandler]{
            param($s, $e)
            Log ("UI exception caught safely: {0}" -f $e.Exception.Message) 'ERROR'
        })
} catch { }

try {
    [AppDomain]::CurrentDomain.add_ProcessExit(
        [EventHandler]{
            param($s, $e)
            try { $icon.Visible = $false; $icon.Dispose() } catch { }
            try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }
        })
} catch { }

if (-not $Silent) { Show-Window } else { try { Set-Content -Path $ReadyFile -Value $PID -Encoding ASCII -ErrorAction SilentlyContinue } catch { } }
Log 'Bluetooth Guard ready'
[System.Windows.Forms.Application]::Run()
Quit-App

} catch {
    $report = @(
        ('Bluetooth Guard crashed at {0}' -f (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')),
        ('Error: {0}' -f $_.Exception.Message),
        ('Where: {0}' -f $_.InvocationInfo.PositionMessage),
        ('Stack: {0}' -f $_.ScriptStackTrace)
    ) -join "`r`n`r`n"
    try { Set-Content -Path $CrashFile -Value $report -Encoding UTF8 -ErrorAction SilentlyContinue } catch { }
    try { Remove-Item $LockFile -Force -ErrorAction SilentlyContinue } catch { }
    if (-not $Silent) {
        try {
            Add-Type -AssemblyName System.Windows.Forms -ErrorAction SilentlyContinue
            [void][System.Windows.Forms.MessageBox]::Show($report, 'Bluetooth Guard Error')
        } catch { }
    }
    exit 1
}
