# Install/start a provider driver (ADMIN) with HVCI-aware error hints.
#
#   pwsh -File scripts\install_driver.ps1 -DryRun -DriverPath .\drivers\aswarpot\aswArPot.sys -ServiceName aswArPot
#   pwsh -File scripts\install_driver.ps1 -DriverPath .\drivers\aswarpot\aswArPot.sys -ServiceName aswArPot
#   pwsh -File scripts\install_driver.ps1 -DriverPath .\drivers\mrt\fekern_mrt.sys -ServiceName fekern_mrt -CopyToSystem32
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DriverPath,
    [Parameter(Mandatory = $true)][string]$ServiceName,
    [string]$DevicePath = '',
    [string]$LayersPath = '',
    [switch]$CopyToSystem32,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
if (-not $LayersPath) { $LayersPath = Join-Path $base 'bin\layers.exe' }
$DriverPath = (Resolve-Path -LiteralPath $DriverPath -ErrorAction Stop).Path

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

$hvci = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity' -ErrorAction SilentlyContinue).Enabled
$blocklist = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -ErrorAction SilentlyContinue).VulnerableDriverBlocklistEnable
if ($null -eq $blocklist) { $blocklist = 'absent(default-may-enforce)' }
Write-Host "== install driver =="
Write-Host "driver=$DriverPath"
Write-Host "service=$ServiceName device=$DevicePath"
Write-Host "HVCI=$hvci vulnerable-driver-blocklist=$blocklist"
Write-Host "DryRun=$DryRun CopyToSystem32=$CopyToSystem32"

$target = $DriverPath
if ($CopyToSystem32) {
    $target = Join-Path $env:SystemRoot ('System32\drivers\' + [IO.Path]::GetFileName($DriverPath))
    Write-Host "copy: $DriverPath -> $target"
    if (-not $DryRun) {
        if (-not (Test-Admin)) { throw "CopyToSystem32 requires Administrator" }
        Copy-Item -LiteralPath $DriverPath -Destination $target -Force
    }
} elseif (-not $DryRun -and -not (Test-Admin)) {
    throw "installing a kernel service requires Administrator (use -DryRun for a safe preview)"
}

if ($DryRun) {
    Write-Host ">> sc.exe create `"$ServiceName`" type= kernel start= demand binPath= `"$target`""
    Write-Host ">> sc.exe start `"$ServiceName`""
    if (Test-Path $LayersPath) {
        Write-Host ">> `"$LayersPath`" --probe-readers"
        if ($DevicePath) { Write-Host ">> `"$LayersPath`" --reader aswarpot --device `"$DevicePath`" --read-phys --pa 0x10000000 --size 16" }
    }
    Write-Host "DRY_RUN=DONE"
    return
}

& sc.exe create $ServiceName binPath= $target type= kernel start= demand | Out-Host
if ($LASTEXITCODE -ne 0) { throw "sc create failed: $LASTEXITCODE" }
& sc.exe start $ServiceName | Out-Host
$rc = $LASTEXITCODE
if ($rc -ne 0) {
    Write-Host "[WARN] sc start failed rc=$rc. HVCI/code-integrity pointers:" -ForegroundColor Yellow
    Write-Host "  577 / 0xC0000365 : signature/code integrity/HVCI-incompatible (W^X, unsigned, revoked)"
    Write-Host "  1275 / 0xC0000428: blocked by policy / vulnerable-driver blocklist"
    Write-Host "  2 / 3             : driver file/service configuration"
    Write-Host "  Use layers.exe --list-providers and docs/hvci.md."
    Write-Host "  Cleanup: sc.exe delete $ServiceName"
    exit $rc
}

if (Test-Path $LayersPath) {
    Write-Host ">> $LayersPath --probe-readers"
    & $LayersPath --probe-readers
}
Write-Host "INSTALL=DONE (device: $DevicePath)"
Write-Host "Stop/remove: sc.exe stop $ServiceName; sc.exe delete $ServiceName"
