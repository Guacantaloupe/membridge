# Dynamic verification harness for membridge layers.
#
# On an ADMIN host this can (optionally) create/start a kernel service for a
# candidate driver, probe the provider devices, perform a physical read, run
# the auto locator, then stop/delete the service. It always checks for a new
# BSOD minidump before/after and aborts if one appeared.
#
# Safe dry-run (no service changes, no privileged operations):
#   pwsh -File scripts\dynamic_verify.ps1 -DryRun -Reader winio64 -DevicePath '\\.\WinIo' -Locate
#
# Full run (ADMIN, examples):
#   pwsh -File scripts\dynamic_verify.ps1 -Reader winio64 -DevicePath '\\.\WinIo' `
#        -DriverPath C:\path\WinIo64.sys -ServiceName WinIo64
#   pwsh -File scripts\dynamic_verify.ps1 -Reader asio64 -DevicePath '\\.\Asusgio' `
#        -DriverPath C:\path\AsIO64.sys -ServiceName AsIO64
[CmdletBinding()]
param(
    [string]$Reader = "winio64",
    [string]$DevicePath = "",
    [string]$DriverPath = "",
    [string]$ServiceName = "",
    [uint64]$Pa = 0x10000000,
    [uint32]$Size = 16,
    [uint32]$PidToLocate = 4,
    [switch]$Locate,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$layers = Join-Path $base 'bin\layers.exe'
if (-not (Test-Path -LiteralPath $layers)) { throw "layers.exe not found: $layers" }

function Get-MinidumpState {
    $d = 'C:\Windows\Minidump'
    if (-not (Test-Path -LiteralPath $d)) { return @{ count = 0; latest = $null } }
    $f = Get-ChildItem -LiteralPath $d -Filter *.dmp -File -ErrorAction SilentlyContinue |
         Sort-Object LastWriteTime -Descending | Select-Object -First 1
    return @{ count = @(Get-ChildItem -LiteralPath $d -Filter *.dmp -File -ErrorAction SilentlyContinue).Count;
              latest = if ($f) { $f.Name + '@' + $f.LastWriteTime } else { $null } }
}

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Invoke-Layers {
    param([string[]]$Arguments)
    Write-Host (">> layers.exe " + ($Arguments -join ' '))
    if ($DryRun) { Write-Host "   (dry-run: not executed)"; return 0 }
    & $layers @Arguments
    return $LASTEXITCODE
}

$before = Get-MinidumpState
Write-Host "== membridge dynamic verify =="
Write-Host ("reader={0} device={1} driver={2} service={3} dryRun={4}" -f `
    $Reader, $DevicePath, $DriverPath, $ServiceName, $DryRun)
Write-Host ("minidumps before: count={0} latest={1}" -f $before.count, $before.latest)

$serviceCreated = $false
try {
    if (($DriverPath -or $ServiceName) -and -not $DryRun) {
        if (-not (Test-Admin)) { throw "creating/starting a kernel service requires Administrator" }
        if (-not $ServiceName) { $ServiceName = [IO.Path]::GetFileNameWithoutExtension($DriverPath) }
        if (-not (Test-Path -LiteralPath $DriverPath)) { throw "driver not found: $DriverPath" }
        Write-Host ">> sc create $ServiceName binPath=$DriverPath type=kernel"
        & sc.exe create $ServiceName binPath= $DriverPath type= kernel | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "sc create failed: $LASTEXITCODE" }
        $serviceCreated = $true
        Write-Host ">> sc start $ServiceName"
        & sc.exe start $ServiceName | Out-Host
        Start-Sleep -Milliseconds 800
    }

    Invoke-Layers @('--probe-readers') | Out-Null

    $readArgs = @('--reader', $Reader, '--read-phys', '--pa', ('0x{0:X}' -f $Pa), '--size', $Size)
    if ($DevicePath) { $readArgs += @('--device', $DevicePath) }
    $readRc = Invoke-Layers $readArgs
    Write-Host ("physical-read exit: {0}" -f $readRc)

    if ($Locate) {
        $locArgs = @('--locate', '--auto', '--pid', $PidToLocate, '--reader', $Reader, '--auto-ranges')
        if ($DevicePath) { $locArgs += @('--device', $DevicePath) }
        $locRc = Invoke-Layers $locArgs
        Write-Host ("locate exit: {0}" -f $locRc)
    }
}
finally {
    if ($serviceCreated) {
        Write-Host ">> sc stop/delete $ServiceName"
        & sc.exe stop $ServiceName | Out-Host
        & sc.exe delete $ServiceName | Out-Host
    }
}

$after = Get-MinidumpState
Write-Host ("minidumps after: count={0} latest={1}" -f $after.count, $after.latest)
if ($after.count -ne $before.count -or $after.latest -ne $before.latest) {
    throw "NEW BSOD minidump detected - aborting dynamic verification"
}
Write-Host "DYNAMIC_VERIFY=DONE"
