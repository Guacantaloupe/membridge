# Install the DSH autostart scheduled task (must be run elevated).
# RunLevel Highest means the task gets an elevated token at logon without
# changing UAC settings (EnableLUA stays 1).
param(
    [string]$TaskName = 'DSH_Harness_Autostart',
    [switch]$StartTarget
)

$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Administrator required: run this script from an elevated PowerShell.'
}

$base = Split-Path $PSScriptRoot -Parent
$script = Join-Path $PSScriptRoot 'autostart_dsh.ps1'
if (-not (Test-Path -LiteralPath $script)) { throw "missing: $script" }

$user = "$env:USERDOMAIN\$env:USERNAME"
$actionArgs = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`""
if ($StartTarget) { $actionArgs += ' -StartTarget' }

$action = New-ScheduledTaskAction -Execute "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
    -Argument $actionArgs -WorkingDirectory $base
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew -StartWhenAvailable

$task = New-ScheduledTask -Action $action -Trigger $trigger -Settings $settings `
    -Description 'Start DeepSeek Harness (and optional cmd target) at logon with highest privileges.'
Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Settings $settings `
    -User $user -RunLevel Highest `
    -Description 'Start DeepSeek Harness (and optional cmd target) at logon with highest privileges.' -Force | Out-Null
Write-Host "installed task: $TaskName"
Get-ScheduledTask -TaskName $TaskName | Select-Object TaskName,State
