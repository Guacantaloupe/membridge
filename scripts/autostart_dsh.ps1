# Auto-start DeepSeek Harness and the elevated cmd.exe target after logon.
# Driver tests are deliberately NOT auto-run here; the agent runs them explicitly.
param(
    [switch]$StartTarget
)

$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$stateDir = Join-Path $base 'evidence'
$log = Join-Path $stateDir 'autostart.log'
$stateFile = Join-Path $stateDir 'autostart_state.json'
New-Item -ItemType Directory -Force -Path $stateDir | Out-Null

function Write-Log([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date).ToString('s'), $msg
    Add-Content -LiteralPath $log -Value $line -Encoding UTF8
}

Write-Log "autostart begin user=$env:USERNAME boot=$((Get-CimInstance Win32_OperatingSystem).LastBootUpTime)"

$dshExe = 'C:\Users\m4a1_\AppData\Local\Programs\DeepSeek Harness\DeepSeek Harness.exe'
if (-not (Test-Path -LiteralPath $dshExe)) {
    Write-Log "DSH executable missing: $dshExe"
} elseif (-not (Get-Process -Name 'DeepSeek Harness' -ErrorAction SilentlyContinue)) {
    try {
        Start-Process -FilePath $dshExe -WorkingDirectory (Split-Path $dshExe -Parent)
        Write-Log "started DSH: $dshExe"
    } catch {
        Write-Log "DSH start failed: $($_.Exception.Message)"
    }
} else {
    Write-Log "DSH already running"
}

if ($StartTarget) {
    $cmd = Get-Process -Name cmd -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $cmd) {
        try {
            Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\cmd.exe') -ArgumentList '/k','title CR3 target'
            Write-Log "started cmd target"
        } catch {
            Write-Log "cmd start failed: $($_.Exception.Message)"
        }
    } else {
        Write-Log "cmd target already running pid=$($cmd.Id)"
    }
}

$state = [pscustomobject]@{
    time = (Get-Date).ToString('s')
    user = $env:USERNAME
    boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
    dsh_running = [bool](Get-Process -Name 'DeepSeek Harness' -ErrorAction SilentlyContinue)
    cmd_running = [bool](Get-Process -Name cmd -ErrorAction SilentlyContinue)
}
$state | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $stateFile -Encoding UTF8
Write-Log "autostart end"
