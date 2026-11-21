# Build bin\layers_asio2.exe with an ASUSCERT resource and PE timestamp 0.
# This executable passes the AsIO2 driver's gate (see docs/layers.md / AsIO2 notes).
$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$windres = (Get-Command windres -ErrorAction Stop).Source
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$asio2Dir = Join-Path $base 'misc\asio2'
Push-Location $asio2Dir
try {
    & $windres asio2_gate.rc -O coff -o asio2_gate.res
    if ($LASTEXITCODE -ne 0) { throw "windres failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}
& $gcc -O2 -o (Join-Path $base 'bin\layers_asio2.exe') (Join-Path $base 'src\layers.c') (Join-Path $asio2Dir 'asio2_gate.res') -ladvapi32 -lbcrypt -lpsapi
if ($LASTEXITCODE -ne 0) { throw "gcc layers_asio2 failed: $LASTEXITCODE" }
python (Join-Path $base 'scripts\patch_pe_timestamp.py') (Join-Path $base 'bin\layers_asio2.exe')
if ($LASTEXITCODE -ne 0) { throw "timestamp patch failed: $LASTEXITCODE" }
Write-Host "built: $base\bin\layers_asio2.exe (ASUSCERT resource, TimeDateStamp=0)"
