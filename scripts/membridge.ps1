# membridge.ps1 — unified physical-memory read framework with pluggable BYOV drivers.
# Actions:
#   list                               show available driver backends
#   read   -Backend <id> -Pa <hex> ... read physical memory through selected driver
#   selftest [-Backends id,id,...]     AWE-based end-to-end self-test of backends
#
# Example:
#   .\scripts\membridge.ps1 -Action read -Backend sjtwinio -Pa 0x1E01C6000 -Size 0x100 -Dump
#   .\scripts\membridge.ps1 -Action selftest
param(
    [ValidateSet('list', 'read', 'selftest')][string]$Action = 'list',
    [string]$Backend = 'cchwapi_2_12',
    [string]$Pa = '0x100000',
    [int]$Size = 256,
    [string]$Expect = '',
    [switch]$Dump,
    [string]$Backends = '',
    [string]$DriverPath = ''
)
$ErrorActionPreference = 'Continue'
$base = Split-Path -Parent $PSScriptRoot   # ...\msdriver-0day
$env:MEMBRIDGE_BASE = $base
$exe = Join-Path $base 'bin\membridge.exe'
$children = $PSScriptRoot

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

switch ($Action) {
    'list' {
        & $exe --list
    }
    'read' {
        if (-not (Test-Path $exe)) { Write-Error "framework exe not found: $exe"; exit 1 }
        $logPath = Join-Path $base 'evidence\last_read.log'
        $exeArgs = @('--backend', $Backend, '--pa', $Pa, '--size', ('0x{0:X}' -f $Size))
        if ($Expect) { $exeArgs += @('--expect', $Expect) }
        if ($Dump) { $exeArgs += '--dump' }
        if ($DriverPath) { $exeArgs += @('--driver-path', $DriverPath) }
        if (Test-Admin) {
            & $exe @exeArgs 2>&1 | Tee-Object -FilePath $logPath
            exit $LASTEXITCODE
        } else {
            $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$children\_elev_membridge_read.ps1`"",
                '-Backend', $Backend, '-Pa', $Pa, '-Size', "$Size", '-LogPath', "`"$logPath`"")
            if ($Expect) { $argList += @('-Expect', "`"$Expect`"") }
            if ($Dump) { $argList += '-Dump' }
            if ($DriverPath) { $argList += @('-DriverPath', "`"$DriverPath`"") }
            Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList $argList -Wait -WindowStyle Hidden
            Start-Sleep -Milliseconds 200
            if (Test-Path $logPath) {
                Get-Content $logPath -Encoding UTF8
                $code = (Get-Content $logPath -Encoding UTF8 | Select-String 'EXITCODE=(\d+)').Matches.Groups[1].Value
                exit ([int]$code)
            } else { Write-Error "no output from elevated read"; exit 7 }
        }
    }
    'selftest' {
        $log = Join-Path $base 'evidence\_elev_membridge_selftest.log'
        if (Test-Path $log) { Remove-Item $log -Force }
        if ($Backends) { $argList += @('-Backends', "`"$Backends`"") }
        $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
            "`"$children\_elev_membridge_selftest.ps1`"")
        if (-not (Test-Admin)) {
            Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList $argList -Wait -WindowStyle Hidden
        } else {
            & powershell.exe @argList | Out-Null
        }
        Start-Sleep -Milliseconds 500
        if (Test-Path $log) { Get-Content $log -Encoding UTF8 }
        $res = Join-Path $base 'evidence\membridge_selftest.json'
        if (Test-Path $res) { "`n=== selftest json ==="; Get-Content $res -Raw -Encoding UTF8 }
    }
}
