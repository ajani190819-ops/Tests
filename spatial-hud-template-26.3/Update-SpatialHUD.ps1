# Update-SpatialHUD.ps1
#
# Maintained by Update-SpatialHUD.bat. The batch file is the only file a player
# needs to keep; it refreshes this helper from GitHub before every run.
#
# Default target: %APPDATA%\ModrinthApp\profiles\F5W\mods
# The target, release feed, and optional folder opener are remembered under
# %LOCALAPPDATA%\SpatialHudUpdater and can always be changed from the menu.

[CmdletBinding()]
param(
    [string]$TargetDirectory,
    [string]$ReleaseTag,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$Repository = 'ajani190819-ops/Tests'
$DefaultReleaseTag = 'spatial-hud-latest'
$DefaultTargetDirectory = Join-Path $env:APPDATA 'ModrinthApp\profiles\F5W\mods'
$StateDirectory = Join-Path $env:LOCALAPPDATA 'SpatialHudUpdater'
$TargetStateFile = Join-Path $StateDirectory 'target-directory.txt'
$ReleaseStateFile = Join-Path $StateDirectory 'release-tag.txt'
$FolderOpenerStateFile = Join-Path $StateDirectory 'folder-opener.txt'

function Read-RememberedValue {
    param([string]$Path, [string]$Fallback)

    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $value = (Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue).Trim()
        if ($value) {
            return $value
        }
    }
    return $Fallback
}

function Save-RememberedValue {
    param([string]$Path, [string]$Value)

    if (-not (Test-Path -LiteralPath $StateDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $StateDirectory -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Value.Trim(), [System.Text.Encoding]::UTF8)
}

function Test-ReleaseTag {
    param([string]$Tag)

    if ([string]::IsNullOrWhiteSpace($Tag) -or $Tag -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
        throw 'The release tag may contain only letters, numbers, dots, underscores, and hyphens.'
    }
}

function Resolve-InstallDirectory {
    param([string]$Path)

    $expanded = [Environment]::ExpandEnvironmentVariables($Path.Trim().Trim('"'))
    if ([string]::IsNullOrWhiteSpace($expanded)) {
        throw 'The mods folder cannot be empty.'
    }
    return [System.IO.Path]::GetFullPath($expanded)
}

function Get-ReleaseAsset {
    param([string]$Tag)

    Test-ReleaseTag $Tag
    $headers = @{ 'User-Agent' = 'SpatialHUD-Updater' }
    $release = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/$Repository/releases/tags/$Tag"
    $assets = @($release.assets | Where-Object { $_.name -match '^spatial-hud-.*\.jar$' })
    if ($assets.Count -ne 1) {
        throw "Release '$Tag' does not contain exactly one Spatial HUD JAR. Nothing was changed."
    }

    $asset = $assets[0]
    if (-not $asset.browser_download_url -or $asset.size -lt 1024) {
        throw "Release '$Tag' has an invalid Spatial HUD asset. Nothing was changed."
    }

    return [PSCustomObject]@{
        Tag = $Tag
        Name = [string]$asset.name
        Url = [string]$asset.browser_download_url
        Size = [Int64]$asset.size
        UpdatedAt = [string]$asset.updated_at
        ReleaseName = [string]$release.name
    }
}

function Get-SpatialHudJarId {
    param([string]$JarPath)

    # A name match is never trusted for deletion. Only a Fabric JAR whose own
    # fabric.mod.json identifies it as spatialhud can be replaced.
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        $archive = [System.IO.Compression.ZipFile]::OpenRead($JarPath)
        try {
            $entry = $archive.GetEntry('fabric.mod.json')
            if ($null -eq $entry) {
                return $null
            }
            $reader = New-Object System.IO.StreamReader($entry.Open())
            try {
                $metadata = $reader.ReadToEnd() | ConvertFrom-Json
                return [string]$metadata.id
            }
            finally {
                $reader.Dispose()
            }
        }
        finally {
            $archive.Dispose()
        }
    }
    catch {
        # Another mod may use a malformed/non-ZIP .jar. It is not ours and is
        # deliberately left alone.
        return $null
    }
}

function Test-SpatialHudJar {
    param([string]$JarPath)

    $item = Get-Item -LiteralPath $JarPath
    if ($item.Length -lt 1024) {
        throw "Downloaded file is unexpectedly small ($($item.Length) bytes)."
    }

    $id = Get-SpatialHudJarId $JarPath
    if ($id -ne 'spatialhud') {
        throw 'Downloaded file is not a Spatial HUD Fabric JAR. Nothing was changed.'
    }
}

