# Test the 13 HVCI-PASS legacy memory-reading backends against a real EXE target.
#
# The target is `test\test.exe` (AWE page marker BYOV_TESTEXE_AWE_PAGE_00).
# Each backend independently loads its driver and reads the target EXE's AWE
# physical page; PASS means that backend read the EXE memory itself.
#
# Modes:
#   -StaticOnly : no admin required; verifies all 13 backend IDs are registered.
#   -DryRun     : no admin required; prints the exact admin test plan.
#   default     : requires Administrator; starts test.exe as SYSTEM (scheduled
#                 task, same proven flow as _elev_membridge_selftest.ps1), then
#                 runs each backend and writes evidence/test_13_pass_exe_read.json
#
# The 13 PASS IDs are the HVCI-PASS subset of the 24 legacy backends:
#   cchwapi_2_9, cchwapi_2_12, cchwapi_ext_4_5, hp_swtools, swtoolsdriver,
#   hp_swtools_v1, isharepp, gibepext, gibepext2, tdeio, mrt,
#   ufu_win10 (32-bit PA only), winring0x64 (low memory only)
#
# Usage:
#   pwsh -File scripts\test_13_pass_exe_read.ps1 -StaticOnly
#   pwsh -File scripts\test_13_pass_exe_read.ps1 -DryRun
#   # elevated:
#   pwsh -File scripts\test_13_pass_exe_read.ps1
[CmdletBinding()]
param(
    [switch]$StaticOnly,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$selftest = Join-Path $PSScriptRoot '_elev_membridge_selftest.ps1'
$outJson = Join-Path $base 'evidence\test_13_pass_exe_read.json'

$passBackends = @(
    'cchwapi_2_9', 'cchwapi_2_12', 'cchwapi_ext_4_5',
    'hp_swtools', 'swtoolsdriver', 'hp_swtools_v1', 'isharepp',
    'gibepext', 'gibepext2', 'tdeio', 'mrt',
    'ufu_win10', 'winring0x64'
)

if (-not (Test-Path $membridge)) { throw "membridge.exe not found: $membridge" }

Write-Host "== 13 PASS exe-memory read test =="
Write-Host "backends: $($passBackends.Count)"
$list = (& $membridge --list 2>&1 | Out-String)
$missing = @($passBackends | Where-Object { $list -notmatch [regex]::Escape($_) })
if ($missing.Count -gt 0) {
    throw "missing backend IDs in membridge --list: $($missing -join ',')"
}
Write-Host "static registry check: 13/13 present"

if ($StaticOnly) {
    [pscustomobject]@{
        mode = 'static'
        backends = $passBackends
        present = 13
        target = 'test\test.exe (AWE marker BYOV_TESTEXE_AWE_PAGE_00)'
        note = 'dynamic test requires Administrator'
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outJson -Encoding UTF8
    Write-Host "STATIC_ONLY=PASS"
    exit 0
}

if ($DryRun) {
    Write-Host "`nAdmin plan (run without -DryRun):"
    Write-Host "  1) start test\test.exe as SYSTEM, parse PAGE[00] PA"
    Write-Host "  2) for each backend:"
    foreach ($id in $passBackends) {
        Write-Host ("     bin\membridge.exe --backend {0} --pa <target_pa> --size 0x100 --expect BYOV_TESTEXE_AWE_PAGE_00 --dump" -f $id)
    }
    Write-Host "  3) PASS = each backend independently reads the EXE AWE page"
    Write-Host "DRY_RUN=PASS"
    exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load the 13 drivers and start the SYSTEM AWE target." -ForegroundColor Yellow
    Write-Host "Run: pwsh -File scripts\test_13_pass_exe_read.ps1  (from an elevated shell)"
    Write-Host "Static registry check already PASS; dynamic test not executed."
    exit 2
}

Write-Host ">> elevated test: starting AWE target and loading 13 backends..."
& $selftest -Backends ($passBackends -join ',')
if ($LASTEXITCODE -ne 0) { Write-Host "[WARN] selftest exit=$LASTEXITCODE" -ForegroundColor Yellow }

$summary = Join-Path $base 'evidence\membridge_selftest.json'
if (-not (Test-Path $summary)) { throw "missing selftest summary: $summary" }
$data = Get-Content -LiteralPath $summary -Raw -Encoding UTF8 | ConvertFrom-Json
$rows = @()
foreach ($id in $passBackends) {
    $r = @($data.results | Where-Object { $_.backend -eq $id })
    $verdict = if ($r.Count -gt 0) { $r[0].verdict } else { 'NOT_RUN' }
    $rows += [pscustomobject]@{ backend = $id; verdict = $verdict }
}
$pass = @($rows | Where-Object { $_.verdict -eq 'PASS' }).Count
$report = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    target = 'test\test.exe (AWE marker BYOV_TESTEXE_AWE_PAGE_00)'
    page_pa = $data.page_pa
    total = $rows.Count
    pass = $pass
    results = $rows
    new_dumps = $data.new_dumps
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host ("RESULT: {0}/{1} PASS -> {2}" -f $pass, $rows.Count, $outJson)
if ($pass -eq 13) { exit 0 } else { exit 2 }
