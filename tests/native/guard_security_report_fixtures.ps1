#requires -Version 5.1
# Offline fixture tests, NOT a real antivirus-history or Guard-launch test.
# Run with ordinary Windows PowerShell under the existing script policy.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\tools\guard_security_report.ps1')
$checks = 0

function Assert-GuardFixture {
    param([bool]$Condition, [string]$Description)
    if (-not $Condition) { throw ('Fixture failed: ' + $Description) }
    $script:checks++
}

$guard = Join-Path $env:LOCALAPPDATA 'Programs\TestsSuite\BluetoothGuard\BluetoothGuard.exe'
$matches = @(Get-GuardResourceMatches @(
    ('file:_' + $guard), 'file:_C:\Other\BluetoothGuard.exe',
    'file:_C:\Other\BluetoothGuard.exe.bak', 'file:_C:\Other\NotBluetoothGuard.exe',
    'file:_C:\Other\BluetoothGuardXexe', $null
) $guard)
Assert-GuardFixture ($matches.Count -eq 2) 'only exact executable basenames match'
Assert-GuardFixture ($matches[0].Match -like 'installed-path text*') 'installed-path correlation labeled'
Assert-GuardFixture ($matches[1].Match -like 'filename-only candidate*') 'other copies are only candidates'
$forward = @(Get-GuardResourceMatches $guard.Replace('\', '/') $guard)
Assert-GuardFixture ($forward.Count -eq 1 -and $forward[0].Match -like 'installed-path text*') 'forward separators normalize'
Assert-GuardFixture (@(Get-GuardResourceMatches 'BluetoothGuard.exe' '').Count -eq 1) 'bare resource is only a candidate'

$formatted = Format-GuardValue ($guard.ToUpperInvariant() + "`r`nFORGED" + [char]27)
Assert-GuardFixture ($formatted -notmatch [regex]::Escape($env:LOCALAPPDATA)) 'known local path redacted ignoring case'
Assert-GuardFixture ($formatted -notmatch '[\x00-\x1f\x7f]') 'field controls cannot forge lines'
Assert-GuardFixture ($formatted -like '*\r\nFORGED?') 'controls are represented, not evaluated'
Assert-GuardFixture ((Format-GuardValue $guard.Replace('\', '/')) -like '%LOCALAPPDATA%*') 'forward profile separators redact'
Assert-GuardFixture ((Format-GuardValue $null) -eq 'not reported') 'missing values remain explicit'

$xml = '<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event"><EventData>' +
    '<Data Name="Threat Name">Fixture:Win32/Only</Data><Data Name="Action Name">Quarantine</Data>' +
    '<Data Name="Path">' + [Security.SecurityElement]::Escape(('file:_' + $guard)) + '</Data>' +
    '<Data Name="Detection User">PRIVATE_ACCOUNT</Data></EventData></Event>'
$data = Convert-GuardEventData $xml
Assert-GuardFixture ($data['Threat Name'] -eq 'Fixture:Win32/Only' -and $data['Action Name'] -eq 'Quarantine') 'schema-named fields extracted'
Assert-GuardFixture ($data['Path'] -eq ('file:_' + $guard)) 'XML escapes decoded for literal path correlation'
$refused = $false
try { $null = Convert-GuardEventData '<!DOCTYPE Event [<!ENTITY x SYSTEM "file:///private">]><Event>&x;</Event>' }
catch { $refused = $true }
Assert-GuardFixture $refused 'DTD input refused without external resolution'

# Only synthetic .bin/.txt files are created. No suite EXE is copied or opened.
$scratch = Join-Path $env:TEMP ('GuardSecurityFixtures-' + [Guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($scratch)
try {
    $fixture = Join-Path $scratch 'fixture.bin'
    [IO.File]::WriteAllBytes($fixture, [Text.Encoding]::ASCII.GetBytes("fixture only`n"))
    $known = 'b885fe49dbce4a89fc20cfef68b7925fb87cb0ee88b2d33b645d9f180733f08a'
    $proof = Get-GuardFileEvidence $fixture $known
    Assert-GuardFixture ($proof['SHA256'] -eq $known -and $proof['Bytes'] -eq 13) 'full hash and size of fixture'
    Assert-GuardFixture ($proof['Comparison'] -like 'MATCHES*NOT a malware verdict') 'hash identity is not a clean verdict'
    $different = Get-GuardFileEvidence $fixture ('0' * 64)
    Assert-GuardFixture ($different['Comparison'] -like 'DIFFERS*') 'different expected bytes are not called matching'
    $missing = Get-GuardFileEvidence (Join-Path $scratch 'missing.bin') $known
    Assert-GuardFixture ($missing['Status'] -like 'UNAVAILABLE:*' -and $missing['Comparison'] -like 'NOT COMPARED*') 'missing file remains unavailable'
    $destination = Join-Path $scratch 'report.txt'
    $null = Write-GuardReportFile $destination @('synthetic', [string][char]0x03a9)
    $before = [IO.File]::ReadAllText($destination)
    Assert-GuardFixture ($before.Contains([string][char]0x03a9)) 'report preserves UTF-8 characters'
    $refused = $false
    try { $null = Write-GuardReportFile $destination @('must not overwrite') }
    catch { $refused = $true }
    Assert-GuardFixture ($refused -and [IO.File]::ReadAllText($destination) -eq $before) 'existing report cannot be overwritten'
    $refused = $false
    try { $null = Write-GuardReportFile (Join-Path $scratch 'settings.ini') @('must not write') }
    catch { $refused = $true }
    Assert-GuardFixture ($refused -and -not [IO.File]::Exists((Join-Path $scratch 'settings.ini'))) 'only a new .txt destination is permitted'
    foreach ($path in @('\\server\share\report.txt', '\\?\C:\report.txt', '.\report.txt', 'C:\report.txt:stream')) {
        $refused = $false
        try { $null = Assert-GuardLocalPath $path } catch { $refused = $true }
        Assert-GuardFixture $refused 'UNC/device/relative/ADS destination refused'
    }
} finally {
    [IO.Directory]::Delete($scratch, $true)
}

# From here on every Windows query and installed-file reader is mocked.
# Neither a missing Defender module nor machine policy can make a real query.
$mode = 'Normal'
$fileReads = 0
function Get-CimInstance {
    [CmdletBinding()]
    param($Namespace, $ClassName, $OperationTimeoutSec)
    if ($script:mode -eq 'Denied') { throw [UnauthorizedAccessException]::new('PRIVATE_ERROR') }
    [pscustomobject]@{ displayName = 'Fixture registered provider'; productState = 12345 }
}
function Get-MpComputerStatus {
    [CmdletBinding()]
    param()
    if ($script:mode -eq 'Denied') { throw [UnauthorizedAccessException]::new('PRIVATE_ERROR') }
    [pscustomobject]@{ AMRunningMode = 'Fixture mode'; AMServiceEnabled = $false; AntivirusEnabled = $false }
}
function Get-MpThreatDetection {
    [CmdletBinding()]
    param()
    if ($script:mode -eq 'Denied') { throw [UnauthorizedAccessException]::new('PRIVATE_ERROR') }
    if ($script:mode -eq 'Empty' -or $script:mode -eq 'NoEvents') { return }
    $count = 1
    if ($script:mode -eq 'Cap') { $count = 51 }
    foreach ($i in 1..$count) {
        [pscustomobject]@{
            Resources = @('file:_' + $script:guard); ProcessName = 'PRIVATE_PROCESS'
            ThreatID = $i; InitialDetectionTime = [DateTime]::Now.AddMinutes(-$i)
            DetectionID = ('fixture-' + $i); CleaningActionID = 2; ThreatStatusID = 3; ActionSuccess = $true
        }
    }
    [pscustomobject]@{
        Resources = @('file:_C:\Unrelated\Other.exe'); ProcessName = $script:guard
        ThreatID = 999; InitialDetectionTime = [DateTime]::Now
    }
}
function Get-MpThreat {
    [CmdletBinding()]
    param($ThreatID)
    foreach ($id in $ThreatID) { [pscustomobject]@{ ThreatID = $id; ThreatName = ('Fixture-' + $id) } }
}
function New-GuardFixtureEvent {
    param([string]$Xml, [int]$RecordId = 1)
    $record = [pscustomobject]@{
        Id = 1116; RecordId = $RecordId; TimeCreated = [DateTime]::Now
        ProviderName = 'Fixture event provider'; Xml = $Xml
    }
    $record | Add-Member -MemberType ScriptMethod -Name ToXml -Value { return $this.Xml } -PassThru
}
function Get-WinEvent {
    [CmdletBinding()]
    param($FilterHashtable, $MaxEvents)
    if ($script:mode -eq 'Denied') { throw [UnauthorizedAccessException]::new('PRIVATE_ERROR') }
    if ($script:mode -eq 'NoEvents') {
        Write-Error -Message 'PRIVATE_ERROR' -ErrorId NoMatchingEventsFound -Category ObjectNotFound
        return
    }
    if ($script:mode -eq 'Empty') { return }
    New-GuardFixtureEvent $script:xml 1
    New-GuardFixtureEvent '<not-valid' 2
    $unrelated = '<Event><EventData><Data Name="Path">file:_C:\Other.exe</Data>' +
        '<Data Name="Process Name">' + [Security.SecurityElement]::Escape($script:guard) + '</Data>' +
        '<Data Name="Threat Name">UNRELATED_THREAT</Data></EventData></Event>'
    if ($script:mode -eq 'Cap') {
        foreach ($i in 3..$MaxEvents) { New-GuardFixtureEvent $unrelated $i }
    } else { New-GuardFixtureEvent $unrelated 3 }
}
function Get-GuardFileEvidence {
    param([string]$Path, [string]$ExpectedHash)
    $script:fileReads++
    [ordered]@{ Status = 'UNAVAILABLE: synthetic inaccessible file'; Comparison = 'NOT COMPARED' }
}

$normal = (New-GuardSecurityReport $env:LOCALAPPDATA 14 16) -join "`n"
Assert-GuardFixture ($normal.Contains('ThreatName: Fixture-1')) 'ID to threat-name lookup retained'
Assert-GuardFixture ($normal.Contains('Action Name: Quarantine') -and $normal.Contains('ActionSuccess: True')) 'action name and raw action outcome retained'
Assert-GuardFixture ($normal.Contains('Matching history records (not necessarily this launch): 1')) 'process-only reference is not affected-file evidence'
Assert-GuardFixture ($normal.Contains('Matching affected-resource events: 1')) 'only resource-matched event emitted'
Assert-GuardFixture ($normal.Contains('Unreadable/malformed events (not treated as no detection): 1')) 'malformed event counted explicitly'
foreach ($secret in @('PRIVATE_ACCOUNT', 'PRIVATE_PROCESS', 'UNRELATED_THREAT', $env:LOCALAPPDATA, $env:USERPROFILE)) {
    Assert-GuardFixture (-not $normal.Contains($secret)) 'unrelated/private metadata omitted'
}
Assert-GuardFixture ($normal.Contains('Signature trust/catalog/revocation validation is NOT attempted')) 'no online signature-verification claim'
$mode = 'Cap'
$capped = (New-GuardSecurityReport $env:LOCALAPPDATA 14 16) -join "`n"
Assert-GuardFixture ($capped.Contains('LIMIT: only the newest 50 matches') -and $capped.Contains('LIMIT REACHED:')) 'both history limits explicit'
Assert-GuardFixture ([regex]::Matches($capped, '(?m)^Detection \(Defender history;').Count -eq 50) 'matching detection output capped at 50'
$mode = 'Denied'
$denied = (New-GuardSecurityReport $env:LOCALAPPDATA 14 16) -join "`n"
Assert-GuardFixture (-not $denied.Contains('PRIVATE_ERROR') -and $denied.Contains('UNAVAILABLE:')) 'query errors explicit without raw messages'
Assert-GuardFixture ($script:fileReads -eq 6) 'unavailable queries do not suppress file-provenance sections'
$mode = 'Empty'
$empty = (New-GuardSecurityReport $env:LOCALAPPDATA 14 16) -join "`n"
Assert-GuardFixture ($empty.Contains('No matching resource records returned. NOT a clean verdict') -and $empty.Contains('No matching affected-resource events')) 'empty history not a clean verdict'
$mode = 'NoEvents'
$noEvents = (New-GuardSecurityReport $env:LOCALAPPDATA 14 16) -join "`n"
Assert-GuardFixture ($noEvents.Contains('No selected event IDs returned in this window. NOT a clean verdict')) 'no-event error distinct from unavailable service'
Write-Host ('Passed ' + $checks + ' offline/mocked fixture assertions. Not a native provider, malware, launch or focus verdict.')