function Get-InstalledSpatialHudJars {
    param([string]$Directory)

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return @()
    }

    return @(Get-ChildItem -LiteralPath $Directory -File -Filter '*.jar' |
        Where-Object { (Get-SpatialHudJarId $_.FullName) -eq 'spatialhud' })
}

function Show-FeedDetails {
    param([string]$Tag)

    Write-Host ''
    Write-Host 'Checking the current release feed...' -ForegroundColor Cyan
    $asset = Get-ReleaseAsset $Tag
    Write-Host "  Feed:     $($asset.Tag)"
    Write-Host "  Release:  $($asset.ReleaseName)"
    Write-Host "  Asset:    $($asset.Name) ($([Math]::Round($asset.Size / 1KB, 1)) KB)"
    Write-Host "  Updated:  $($asset.UpdatedAt)"
}

function Set-InstallDirectory {
    param([string]$CurrentDirectory)

    Write-Host ''
    Write-Host 'Modrinth mods folder' -ForegroundColor Cyan
    Write-Host "Current: $CurrentDirectory"
    Write-Host 'Paste a different folder, or press Enter to keep the current folder.'
    $entered = Read-Host 'Folder'
    if ([string]::IsNullOrWhiteSpace($entered)) {
        return $CurrentDirectory
    }

    $newDirectory = Resolve-InstallDirectory $entered
    if (-not (Test-Path -LiteralPath $newDirectory -PathType Container)) {
        $create = Read-Host "'$newDirectory' does not exist. Create it? [Y/n]"
        if ($create -and $create -notmatch '^[Yy]') {
            return $CurrentDirectory
        }
        New-Item -ItemType Directory -Path $newDirectory -Force | Out-Null
    }

    Save-RememberedValue $TargetStateFile $newDirectory
    Write-Host "Saved target: $newDirectory" -ForegroundColor Green
    return $newDirectory
}

function Set-ReleaseFeed {
    param([string]$CurrentTag)

    Write-Host ''
    Write-Host 'Release feed' -ForegroundColor Cyan
    Write-Host "Current: $CurrentTag"
    Write-Host "The default '$DefaultReleaseTag' is the newest successful Spatial HUD build."
    Write-Host 'Enter another published GitHub release tag, or press Enter to keep the current feed.'
    $entered = Read-Host 'Release tag'
    if ([string]::IsNullOrWhiteSpace($entered)) {
        return $CurrentTag
    }

    $newTag = $entered.Trim()
    Test-ReleaseTag $newTag
    # Verify before remembering a custom channel; a typo is never saved.
    $null = Get-ReleaseAsset $newTag
    Save-RememberedValue $ReleaseStateFile $newTag
    Write-Host "Saved release feed: $newTag" -ForegroundColor Green
    return $newTag
}

function Get-OneCommanderExecutable {
    # OneCommander can be installed per-user, machine-wide, or be available on
    # PATH. Try the common locations first; a custom executable path remains
    # available in the menu when a portable/MS Store layout is used instead.
    $candidates = @()
    if ($env:LOCALAPPDATA) { $candidates += (Join-Path $env:LOCALAPPDATA 'Programs\OneCommander\OneCommander.exe') }
    if ($env:LOCALAPPDATA) { $candidates += (Join-Path $env:LOCALAPPDATA 'OneCommander\OneCommander.exe') }
    if ($env:ProgramFiles) { $candidates += (Join-Path $env:ProgramFiles 'OneCommander\OneCommander.exe') }
    if (${env:ProgramFiles(x86)}) { $candidates += (Join-Path ${env:ProgramFiles(x86)} 'OneCommander\OneCommander.exe') }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $onPath = Get-Command 'OneCommander.exe' -ErrorAction SilentlyContinue
    if ($onPath -and (Test-Path -LiteralPath $onPath.Source -PathType Leaf)) {
        return $onPath.Source
    }

    foreach ($registryPath in @(
        'HKCU:\Software\Microsoft\Windows\CurrentVersion\App Paths\OneCommander.exe',
        'HKLM:\Software\Microsoft\Windows\CurrentVersion\App Paths\OneCommander.exe'
    )) {
        $key = Get-Item -LiteralPath $registryPath -ErrorAction SilentlyContinue
        if ($key) {
            $registered = [string]$key.GetValue('')
            if ($registered -and (Test-Path -LiteralPath $registered -PathType Leaf)) {
                return $registered
            }
        }
    }
    return $null
}

