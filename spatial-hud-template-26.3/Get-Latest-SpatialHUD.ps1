# Get-Latest-SpatialHUD.ps1
#
# SpatialHUD-Helper-Version: 1
#
# Downloads the newest successful Spatial HUD build to this Windows account's
# Downloads folder. It keeps one predictable jar name and removes only older
# Spatial HUD jars after the new download has passed a basic jar-file check.
# It does not modify a Minecraft or Modrinth instance.

[CmdletBinding()]
param(
    [switch]$OpenFolder
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$ReleaseUrl = 'https://github.com/ajani190819-ops/Tests/releases/download/spatial-hud-latest/spatial-hud-1.0.0.jar'
$DownloadsFolder = Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Downloads'
$Destination = Join-Path $DownloadsFolder 'spatial-hud-1.0.0.jar'
$Temporary = Join-Path $DownloadsFolder 'spatial-hud-1.0.0.jar.download'

try {
    if (-not (Test-Path -LiteralPath $DownloadsFolder -PathType Container)) {
        New-Item -ItemType Directory -Path $DownloadsFolder -Force | Out-Null
    }

    # Never remove a working jar before a complete replacement is available.
    Remove-Item -LiteralPath $Temporary -Force -ErrorAction SilentlyContinue
    Write-Host "Downloading the latest Spatial HUD build..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $ReleaseUrl -OutFile $Temporary

    $download = Get-Item -LiteralPath $Temporary
    if ($download.Length -lt 1024) {
        throw "The download is unexpectedly small ($($download.Length) bytes)."
    }

    # JARs are ZIP files and begin with the PK signature. This avoids replacing
    # a working mod with an HTML error page if the release URL ever changes.
    $header = [System.IO.File]::ReadAllBytes($Temporary)
    if ($header.Length -lt 4 -or $header[0] -ne 0x50 -or $header[1] -ne 0x4B) {
        throw 'The download is not a valid JAR/ZIP file.'
    }

    # Clear previous copies of this mod only, including browser-created names
    # such as "spatial-hud-1.0.0 (1).jar". Spatial GUI and every other mod are
    # intentionally outside this pattern.
    Get-ChildItem -LiteralPath $DownloadsFolder -File -Filter 'spatial-hud*.jar' |
        Remove-Item -Force

    Move-Item -LiteralPath $Temporary -Destination $Destination -Force
    Write-Host "Done: $Destination" -ForegroundColor Green
    Write-Host 'In the Modrinth App, open your instance, choose Mods -> Add content -> From file, and select this jar.'

    if ($OpenFolder) {
        Start-Process explorer.exe -ArgumentList "/select,`"$Destination`""
    }
}
catch {
    Remove-Item -LiteralPath $Temporary -Force -ErrorAction SilentlyContinue
    Write-Error "Spatial HUD was not changed: $($_.Exception.Message)"
    exit 1
}
