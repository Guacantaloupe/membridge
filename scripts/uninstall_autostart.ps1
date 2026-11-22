# Remove the DSH autostart scheduled task (elevated).
param([string]$TaskName = 'DSH_Harness_Autostart')
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Administrator required: run this script from an elevated PowerShell.'
}
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    Write-Host "removed task: $TaskName"
} else {
    Write-Host "task not found: $TaskName"
}
