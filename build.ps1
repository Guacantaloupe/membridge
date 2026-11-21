# Build membridge.exe (MinGW-w64 gcc required)
$ErrorActionPreference = 'Stop'
$base = $PSScriptRoot
$gcc = (Get-Command gcc -ErrorAction SilentlyContinue)
if (-not $gcc) { throw 'gcc not found (install MSYS2/MinGW-w64)' }
& $gcc.Source -O2 -municode -o (Join-Path $base 'bin\membridge.exe') (Join-Path $base 'src\membridge.c') -lsetupapi -ladvapi32
if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
Write-Host "built: $base\bin\membridge.exe"
