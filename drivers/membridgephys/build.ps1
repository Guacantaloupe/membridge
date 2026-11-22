# Build MembridgePhys.sys with the Windows Driver Kit.
# Requires: Visual Studio Build Tools (MSVC) + Windows Driver Kit (WDK).
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $PSScriptRoot 'membridgephys.c'
$out = Join-Path $PSScriptRoot 'MembridgePhys.sys'

$kits = 'C:\Program Files (x86)\Windows Kits\10'
$kmInclude = Get-ChildItem (Join-Path $kits 'Include') -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'km\ntddk.h') } |
    Sort-Object Name -Descending | Select-Object -First 1
$msvc = Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC' -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1

if (-not $kmInclude -or -not $msvc) {
    Write-Host "WDK kernel headers or MSVC not found." -ForegroundColor Yellow
    Write-Host "Install the Windows Driver Kit (WDK) for Windows 11, then run from an"
    Write-Host "elevated 'x64 Native Tools Command Prompt for VS 2022':"
    Write-Host "  cl /kernel /GS- /Gw /Gy /Oi /Os /D_AMD64_ /DAMD64 /c membridgephys.c"
    Write-Host "  link /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /OUT:MembridgePhys.sys membridgephys.obj ntoskrnl.lib"
    exit 0
}

$cl = Join-Path $msvc.FullName 'bin\Hostx64\x64\cl.exe'
$link = Join-Path $msvc.FullName 'bin\Hostx64\x64\link.exe'
$km = Join-Path $kmInclude.FullName 'km'
$kmLib = Join-Path (Join-Path $kits 'Lib') (Join-Path $kmInclude.Name 'km\x64')
Write-Host "WDK: $km"
Write-Host "MSVC: $($msvc.FullName)"

Push-Location $PSScriptRoot
try {
    & $cl /nologo /kernel /GS- /Gw /Gy /Oi /Os /D_AMD64_ /DAMD64 /I $km /c membridgephys.c
    if ($LASTEXITCODE -ne 0) { throw "cl failed: $LASTEXITCODE" }
    & $link /nologo /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /OUT:MembridgePhys.sys membridgephys.obj (Join-Path $kmLib 'ntoskrnl.lib')
    if ($LASTEXITCODE -ne 0) { throw "link failed: $LASTEXITCODE" }
}
finally {
    Pop-Location
}
Write-Host "Built: $out (unsigned; sign it before loading under HVCI)"
