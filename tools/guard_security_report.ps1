#requires -Version 5.1
<#
.SYNOPSIS
Collect local, read-only evidence for the Bluetooth Guard Windows 225 block.
.DESCRIPTION
No downloads, uploads, scans, launches, elevation, policy changes or restores.
Only the explicitly chosen new .txt report is written. Run unelevated under
existing script policy; if policy or security software blocks this script,
stop and use Protection history instead. Do not bypass the block.

Default output: a new GuardSecurityReport-<UTC timestamp>.txt in LOCALAPPDATA.
Known profile prefixes are redacted, not all possible personal information.
Review the report before sharing it. No absence/match is a malware verdict.

Dot-source to load fixture-test functions without collecting or writing.
#>
[CmdletBinding()]
param(
    [string]$OutputPath = '',
    [ValidateRange(1, 90)][int]$LookbackDays = 14,
    [ValidateRange(16, 2000)][int]$EventLimit = 256
)

function Format-GuardValue {
    param($Value)
    if ($null -eq $Value) { return 'not reported' }
    if ($Value -is [DateTime]) { return $Value.ToString('o') }
    $text = [string]$Value
    foreach ($entry in @(
        @($env:LOCALAPPDATA, '%LOCALAPPDATA%'),
        @($env:USERPROFILE, '%USERPROFILE%')
    )) {
        if (-not [string]::IsNullOrWhiteSpace($entry[0])) {
            $prefix = [regex]::Escape($entry[0]).Replace('\\', '[\\/]')
            $text = [regex]::Replace($text, $prefix, $entry[1], 'IgnoreCase')
        }
    }
    $text = [regex]::Replace($text, '(?i)[A-Z]:[\\/]Users[\\/][^\\/\r\n";|]+', '%USERPROFILE%')
    # A field may not forge a new report heading through control characters.
    return [regex]::Replace($text.Replace("`r", '\r').Replace("`n", '\n'), '[\x00-\x1f\x7f]', '?')
}

function Add-GuardField {
    param([Collections.Generic.List[string]]$Lines, [string]$Name, $Value)
    $Lines.Add(('{0}: {1}' -f $Name, (Format-GuardValue $Value)))
}

function Format-GuardQueryError {
    param($Record)
    # Do not copy exception messages, user names, remote names or raw errors.
    return ('UNAVAILABLE: {0}; {1}; HResult={2:X8}. No access/policy change attempted.' -f
        $Record.CategoryInfo.Category, $Record.Exception.GetType().Name, $Record.Exception.HResult)
}

