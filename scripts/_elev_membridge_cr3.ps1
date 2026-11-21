param(
    [int]$ProcessId = 0,
    [string]$Module = '',
    [string]$Va = '',
    [int]$Size = 64,
    [switch]$Dump,
    [switch]$Compare,
    [string]$LogPath = ''
)
$ErrorActionPreference = 'Continue'
$base = Split-Path -Parent $PSScriptRoot          # ...\membridge
$exe = Join-Path $base 'bin\cr3read.exe'
$kkyum = Join-Path $base 'drivers\KKYUM\KKYUM.sys'
$eneio = Join-Path $base 'drivers\eneio64\eneio64.sys'
if (-not (Test-Path $exe)) { "cr3read.exe not found: $exe" | Tee-Object -FilePath $LogPath; "EXITCODE=7" | Tee-Object -FilePath $LogPath -Append; exit 7 }
$argList = @('--pid', "$ProcessId", '--size', ('0x{0:X}' -f $Size),
             '--kkyum-driver', "$kkyum", '--phys-driver', "$eneio", '--phys-mode', 'eneio64')
if ($Module) { $argList += @('--module', $Module) }
elseif ($Va) { $argList += @('--va', $Va) }
if ($Dump) { $argList += '--dump' }
if ($Compare) { $argList += '--compare' }
& $exe @argList 2>&1 | Tee-Object -FilePath $LogPath
"EXITCODE=$LASTEXITCODE" | Tee-Object -FilePath $LogPath -Append
