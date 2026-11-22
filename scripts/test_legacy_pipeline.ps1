# Reproducible offline test for the 24 legacy backend bridge.
# Builds a synthetic physical-memory membridge.exe and runs:
#   1. translation+fetch via --reader legacy
#   2. physical scan locator + translation + fetch via --reader legacy
# No real driver or admin rights required.
#
# Usage: pwsh -File scripts\test_legacy_pipeline.ps1
$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$layers = Join-Path $base 'bin\layers.exe'
if (-not (Test-Path $layers)) { throw "layers.exe not found: $layers" }
$tmp = Join-Path $base '_tmp'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$fake = Join-Path $tmp 'fake_legacy_mem.exe'
$src = Join-Path $base 'test\fake_legacy_mem.c'
$gcc = (Get-Command gcc -ErrorAction Stop).Source
& $gcc -O2 -o $fake $src
if ($LASTEXITCODE -ne 0) { throw "fake build failed" }

function Run-Test([string]$name, [string[]]$argsList, [string[]]$expect) {
    Write-Host "== $name =="
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $out = & $layers @argsList 2>&1 | Out-String
    $rc = $LASTEXITCODE
    $ErrorActionPreference = $old
    foreach ($e in $expect) {
        if ($out -notmatch [regex]::Escape($e)) {
            Write-Host $out
            throw "missing expected output: $e (exit=$rc)"
        }
    }
    if ($rc -ne 0) {
        Write-Host $out
        throw "$name failed: exit=$rc"
    }
    Write-Host $out
}

Run-Test 'translate+fetch via legacy backend' @(
    '--reader','legacy','--legacy-backend','cchwapi_2_12','--legacy-bin',$fake,
    '--cr3','0x200000','--va','0x400000','--size','8','--dump'
) @('PA=0x300000', '4C 45 47 41 43 59', 'VERDICT=PASS')

Run-Test 'physical scan locator + translate + fetch' @(
    '--reader','legacy','--legacy-backend','cchwapi_2_12','--legacy-bin',$fake,
    '--locate','--pid','4242','--name','cmd.exe',
    '--scan-start','0x100000','--scan-size','0x100000',
    '--va','0x400000','--size','8','--dump'
) @('PID=4242', 'EPROCESS_PA=0x100000', 'CR3=0x200000', '4C 45 47 41', 'VERDICT=PASS')

Run-Test '1MB scan chunking (> membridge 64KB limit)' @(
    '--reader','legacy','--legacy-backend','cchwapi_2_12','--legacy-bin',$fake,
    '--locate','--pid','4242','--name','cmd.exe',
    '--scan-start','0x100000','--scan-size','0x100000'
) @('CR3=0x200000')

Run-Test 'provider-scan (20 providers + legacy fallback)' @(
    '--locate','--scan-providers','--pid','4242','--name','cmd.exe',
    '--scan-start','0x100000','--scan-size','0x100000',
    '--reader','legacy','--legacy-backend','cchwapi_2_12','--legacy-bin',$fake,
    '--va','0x400000','--size','8','--dump'
) @('provider=legacy:cchwapi_2_12', 'CR3=0x200000', '4C 45 47 41', 'VERDICT=PASS')

Write-Host "LEGACY_PIPELINE_TEST=PASS"