function Get-GuardResourceMatches {
    param($Resources, [string]$InstalledPath)
    foreach ($resource in @($Resources)) {
        if ($null -eq $resource) { continue }
        $text = ([string]$resource).Replace('/', '\')
        if ($text -notmatch '(?i)(?:^|[\\/:_\s"''=;|])BluetoothGuard\.exe(?=$|[\s"'';|])') { continue }
        $kind = 'filename-only candidate; NOT installed-path proof'
        if (-not [string]::IsNullOrWhiteSpace($InstalledPath)) {
            $exact = [regex]::Escape($InstalledPath.Replace('/', '\')) + '(?=$|[\s"'';|])'
            if ([regex]::IsMatch($text, $exact, 'IgnoreCase')) {
                $kind = 'installed-path text; NOT file-hash proof'
            }
        }
        [pscustomobject]@{ Resource = [string]$resource; Match = $kind }
    }
}

function Convert-GuardEventData {
    param([string]$XmlText)
    # Event fields are selected by their schema names, not localized messages.
    # No external XML resolution or DTD processing is permitted.
    if ($XmlText -match '(?i)<!DOCTYPE') { throw 'Event DTD refused.' }
    $document = [Xml.XmlDocument]::new()
    $document.XmlResolver = $null
    $document.LoadXml($XmlText)
    $data = @{}
    foreach ($node in $document.SelectNodes('/*[local-name()="Event"]/*[local-name()="EventData"]/*[local-name()="Data"]')) {
        $name = $node.GetAttribute('Name')
        if ($name) { $data[$name] = $node.InnerText }
    }
    return $data
}

function Assert-GuardLocalPath {
    param([string]$Path)
    # Reject UNC/device/relative/ADS paths and network drives before any I/O.
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2).Contains(':')) {
        throw 'An absolute local drive path without an alternate stream is required.'
    }
    $full = [IO.Path]::GetFullPath($Path)
    $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($full))
    if ($drive.DriveType -ne [IO.DriveType]::Fixed -and $drive.DriveType -ne [IO.DriveType]::Removable) {
        throw 'Only a local fixed/removable drive is permitted.'
    }
    $parent = [IO.Directory]::GetParent($full)
    while ($null -ne $parent) {
        if (([IO.File]::GetAttributes($parent.FullName) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Reparse-point parents are not followed.'
        }
        $parent = $parent.Parent
    }
    if ([IO.File]::Exists($full) -and
        ([IO.File]::GetAttributes($full) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'A reparse-point file is not followed.'
    }
    return $full
}

function Get-GuardFileEvidence {
    param([string]$Path, [string]$ExpectedHash)
    $stream = $null
    $sha = $null
    try {
        $full = Assert-GuardLocalPath $Path
        # One read handle prevents size/hash races with ordinary replacement.
        $stream = [IO.File]::Open($full, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $sha = [Security.Cryptography.SHA256]::Create()
        $hash = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
        # Unavailable version metadata must not discard a successfully read hash.
        try { $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($full).FileVersion }
        catch { $version = Format-GuardQueryError $_ }
        $comparison = 'DIFFERS from pinned artifact; NOT a malware verdict'
        if ($hash -eq $ExpectedHash) { $comparison = 'MATCHES pinned bytes; NOT a malware verdict' }
        return [ordered]@{
            Status = 'READABLE (not executed)'; Bytes = $stream.Length; SHA256 = $hash
            'FileVersion (untrusted metadata)' = $version; Comparison = $comparison
        }
    } catch {
        return [ordered]@{
            Status = (Format-GuardQueryError $_)
            Comparison = 'NOT COMPARED. Missing, quarantined or unreadable files are not restored/retried.'
        }
    } finally {
        if ($null -ne $sha) { $sha.Dispose() }
        if ($null -ne $stream) { $stream.Dispose() }
    }
}

function New-GuardSecurityReport {
    param([string]$LocalAppData, [int]$LookbackDays, [int]$EventLimit)
    $lines = [Collections.Generic.List[string]]::new()
    $suite = Join-Path $LocalAppData 'Programs\TestsSuite'
    $guardPath = Join-Path $suite 'BluetoothGuard\BluetoothGuard.exe'
    $hubPath = Join-Path $suite 'project-hub\ProjectHub.exe'
    # Frozen provenance, not a mutable branch-tip/latest comparison or verdict.
    $baselineCommit = '543be95ad860d6e11ffadf30248d413e0dcc839c'
    $guardHash = '4ad58a32f01a94713c37061cb79e2bf596e7c61ce8f6255b33caccb96938a2a6'
    $hubHash = '289de84e01b63375bc7e622ea11cc82a647f29acadb650c957d6a693d1f0a8e0'
    $since = [DateTime]::Now.AddDays(-$LookbackDays)
    $lines.Add('BLUETOOTH GUARD SECURITY REPORT v1')
    Add-GuardField $lines 'Started UTC' ([DateTime]::UtcNow)
    Add-GuardField $lines 'Windows' ([Environment]::OSVersion.VersionString)
    Add-GuardField $lines 'PowerShell' $PSVersionTable.PSVersion
    $lines.Add('Read-only local queries; only the chosen new report is written. No direct network requests or uploads.')
    $lines.Add('No launch, scan, restore, allow, exclusion, security-policy change or elevation is attempted.')
    $lines.Add('Known profile paths are redacted. Other identifiers/paths may remain: REVIEW BEFORE SHARING.')
    $lines.Add('225 identifies a security block, NOT its provider/rule/verdict. Compare record times and affected paths.')
    $lines.Add('No records, unavailable history and matching hashes NEVER establish a clean file or successful launch.')
    Add-GuardField $lines 'Installed target' $guardPath
    Add-GuardField $lines 'Pinned artifact commit (NOT necessarily installed)' $baselineCommit
    Add-GuardField $lines 'Pinned Guard SHA256' $guardHash
    Add-GuardField $lines 'Pinned Hub 2.8.69 SHA256' $hubHash

    $lines.Add(''); $lines.Add('--- Registered antivirus products (NOT proof which product blocked this launch) ---')
    try {
        $products = @(Get-CimInstance -Namespace 'root\SecurityCenter2' -ClassName AntiVirusProduct -OperationTimeoutSec 8 -ErrorAction Stop)
        if ($products.Count -eq 0) { $lines.Add('No registrations returned; provider remains UNKNOWN.') }
        foreach ($product in $products) {
            Add-GuardField $lines 'Registered product' $product.displayName
            Add-GuardField $lines 'productState (raw; not decoded)' $product.productState
        }
    } catch { $lines.Add((Format-GuardQueryError $_)) }

    $lines.Add(''); $lines.Add('--- Defender status (not a third-party provider history) ---')
    try {
        $status = Get-MpComputerStatus -ErrorAction Stop
        if ($null -eq $status) { $lines.Add('No status returned; Defender mode remains UNKNOWN.') }
        foreach ($field in @('AMRunningMode', 'AMServiceEnabled', 'AntivirusEnabled', 'RealTimeProtectionEnabled',
            'AntivirusSignatureVersion', 'AntivirusSignatureLastUpdated')) {
            Add-GuardField $lines $field $status.$field
        }
    } catch { $lines.Add((Format-GuardQueryError $_)) }

    $lines.Add(''); $lines.Add('--- Defender Guard resource detections (newest 50 matching history records, any age) ---')
    $lines.Add('Only affected Resources are matched, never ProcessName. Filename-only matches may be other copies.')
    try {
        $detections = @(Get-MpThreatDetection -ErrorAction Stop)
        $rows = @(
            foreach ($detection in $detections) {
                $matches = @(Get-GuardResourceMatches $detection.Resources $guardPath)
                if ($matches.Count -gt 0) { [pscustomobject]@{ Detection = $detection; Matches = $matches } }
            }
        )
        Add-GuardField $lines 'Matching history records (not necessarily this launch)' $rows.Count
        if ($rows.Count -eq 0) { $lines.Add('No matching resource records returned. NOT a clean verdict; history may be unavailable elsewhere.') }
        if ($rows.Count -gt 50) { $lines.Add('LIMIT: only the newest 50 matches are included; older matches omitted.') }
        $selected = @($rows | Sort-Object { $_.Detection.InitialDetectionTime } -Descending | Select-Object -First 50)
        $names = @{}
        $ids = @($selected | ForEach-Object { $_.Detection.ThreatID } | Where-Object { $null -ne $_ } | Sort-Object -Unique)
        if ($ids.Count -gt 0) {
            try {
                foreach ($threat in @(Get-MpThreat -ThreatID $ids -ErrorAction Stop)) {
                    $names[[string]$threat.ThreatID] = $threat.ThreatName
                }
            } catch { $lines.Add(('Threat-name lookup ' + (Format-GuardQueryError $_))) }
        }
        foreach ($row in $selected) {
            $detection = $row.Detection
            $lines.Add('Detection (Defender history; not an assertion of current active provider):')
            Add-GuardField $lines 'ThreatName' $names[[string]$detection.ThreatID]
            foreach ($field in @('ThreatID', 'DetectionID', 'InitialDetectionTime', 'LastThreatStatusChangeTime',
                'RemediationTime', 'ActionSuccess', 'CleaningActionID', 'ThreatStatusID', 'CurrentThreatExecutionStatusID',
                'DetectionSourceTypeID', 'ThreatStatusErrorCode')) {
                Add-GuardField $lines $field $detection.$field
            }
            foreach ($match in $row.Matches) {
                Add-GuardField $lines 'Resource correlation' $match.Match
                Add-GuardField $lines 'Affected resource' $match.Resource
            }
        }
    } catch { $lines.Add((Format-GuardQueryError $_)) }

    $lines.Add(''); $lines.Add('--- Defender operational events 1116/1117/1118/1119 (affected Path/Resources only) ---')
    Add-GuardField $lines 'Window start (local)' $since
    Add-GuardField $lines 'Newest event query limit' $EventLimit
    $lines.Add('Names/actions below are event data, not an inference from registration, a process reference or numeric IDs.')
    try {
        $events = @(Get-WinEvent -FilterHashtable @{
            LogName = 'Microsoft-Windows-Windows Defender/Operational'
            Id = @(1116, 1117, 1118, 1119); StartTime = $since
        } -MaxEvents $EventLimit -ErrorAction Stop)
        if ($events.Count -ge $EventLimit) { $lines.Add('LIMIT REACHED: older events may be omitted; history is NOT exhaustive.') }
        $matchingEvents = 0
        $invalidEvents = 0
        foreach ($event in $events) {
            try {
                $data = Convert-GuardEventData $event.ToXml()
                $matches = @(Get-GuardResourceMatches @($data['Path'], $data['Resources']) $guardPath)
                if ($matches.Count -eq 0) { continue }
                $matchingEvents++
                Add-GuardField $lines 'EventID' $event.Id
                Add-GuardField $lines 'RecordID' $event.RecordId
                Add-GuardField $lines 'TimeCreated' $event.TimeCreated
                Add-GuardField $lines 'Event provider' $event.ProviderName
                foreach ($field in @('Threat ID', 'Threat Name', 'Severity Name', 'Category Name', 'Detection ID',
                    'Detection Time', 'Action ID', 'Action Name', 'Status Code', 'Status Description', 'State',
                    'Error Code', 'Error Description', 'Product Name', 'Product Version')) {
                    if ($data.ContainsKey($field)) { Add-GuardField $lines $field $data[$field] }
                }
                foreach ($match in $matches) {
                    Add-GuardField $lines 'Resource correlation' $match.Match
                    Add-GuardField $lines 'Affected resource' $match.Resource
                }
            } catch { $invalidEvents++ }
        }
        Add-GuardField $lines 'Matching affected-resource events' $matchingEvents
        Add-GuardField $lines 'Unreadable/malformed events (not treated as no detection)' $invalidEvents
        if ($matchingEvents -eq 0) { $lines.Add('No matching affected-resource events in queried window/limit. NOT a clean verdict.') }
    } catch {
        if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') {
            $lines.Add('No selected event IDs returned in this window. NOT a clean verdict.')
        } else { $lines.Add((Format-GuardQueryError $_)) }
    }

    $lines.Add(''); $lines.Add('--- Installed-file provenance (read only; no quarantine restore) ---')
    $lines.Add('Hashing may encounter normal on-access protection. Inaccessible files are not retried.')
    $lines.Add('Signature trust/catalog/revocation validation is NOT attempted; no online trust lookup is made.')
    foreach ($entry in @(@($guardPath, $guardHash), @($hubPath, $hubHash))) {
        Add-GuardField $lines 'File' $entry[0]
        $evidence = Get-GuardFileEvidence $entry[0] $entry[1]
        foreach ($key in $evidence.Keys) { Add-GuardField $lines $key $evidence[$key] }
    }
    $lines.Add(''); $lines.Add('--- Next evidence / limits ---')
    $lines.Add('Review the matching Protection history entry (or actual antivirus history) for detection, provider, time, action/status and path.')
    $lines.Add('Third-party histories are NOT collected. Registered products or old Defender entries alone do not identify the blocking provider.')
    $lines.Add('Do not Allow/Restore, disable protection, add exclusions, elevate to hide errors, or rename/rebuild to evade detection.')
    $lines.Add('No false-positive finding, malware verdict, successful Guard launch or Open-to-front acceptance is claimed.')
    Add-GuardField $lines 'Finished UTC' ([DateTime]::UtcNow)
    return $lines
}

function Write-GuardReportFile {
    param([string]$Path, [string[]]$Lines)
    $full = Assert-GuardLocalPath $Path
    if ([IO.Path]::GetExtension($full) -ine '.txt') { throw 'Choose a new .txt report file.' }
    $stream = $null
    $writer = $null
    try {
        # CreateNew is deliberate: never overwrite an executable, setting or old report.
        $stream = [IO.File]::Open($full, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $writer = [IO.StreamWriter]::new($stream, [Text.UTF8Encoding]::new($true))
        foreach ($line in $Lines) { $writer.WriteLine($line) }
    } finally {
        if ($null -ne $writer) { $writer.Dispose() }
        if ($null -ne $stream) { $stream.Dispose() }
    }
    return $full
}

function Invoke-GuardSecurityReport {
    param([string]$OutputPath, [int]$LookbackDays, [int]$EventLimit)
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Run this collector on the affected Windows PC.' }
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) { throw 'LOCALAPPDATA is unavailable; use security history instead.' }
    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $OutputPath = Join-Path $env:LOCALAPPDATA ('GuardSecurityReport-{0}.txt' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff'))
    }
    # Validate destination first, without creating it or changing existing files.
    $OutputPath = Assert-GuardLocalPath $OutputPath
    if ([IO.Path]::GetExtension($OutputPath) -ine '.txt' -or [IO.File]::Exists($OutputPath)) {
        throw 'Choose a NEW local .txt report filename. Existing files are never overwritten.'
    }
    $lines = @(New-GuardSecurityReport $env:LOCALAPPDATA $LookbackDays $EventLimit)
    $written = Write-GuardReportFile $OutputPath $lines
    Write-Host ('Report written: ' + $written)
    Write-Host 'Review before sharing. Unavailable/no-match data is not a clean verdict; keep the security block in place.'
}

if ($MyInvocation.InvocationName -ne '.') {
    Invoke-GuardSecurityReport $OutputPath $LookbackDays $EventLimit
}
