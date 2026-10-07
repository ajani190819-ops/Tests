# Update-SpatialHUD.ps1
#
# Maintained by Update-SpatialHUD.bat. The batch file is the only file a player
# needs to keep; it refreshes this helper from GitHub before every run.
#
# Default target: %APPDATA%\ModrinthApp\profiles\F5W\mods
# The target and release feed are remembered under %LOCALAPPDATA%\SpatialHudUpdater
# and can always be changed from the menu.

[CmdletBinding()]
param(
    [string]$TargetDirectory,
    [string]$ReleaseTag,
    [switch]$Install,
    [switch]$OpenFolder
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$Repository = 'ajani190819-ops/Tests'
$DefaultReleaseTag = 'spatial-hud-latest'
$DefaultTargetDirectory = Join-Path $env:APPDATA 'ModrinthApp\profiles\F5W\mods'
$StateDirectory = Join-Path $env:LOCALAPPDATA 'SpatialHudUpdater'
$TargetStateFile = Join-Path $StateDirectory 'target-directory.txt'
$ReleaseStateFile = Join-Path $StateDirectory 'release-tag.txt'

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

function Install-SpatialHud {
    param([string]$Directory, [string]$Tag, [switch]$SelectResult)

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

        if ($SelectResult) {
            Start-Process explorer.exe -ArgumentList "/select,`"$destination`""
        }
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
Test-ReleaseTag $rememberedTag

try {
    if ($Install) {
        Install-SpatialHud $rememberedTarget $rememberedTag -SelectResult:$OpenFolder
        exit 0
    }

    while ($true) {
        Clear-Host
        Write-Host '==============================================================='
        Write-Host ' Spatial HUD -- Modrinth install / update'
        Write-Host '==============================================================='
        Write-Host " Folder:  $rememberedTarget"
        Write-Host " Feed:    $rememberedTag"
        Write-Host '==============================================================='
        Write-Host ''
        Write-Host ' [1] Install or update Spatial HUD now'
        Write-Host ' [2] Change the Modrinth mods folder'
        Write-Host ' [3] Change the release feed'
        Write-Host ' [4] Check the current feed details'
        Write-Host ' [5] Restore the default folder and feed'
        Write-Host ' [Q] Quit'
        Write-Host ''
        $choice = Read-Host 'Choice, or Enter to install'

        if ([string]::IsNullOrWhiteSpace($choice) -or $choice -eq '1') {
            Install-SpatialHud $rememberedTarget $rememberedTag -SelectResult
            Read-Host 'Press Enter to return to the menu' | Out-Null
            continue
        }
        switch -Regex ($choice) {
            '^2$' { $rememberedTarget = Set-InstallDirectory $rememberedTarget; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^3$' { $rememberedTag = Set-ReleaseFeed $rememberedTag; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^4$' { Show-FeedDetails $rememberedTag; Read-Host 'Press Enter to continue' | Out-Null; continue }
            '^5$' {
                $rememberedTarget = $DefaultTargetDirectory
                $rememberedTag = $DefaultReleaseTag
                Remove-Item -LiteralPath $TargetStateFile, $ReleaseStateFile -Force -ErrorAction SilentlyContinue
                Write-Host 'Restored the default folder and release feed.' -ForegroundColor Green
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
