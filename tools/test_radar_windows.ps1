#requires -Version 5.1
<#
.SYNOPSIS
Run the opt-in Weather Report native component review, not desktop acceptance.
.DESCRIPTION
Requires Python and ziglang==0.17.1 already installed. OutputDirectory must be
NEW, local and outside the checkout. Default fixture mode refuses HTTP. -Live
explicitly enables public forecast/tile requests for two fixed cities. No
uploads, app replacement, elevation or script/security policy changes.
If policy or security blocks this review, stop; do not bypass the block.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [switch]$Live
)
$ErrorActionPreference = 'Stop'
$runner = Join-Path $PSScriptRoot 'test_radar_windows.py'
$reviewArgs = @($runner, '--output-directory', $OutputDirectory)
if ($Live) { $reviewArgs += '--live' }
& python @reviewArgs
if ($LASTEXITCODE -ne 0) {
    throw 'Weather component review failed or is incomplete. No desktop acceptance is implied.'
}