function Get-FolderOpenerLabel {
    param([string]$Executable)

    if ([string]::IsNullOrWhiteSpace($Executable)) {
        return 'Disabled (do not open a folder after install)'
    }
    return "$(Split-Path -Leaf $Executable): $Executable"
}

function Set-FolderOpener {
    param([string]$CurrentExecutable)

    Write-Host ''
    Write-Host 'Optional post-install folder opener' -ForegroundColor Cyan
    Write-Host "Current: $(Get-FolderOpenerLabel $CurrentExecutable)"
    Write-Host ''
    Write-Host ' [1] Disabled - do not open any folder after installing (default)'
    Write-Host ' [2] Use OneCommander - find it automatically'
    Write-Host ' [3] Use another file manager - paste its .exe path'
    Write-Host ' [M] Keep the current setting'
    $choice = Read-Host 'Choice'

    switch -Regex ($choice) {
        '^1$' {
            Save-RememberedValue $FolderOpenerStateFile ''
            Write-Host 'Folder opening disabled.' -ForegroundColor Green
            return ''
        }
        '^2$' {
            $oneCommander = Get-OneCommanderExecutable
            if (-not $oneCommander) {
                Write-Host 'OneCommander was not found automatically. Use option 3 to paste OneCommander.exe.' -ForegroundColor Yellow
                return $CurrentExecutable
            }
            Save-RememberedValue $FolderOpenerStateFile $oneCommander
            Write-Host "OneCommander saved: $oneCommander" -ForegroundColor Green
            return $oneCommander
        }
        '^3$' {
            $entered = Read-Host 'Full path to the file manager .exe'
            if ([string]::IsNullOrWhiteSpace($entered)) {
                return $CurrentExecutable
            }
            $executable = [Environment]::ExpandEnvironmentVariables($entered.Trim().Trim('"'))
            if (-not (Test-Path -LiteralPath $executable -PathType Leaf) -or [System.IO.Path]::GetExtension($executable) -notmatch '^\.exe$') {
                Write-Host 'That is not an existing .exe file. The current setting was kept.' -ForegroundColor Yellow
                return $CurrentExecutable
            }
            $executable = [System.IO.Path]::GetFullPath($executable)
            Save-RememberedValue $FolderOpenerStateFile $executable
            Write-Host "Folder opener saved: $executable" -ForegroundColor Green
            return $executable
        }
        '^[Mm]$' { return $CurrentExecutable }
        default {
            Write-Host 'That is not a menu choice. The current setting was kept.' -ForegroundColor Yellow
            return $CurrentExecutable
        }
    }
}

