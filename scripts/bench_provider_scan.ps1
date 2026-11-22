# Benchmark provider physical-scan CR3 discovery.
#
# Runs: layers.exe --locate --scan-bench ...
# Each provider prints open/scan milliseconds and result (unavailable / no-cr3 / CR3).
#
# Admin test host example:
#   pwsh -File scripts\bench_provider_scan.ps1 -ProcessId 1234 -Name cmd.exe -AutoRanges
#   pwsh -File scripts\bench_provider_scan.ps1 -ProcessId 1234 -ScanStart 0x100000 -ScanSize 0x100000
# With legacy fallback:
#   pwsh -File scripts\bench_provider_scan.ps1 -ProcessId 4242 -ScanStart 0x100000 -ScanSize 0x100000 `
#       -LegacyBackend cchwapi_2_12 -LegacyBin .\_tmp\fake_legacy_mem.exe
[CmdletBinding()]
param(
    [int]$ProcessId = 0,
    [string]$Name = 'cmd.exe',
    [string]$ScanStart = '0x100000',
    [string]$ScanSize = '0x100000',
    [switch]$AutoRanges,
    [string]$LegacyBackend = '',
    [string]$LegacyBin = ''
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$layers = Join-Path $base 'bin\layers.exe'
if (-not (Test-Path $layers)) { throw "layers.exe not found: $layers" }
if ($ProcessId -le 0) { throw "provide -ProcessId (cmd.exe PID)" }

$argsList = @('--locate', '--scan-bench', '--pid', $ProcessId.ToString(), '--name', $Name)
if ($AutoRanges) {
    $argsList += '--auto-ranges'
} else {
    $argsList += @('--scan-start', $ScanStart, '--scan-size', $ScanSize)
}
if ($LegacyBackend) {
    $argsList += @('--legacy-backend', $LegacyBackend)
    if ($LegacyBin) { $argsList += @('--legacy-bin', $LegacyBin) }
}

Write-Host ">> layers $($argsList -join ' ')"
$sw = [Diagnostics.Stopwatch]::StartNew()
$old = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $layers @argsList 2>&1 | ForEach-Object { Write-Host $_ }
$rc = $LASTEXITCODE
$ErrorActionPreference = $old
$sw.Stop()
Write-Host ("WALL_MS={0:N1}" -f $sw.Elapsed.TotalMilliseconds)
exit $rc

