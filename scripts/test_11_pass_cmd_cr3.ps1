# Test the 11 real-PASS drivers independently obtaining cmd.exe CR3.
#
# The 11 IDs are the backends that PASSed the real EXE-memory read test
# (mrt and winring0x64 are excluded: mrt needs SYSTEM ACL, winring0x64 is
# low-memory-only).
#
# Each backend performs its own physical EPROCESS scan with its own driver:
#   membridge --backend <id> --locate --pid <cmd_pid> --name cmd.exe \
#       --scan-start <start> --scan-size <size>
# and must print CR3=0x...
#
# Usage:
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -StaticOnly
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -DryRun
#   # elevated, with cmd.exe open:
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1
#   # faster targeted range:
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -ScanStart 0x100000 -ScanSize 0x40000000
#
# WARNING: full physical scan is slow (GBs per backend). Use -MaxRanges or
# -ScanStart/-ScanSize to bound the work. Loading 11 kernel drivers can BSOD
# on a test box only; minidumps are checked before/after.
[CmdletBinding()]
param(
    [switch]$StaticOnly,
    [switch]$DryRun,
    [string]$TargetName = 'cmd.exe',
    [string]$ScanStart = '',
    [string]$ScanSize = '',
    [int]$MaxRanges = 0,
    [int]$MaxBackends = 0
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$layers = Join-Path $base 'bin\layers.exe'
$outJson = Join-Path $base 'evidence\test_11_pass_cmd_cr3.json'

$backends = @(
    'cchwapi_2_9', 'cchwapi_2_12', 'cchwapi_ext_4_5',
    'hp_swtools', 'swtoolsdriver', 'hp_swtools_v1', 'isharepp',
    'gibepext', 'gibepext2', 'tdeio', 'ufu_win10'
)

foreach ($p in @($membridge, $layers)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "missing: $p" }
}
$list = (& $membridge --list 2>&1 | Out-String)
$missing = @($backends | Where-Object { $list -notmatch [regex]::Escape($_) })
if ($missing.Count) { throw "missing backend IDs: $($missing -join ',')" }
Write-Host "== 11 PASS drivers -> cmd.exe CR3 =="
Write-Host "backends: $($backends.Count) target=$TargetName"

function Get-Ranges {
    if ($ScanStart -and $ScanSize) {
        return @([pscustomobject]@{ start = $ScanStart; size = $ScanSize })
    }
    $text = (& $layers --ranges 2>&1 | Out-String)
    $rs = @()
    foreach ($m in [regex]::Matches($text, '0x([0-9A-Fa-f]+)\s*-\s*0x([0-9A-Fa-f]+)\s+len=0x([0-9A-Fa-f]+)')) {
        $rs += [pscustomobject]@{
            start = '0x' + $m.Groups[1].Value
            size  = '0x' + $m.Groups[3].Value
        }
    }
    if ($MaxRanges -gt 0) { $rs = @($rs | Select-Object -First $MaxRanges) }
    return $rs
}

$ranges = @(Get-Ranges)
if ($MaxBackends -gt 0) { $backends = @($backends | Select-Object -First $MaxBackends) }

if ($StaticOnly) {
    $ls = (& $membridge --locate-selftest 2>&1 | Out-String)
    if ($ls -notmatch 'VERDICT=PASS') {
        Write-Host $ls
        throw "membridge --locate-selftest failed"
    }
    Write-Host "locate selftest: PASS"
    $report = [pscustomobject]@{
        mode = 'static'
        backends = $backends
        ranges = $ranges
        locate_selftest = 'PASS'
        note = 'dynamic CR3 test requires Administrator and a running cmd.exe'
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
    Write-Host "STATIC_ONLY=PASS"
    exit 0
}

if ($DryRun) {
    Write-Host "ranges:"
    $ranges | ForEach-Object { Write-Host ("  {0} + {1}" -f $_.start, $_.size) }
    Write-Host "commands:"
    foreach ($id in $backends) {
        foreach ($r in $ranges) {
            Write-Host ("  membridge --backend {0} --locate --pid <cmd_pid> --name {1} --scan-start {2} --scan-size {3}" -f $id, $TargetName, $r.start, $r.size)
        }
    }
    Write-Host "DRY_RUN=PASS"
    exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load the 11 drivers." -ForegroundColor Yellow
    exit 2
}

$procName = $TargetName -replace '\.exe$', ''
$target = Get-Process -Name $procName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $target) { throw "target process not running: $TargetName (start cmd.exe first)" }

$dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$results = @()
foreach ($bid in $backends) {
    Write-Host ">> $bid"
    $found = $null
    $last = ''
    foreach ($r in $ranges) {
        $argsList = @('--backend', $bid, '--locate', '--pid', $target.Id.ToString(),
                      '--name', $TargetName, '--scan-start', $r.start, '--scan-size', $r.size)
        $out = (& $membridge @argsList 2>&1 | Out-String)
        $last = $out
        $m = [regex]::Match($out, 'CR3=(0x[0-9A-Fa-f]+)')
        if ($m.Success) {
            $found = $m.Groups[1].Value
            break
        }
    }
    $results += [pscustomobject]@{
        backend = $bid
        cr3 = if ($found) { $found } else { '' }
        verdict = if ($found) { 'PASS' } else { 'LOCATE_FAILED' }
        last_output = $last.Trim()
    }
    Write-Host ("   => {0} {1}" -f $results[-1].verdict, $results[-1].cr3)
}

$dumpsAfter = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
$pass = @($results | Where-Object { $_.verdict -eq 'PASS' }).Count
$report = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    target_pid = $target.Id
    target = $TargetName
    total = $results.Count
    pass = $pass
    results = $results
    new_dumps = ($newDumps -join ',')
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outJson -Encoding UTF8
$results | Select-Object backend, verdict, cr3 | Format-Table -AutoSize
Write-Host ("RESULT: {0}/{1} CR3 PASS -> {2}" -f $pass, $results.Count, $outJson)
if ($pass -eq $results.Count) { exit 0 } else { exit 2 }