function Open-InstallFolder {
    param([string]$Directory, [string]$Executable)

    if ([string]::IsNullOrWhiteSpace($Executable)) {
        return
    }
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
        Write-Host "The configured folder opener no longer exists, so no folder was opened: $Executable" -ForegroundColor Yellow
        return
    }
    try {
        Start-Process -FilePath $Executable -ArgumentList @("`"$Directory`"") -ErrorAction Stop
    }
    catch {
        # An optional convenience must never turn a successful install into a
        # failure or trigger rollback of the new verified JAR.
        Write-Host "Spatial HUD installed, but the configured folder opener could not start: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

function Install-SpatialHud {
	param([string]$Directory, [string]$Tag, [string]$FolderOpener)

    $Directory = Resolve-InstallDirectory $Directory
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    }

    Write-Host ''
    Write-Host 'Spatial HUD installer' -ForegroundColor Cyan
    Write-Host "  Target: $Directory"
    $asset = Get-ReleaseAsset $Tag
    Write-Host "  Feed:   $($asset.Tag)"
    Write-Host "  Build:  $($asset.Name), updated $($asset.UpdatedAt)"

    $workDirectory = Join-Path $env:TEMP ("SpatialHudUpdater-" + [Guid]::NewGuid().ToString('N'))
    $downloadPath = Join-Path $workDirectory $asset.Name
    $backupDirectory = Join-Path $workDirectory 'previous-spatialhud'
    $moved = @()
    $installed = $false

    try {
        New-Item -ItemType Directory -Path $workDirectory -Force | Out-Null
        Write-Host 'Downloading and verifying the new JAR...' -ForegroundColor Cyan
        Invoke-WebRequest -Uri $asset.Url -OutFile $downloadPath
        Test-SpatialHudJar $downloadPath

        New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
        $existing = Get-InstalledSpatialHudJars $Directory
        foreach ($jar in $existing) {
            $backupPath = Join-Path $backupDirectory ([Guid]::NewGuid().ToString('N') + '.jar')
            Move-Item -LiteralPath $jar.FullName -Destination $backupPath -ErrorAction Stop
            $moved += [PSCustomObject]@{ Original = $jar.FullName; Backup = $backupPath }
        }

        $destination = Join-Path $Directory $asset.Name
        Move-Item -LiteralPath $downloadPath -Destination $destination -ErrorAction Stop
        $installed = $true

        # The old copies are removed only after the verified replacement is in
        # the profile mods folder. Other mods are never inspected or moved.
        Remove-Item -LiteralPath $backupDirectory -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host ''
        Write-Host "Installed: $destination" -ForegroundColor Green
        if ($existing.Count -gt 0) {
            Write-Host "Replaced $($existing.Count) prior Spatial HUD copy/copies."
        }

        # Opening a folder is an optional user preference. Default is no
        # opener, and OneCommander/custom file managers are supported without
        # invoking Windows Explorer.
        Open-InstallFolder $Directory $FolderOpener
    }
    catch {
        if (-not $installed) {
            foreach ($entry in $moved) {
                if (Test-Path -LiteralPath $entry.Backup) {
                    Move-Item -LiteralPath $entry.Backup -Destination $entry.Original -Force -ErrorAction SilentlyContinue
                }
            }
        }
        throw
    }
    finally {
        Remove-Item -LiteralPath $workDirectory -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$rememberedTarget = if ($TargetDirectory) { Resolve-InstallDirectory $TargetDirectory } else { Read-RememberedValue $TargetStateFile $DefaultTargetDirectory }
$rememberedTag = if ($ReleaseTag) { $ReleaseTag.Trim() } else { Read-RememberedValue $ReleaseStateFile $DefaultReleaseTag }
$rememberedFolderOpener = Read-RememberedValue $FolderOpenerStateFile ''
Test-ReleaseTag $rememberedTag

try {
    if ($Install) {
        Install-SpatialHud $rememberedTarget $rememberedTag $rememberedFolderOpener
        exit 0
    }

    while ($true) {
        Clear-Host
        Write-Host '==============================================================='
        Write-Host ' Spatial HUD -- Modrinth install / update'
        Write-Host '==============================================================='
        Write-Host " Folder:  $rememberedTarget"
        Write-Host " Feed:    $rememberedTag"
        Write-Host " Opener:  $(Get-FolderOpenerLabel $rememberedFolderOpener)"
        Write-Host '==============================================================='
        Write-Host ''
        Write-Host ' [1] Install or update Spatial HUD now'
        Write-Host ' [2] Change the Modrinth mods folder'
        Write-Host ' [3] Choose the published build / release feed'
        Write-Host ' [4] Configure optional folder opener (OneCommander / none)'
        Write-Host ' [5] Check the current build details'
        Write-Host ' [6] Restore the default folder, feed, and no-opener setting'
        Write-Host ' [Q] Quit'
        Write-Host ''
        $choice = Read-Host 'Choice, or Enter to install'

        if ([string]::IsNullOrWhiteSpace($choice) -or $choice -eq '1') {
            Install-SpatialHud $rememberedTarget $rememberedTag $rememberedFolderOpener
            Read-Host 'Press Enter to return to the menu' | Out-Null
            continue
        }
        switch -Regex ($choice) {
            '^2$' { $rememberedTarget = Set-InstallDirectory $rememberedTarget; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^3$' { $rememberedTag = Set-ReleaseFeed $rememberedTag; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^4$' { $rememberedFolderOpener = Set-FolderOpener $rememberedFolderOpener; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^5$' { Show-FeedDetails $rememberedTag; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^6$' {
                $rememberedTarget = $DefaultTargetDirectory
                $rememberedTag = $DefaultReleaseTag
                $rememberedFolderOpener = ''
                Remove-Item -LiteralPath $TargetStateFile, $ReleaseStateFile, $FolderOpenerStateFile -Force -ErrorAction SilentlyContinue
                Write-Host 'Restored the default folder, feed, and no-opener setting.' -ForegroundColor Green
                Read-Host 'Press Enter to continue' | Out-Null
                continue
            }
            '^[Qq]$' { exit 0 }
            default { Write-Host 'That is not a menu choice.' -ForegroundColor Yellow; Start-Sleep -Seconds 1 }
        }
    }
}
catch {
    Write-Host ''
    Write-Host "Spatial HUD was not changed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
