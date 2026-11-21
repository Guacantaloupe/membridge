# Build membridge.exe + cr3read.exe (MinGW-w64 gcc required)
$ErrorActionPreference = 'Stop'
$base = $PSScriptRoot
$gcc = (Get-Command gcc -ErrorAction SilentlyContinue)
if (-not $gcc) { throw 'gcc not found (install MSYS2/MinGW-w64)' }
& $gcc.Source -O2 -municode -o (Join-Path $base 'bin\membridge.exe') (Join-Path $base 'src\membridge.c') -lsetupapi -ladvapi32 -lbcrypt
if ($LASTEXITCODE -ne 0) { throw "membridge build failed: $LASTEXITCODE" }
& $gcc.Source -O2 -o (Join-Path $base 'bin\cr3read.exe') (Join-Path $base 'src\cr3read.c') -lpsapi -ladvapi32
if ($LASTEXITCODE -ne 0) { throw "cr3read build failed: $LASTEXITCODE" }
& $gcc.Source -O2 -o (Join-Path $base 'bin\layers.exe') (Join-Path $base 'src\layers.c') -ladvapi32
if ($LASTEXITCODE -ne 0) { throw "layers build failed: $LASTEXITCODE" }
Write-Host "built: $base\bin\membridge.exe"
Write-Host "built: $base\bin\cr3read.exe"
Write-Host "built: $base\bin\layers.exe"
