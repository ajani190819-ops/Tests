# Isolated production PowerShell adapters: no devices, UI, vendor software,
# owner files or engine threads. Run with stock Windows PowerShell 5.1 in CI.
param([string]$Root = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
function Assert([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}
function Import-Functions([string]$path, [string[]]$names) {
    $tokens = $null; $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
    Assert (-not $errors) ('parse failed: ' + $path)
    foreach ($name in $names) {
        $matches = @($ast.FindAll({ param($n)
            $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name
        }.GetNewClosure(), $true))
        Assert ($matches.Count -eq 1) ('missing/duplicate function: ' + $name)
        # Dot-source only this actual function, not the script's device startup.
        . ([scriptblock]::Create($matches[0].Extent.Text.Replace('function ' + $name, 'function script:' + $name)))
    }
}
Import-Functions (Join-Path $Root 'keyboard-lighting/Aura-Background.ps1') @(
    'Set-StartupPreferences','Seed-Zone','Parse-Widths','Apply-LiveTheme',
    'Sync-Live','Publish-Live','Set-Live')
Import-Functions (Join-Path $Root 'keyboard-lighting/Tray.ps1') @(
    'New-GroupCfg','Fix-Widths','Copy-PanelObject','Merge-PanelObject',
    'Write-PanelAtomic','Get-PanelStamp','Load-Cfg','Save-Cfg',
    'Cur','Sync-Link','Sync-CfgFromUi','Load-UiFromCfg')
function Log { param($message, $level) }
function Sync-Sources2 { param($audio, $screen) }
function Set-KbdBacklight { param($level); return $true }
function Redraw-Swatches { }
function Update-EffectInfo { }
# Minimal controls exercise the actual UI adapters without loading WinForms.
function New-TestToggle {
    $control = [pscustomobject]@{Checked=$false}
    $control | Add-Member ScriptMethod SetQuiet { param($value); $this.Checked=[bool]$value }
    return $control
}
$script:Effects = [ordered]@{'Scrolling gradient'='gradient';'Rainbow'='rainbow'}
$script:Tab = 'Kbd'
$chkSmooth=New-TestToggle; $chkEq=New-TestToggle; $chkLink=New-TestToggle
$chkOverlay=New-TestToggle; $chkMirror=New-TestToggle; $chkReverse=New-TestToggle
$chkLoop=New-TestToggle; $chkOn=New-TestToggle
$trkMaster=[pscustomobject]@{Value=100}; $trkSpeed=[pscustomobject]@{Value=10}
$trkBright=[pscustomobject]@{Value=100}
$valMB=[pscustomobject]@{Text=''}; $valSpeed=[pscustomobject]@{Text=''}; $valBright=[pscustomobject]@{Text=''}
$cboEffect=[pscustomobject]@{SelectedItem='Scrolling gradient'}
$cboEffect | Add-Member ScriptMethod SetQuiet { param($index); $this.SelectedItem=@($script:Effects.Keys)[$index] }
function New-Zone {
    return [pscustomobject]@{
        Idx=@(0); Effect='gradient'; Speed=1.0; Brightness=1.0; Mirror=$false; Reverse=$false
        Equalise=$true; Enabled=$true; Loop=$false; SoftBlend=$true; Master=1.0
        PosLoop=@(0); PosAcross=@(0); Pos=@(0); PalR=@(0,1); PalG=@(0,1); PalB=@(0,1); PalW=@(1,1)
        LiveMaster=565; LiveDirty=$false; LiveEffect='gradient'; LiveSpeedMilli=1000
        LiveFlags=0; LiveEnabled=1; LiveBrightness=1000; LivePal=$null
    }
}
$folder = Join-Path ([IO.Path]::GetTempPath()) ('lighting-state-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($folder)
try {
    $liveFile = Join-Path $folder 'live.txt'
    $CfgFile = Join-Path $folder 'panel.json'
    $script:GateReassertAt = [DateTime]::UtcNow.AddDays(1)
    $script:LiveWritePending = $false
    $NativeBrightness = $true
    $palR = $palG = $palB = @(0,1)
    foreach ($smooth in @($false,$true)) {
        foreach ($kbd in @($false,$true)) {
            foreach ($bar in @($false,$true)) {
                $z0=New-Zone; $z1=New-Zone
                $eng=[pscustomobject]@{Groups=@($z0,$z1);DitherMode=-1}
                $eng | Add-Member ScriptMethod PinGate { param($level); return 0 }
                $steady = if ($smooth) {'on'} else {'off'}
                $ke = if ($kbd) {'on'} else {'off'}
                $be = if ($bar) {'on'} else {'off'}
                Seed-Zone $z0 gradient 1.0 0.77 $false $false $ke across $null $true '1,1'
                Seed-Zone $z1 rainbow 1.0 0.23 $false $false $be loop $null $false '1,1'
                Set-StartupPreferences $eng @($z0,$z1) $steady
                Assert ($z0.Equalise -eq $kbd -and $z1.Equalise -eq $bar) 'startup Balance restored incorrectly'
                Assert ($z0.SoftBlend -eq $smooth -and $z1.SoftBlend -eq $smooth) 'startup Steady restored incorrectly'
                Assert ($eng.DitherMode -eq $(if($smooth){0}else{1})) 'startup dither ignored saved false'
                $script:LiveLevel=565; $script:LastOnLevel=565
                $theme = [pscustomobject]@{Brightness=0.9;Smooth=$smooth
                    Kbd=[pscustomobject]@{Equalise=$kbd;On=$true;Brightness=0.77}
                    Bar=[pscustomobject]@{Equalise=$bar;On=$false;Brightness=0.23}}
                Apply-LiveTheme $theme
                Assert (($z0.LiveFlags -band 4) -eq $(if($kbd){4}else{0})) 'live Kbd Balance lost false'
                Assert (($z1.LiveFlags -band 4) -eq $(if($bar){4}else{0})) 'live Bar Balance lost false'
                Assert (($z0.LiveFlags -band 16) -eq $(if($smooth){16}else{0})) 'live Steady lost false'
                Assert ($script:LiveLevel -eq 565 -and $z0.LiveMaster -eq 565) 'a stale theme overwrote native master'
                Assert ($z0.LiveBrightness -eq 770 -and $z1.LiveBrightness -eq 230) 'zone brightness was conflated with master'
                Assert ($z0.LiveEnabled -eq 1 -and $z1.LiveEnabled -eq 0) 'zone enabled states were conflated'

                # Close/reopen all eight saved combinations through actual legacy functions.
                $script:Cfg=[pscustomobject]@{Brightness=50;Overlay=$true;Link=$false;OnExit='white';Smooth=$true
                    Custom=@(16777215,123,16777215,456)
                    Kbd=(New-GroupCfg 'Scrolling gradient' @('#FF0000','#00FF00') $false)
                    Bar=(New-GroupCfg 'Rainbow' @('#123456','#ABCDEF') $true)}
                $script:Cfg.Smooth=$smooth; $script:Cfg.Kbd.Equalise=$kbd; $script:Cfg.Bar.Equalise=$bar
                Write-PanelAtomic $CfgFile $script:Cfg
                Load-Cfg
                $script:Cfg.Kbd.Speed=24;Save-Cfg
                $script:Cfg.Smooth=-not $smooth; $script:Cfg.Kbd.Equalise=-not $kbd; $script:Cfg.Bar.Equalise=-not $bar
                Load-Cfg
                Assert ($script:Cfg.Smooth -eq $smooth -and $script:Cfg.Kbd.Equalise -eq $kbd -and $script:Cfg.Bar.Equalise -eq $bar) 'legacy restart lost saved combinations'
                Assert ($script:Cfg.Custom[1] -eq 123 -and $script:Cfg.Custom[3] -eq 456) 'custom slot positions changed'
                Load-UiFromCfg
                $external=Copy-PanelObject $script:Cfg
                $external.Smooth=-not $smooth; $external.Kbd.Equalise=-not $kbd; $external.Bar.Equalise=-not $bar
                $external.Brightness=57; $external.Overlay=$false
                $external | Add-Member NoteProperty MasterLevel 565
                $external | Add-Member NoteProperty UnknownRoot 99
                $external.Kbd | Add-Member NoteProperty UnknownZone 'keep'
                Write-PanelAtomic $CfgFile $external
                $script:Cfg.Kbd.Speed=31;Save-Cfg
                $saved=Get-Content -LiteralPath $CfgFile -Raw | ConvertFrom-Json
                Assert ($saved.Smooth -eq (-not $smooth) -and $saved.Kbd.Equalise -eq (-not $kbd) -and $saved.Bar.Equalise -eq (-not $bar)) 'stale tray save overwrote external preferences'
                Assert ($saved.Kbd.Speed -eq 31 -and $saved.UnknownRoot -eq 99 -and $saved.Kbd.UnknownZone -eq 'keep') 'three-way merge lost local edit or unknown settings'
                Assert ($chkSmooth.Checked -eq (-not $smooth) -and $chkEq.Checked -eq (-not $kbd)) 'external preferences did not refresh existing controls'
                Assert ($trkMaster.Value -eq 57 -and -not $chkOverlay.Checked) 'external master/overlay left stale controls'
                # An unrelated later UI apply must not put old controls back.
                Sync-CfgFromUi; Save-Cfg
                $saved=Get-Content -LiteralPath $CfgFile -Raw | ConvertFrom-Json
                Assert ($saved.Smooth -eq (-not $smooth) -and $saved.Kbd.Equalise -eq (-not $kbd) -and $saved.Bar.Equalise -eq (-not $bar)) 'later UI save restored stale checkboxes'
                Assert ($saved.Brightness -eq 57 -and $saved.MasterLevel -eq 565) 'later UI save rounded or reverted external master'
            }
        }
    }
    # Exact master channel: apply both directions and half-percent steps, no echo.
    foreach ($level in @(500,450,440,430,500,550,565,580,0,1000)) {
        [IO.File]::WriteAllText($liveFile, [string]$level)
        Sync-Live
        Assert ($script:LiveLevel -eq $level -and $eng.Groups[0].LiveMaster -eq $level -and $eng.Groups[1].LiveMaster -eq $level) 'native master did not reach both engine groups'
        Assert ([IO.File]::ReadAllText($liveFile) -ceq [string]$level) 'engine echoed/quantized native input'
    }
    foreach ($bad in @('', '12x', '-1', '1001', '99999999999999999999')) {
        [IO.File]::WriteAllText($liveFile,$bad);Sync-Live
        Assert ($script:LiveLevel -eq 1000) 'malformed master sample was accepted'
    }
    [IO.File]::WriteAllText($liveFile,'565');Sync-Live
    Assert ($script:LastOnLevel -eq 565) 'Fn toggle forgot native last-on level'
    Set-Live ($script:LiveLevel - 50)
    Assert ([int][IO.File]::ReadAllText($liveFile) -eq 515 -and $eng.Groups[0].LiveMaster -eq 515) 'Fn relative action used stale master'
    $NativeBrightness=$false
    Apply-LiveTheme ([pscustomobject]@{Brightness=0.42;Smooth=$false;Equalise=$false})
    Assert ($script:LiveLevel -eq 420 -and [int][IO.File]::ReadAllText($liveFile) -eq 420) 'legacy theme did not update canonical master'
    [IO.File]::WriteAllText($CfgFile,'{"Smooth":false,"Kbd":')
    Save-Cfg
    Assert ([IO.File]::ReadAllText($CfgFile) -ceq '{"Smooth":false,"Kbd":') 'legacy save destroyed a malformed file'
    'Lighting PowerShell startup/live/legacy-persistence regressions passed'
} finally {
    [IO.Directory]::Delete($folder,$true)
}
