$ErrorActionPreference = 'Stop'
$repo = 'ajani190819-ops/Tests'
if ($args -contains '--help' -or $args -contains '-h') {
  Write-Host 'Usage: Orca-Plugins.bat [Orca data directory]'
  Write-Host '       Set ORCA_DATA_DIR to avoid the directory prompt.'
  exit 0
}
try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  Write-Host "Fetching branches..."
  $branches = @(Invoke-RestMethod -Headers @{ 'User-Agent' = 'Orca-Plugins-Updater' } "https://api.github.com/repos/$repo/branches?per_page=100" | Sort-Object { $_.commit.committer.date } -Descending | Select-Object -First 5 -ExpandProperty name)
} catch {
  Write-Host "Could not fetch the branch list: $($_.Exception.Message)" -ForegroundColor Red
  exit 1
}
if ($branches -notcontains 'main') { $branches += 'main' }
Write-Host ""
for ($i=0; $i -lt $branches.Count; $i++) { Write-Host "[$($i+1)] $($branches[$i])" }
Write-Host "[T] Type a branch name"
$pick = Read-Host 'Choice'
if ($pick -match '^[Tt]$') { $branch = Read-Host 'Branch name' }
elseif ($pick -match '^\d+$' -and [int]$pick -ge 1 -and [int]$pick -le $branches.Count) { $branch = $branches[[int]$pick-1] }
else { Write-Host 'Invalid choice.' -ForegroundColor Red; exit 2 }
Write-Host "Using branch: $branch"
$root = if ($args.Count -gt 0 -and $args[0] -notmatch '^-' -and $args[0]) {
  $args[0]
} elseif ($env:ORCA_DATA_DIR) {
  $env:ORCA_DATA_DIR
} else {
  $candidates = @(Get-ChildItem (Join-Path $env:APPDATA 'OrcaSlicer*') -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
  if ($candidates.Count -eq 1) { $candidates[0].FullName }
  elseif ($candidates.Count -gt 1) {
    Write-Host 'Orca data directories found:'
    for ($i = 0; $i -lt $candidates.Count; $i++) { Write-Host "[$($i+1)] $($candidates[$i].FullName)" }
    $n = Read-Host 'Choose data directory'
    if ($n -match '^\d+$' -and [int]$n -ge 1 -and [int]$n -le $candidates.Count) { $candidates[[int]$n-1].FullName } else { throw 'Invalid data-directory choice.' }
  } else { Join-Path $env:APPDATA 'OrcaSlicer' }
}
$target = Join-Path $root 'orca_plugins'
$catalog = Invoke-RestMethod "https://raw.githubusercontent.com/$repo/$branch/plugins.json"
foreach ($plugin in @($catalog.plugins | Where-Object status -eq 'ready')) {
  $dir = Join-Path $target $plugin.orca_dir
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  $url = "https://raw.githubusercontent.com/$repo/$branch/$($plugin.path)"
  Invoke-WebRequest -UseBasicParsing $url -OutFile (Join-Path $dir $plugin.file)
  Write-Host "Installed $($plugin.name) $($plugin.version)"
}
Write-Host "Done. Plugins installed in $target" -ForegroundColor Green
