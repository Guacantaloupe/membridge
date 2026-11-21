param(
    [Parameter(Mandatory = $true)][string]$Backend,
    [string]$Pa = '0x100000',
    [int]$Size = 256,
    [string]$Expect = '',
    [switch]$Dump,
    [string]$DriverPath = '',
    [Parameter(Mandatory = $true)][string]$LogPath
)
$base = Split-Path -Parent $PSScriptRoot
$env:MEMBRIDGE_BASE = $base
$exe = Join-Path $base 'bin\membridge.exe'
$exeArgs = @('--backend', $Backend, '--pa', $Pa, '--size', ('0x{0:X}' -f $Size))
if ($Expect) { $exeArgs += @('--expect', $Expect) }
if ($Dump) { $exeArgs += '--dump' }
if ($DriverPath) { $exeArgs += @('--driver-path', $DriverPath) }
$out = & $exe @exeArgs 2>&1 | Out-String
$out | Out-File -FilePath $LogPath -Encoding UTF8
"EXITCODE=$LASTEXITCODE" | Out-File -FilePath $LogPath -Append -Encoding UTF8
exit $LASTEXITCODE
