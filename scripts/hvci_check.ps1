# HVCI driver audit wrapper: signature + W^X section hazards for .sys files.
# Usage: pwsh -File scripts\hvci_check.ps1 [-Path C:\path\to\drivers]
[CmdletBinding()]
param(
    [string]$Path = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$py = (Get-Command python -ErrorAction Stop).Source
if (-not $Path) { $Path = Join-Path $repoRoot 'drivers' }
& $py (Join-Path $PSScriptRoot 'hvci_check.py') $Path
exit $LASTEXITCODE
