# Install/remove the crash-aware one-driver-per-boot CR3 runner.
# Must run elevated.
param(
    [string]$TaskName = 'DSH_Cr3_BootRunner',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Administrator required: run this script from an elevated PowerShell.'
}
if ($Remove) {
    if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
        Write-Host "removed: $TaskName"
    } else { Write-Host "not found: $TaskName" }
    exit 0
}

$base = Split-Path $PSScriptRoot -Parent
$runner = Join-Path $PSScriptRoot 'boot_cr3_runner.ps1'
if (-not (Test-Path -LiteralPath $runner)) { throw "missing: $runner" }
$user = "$env:USERDOMAIN\$env:USERNAME"
$action = New-ScheduledTaskAction -Execute "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
    -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$runner`"" `
    -WorkingDirectory $base
$trigger = New-ScheduledTaskTrigger -AtStartup
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::FromMinutes(45)) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Settings $settings `
    -User 'NT AUTHORITY\SYSTEM' -RunLevel Highest `
    -Description 'One physical driver per boot; crash-aware CR3 acquisition (SYSTEM).' -Force | Out-Null
Write-Host "installed task: $TaskName"
Get-ScheduledTask -TaskName $TaskName | Select-Object TaskName,State
