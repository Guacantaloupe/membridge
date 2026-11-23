# Resume the CR3 research session after logon/reboot.
# This script intentionally does NOT load or run any driver.
# It only:
#   - ensures DeepSeek Harness is running
#   - ensures the cmd.exe CR3 target exists
#   - writes a resume marker for the agent/session
#   - logs boot/autostart state
param(
    [switch]$NoStartHarness,
    [switch]$NoStartTarget
)

$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$evidence = Join-Path $base 'evidence'
$log = Join-Path $evidence 'resume_cr3_goal.log'
$stateFile = Join-Path $evidence 'resume_cr3_goal_state.json'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null

function Write-Log([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date).ToString('s'), $msg
    Add-Content -LiteralPath $log -Value $line -Encoding UTF8
}

$boot = $null
try { $boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime } catch {}

Write-Log "resume begin user=$env:USERNAME boot=$boot"

$dshExe = 'C:\Users\m4a1_\AppData\Local\Programs\DeepSeek Harness\DeepSeek Harness.exe'
$dshRunning = [bool](Get-Process -Name 'DeepSeek Harness' -ErrorAction SilentlyContinue)
if (-not $NoStartHarness) {
    if (Test-Path -LiteralPath $dshExe) {
        if (-not $dshRunning) {
            try {
                Start-Process -FilePath $dshExe -WorkingDirectory (Split-Path $dshExe -Parent)
                Write-Log "started DSH"
                $dshRunning = $true
            } catch {
                Write-Log "DSH start failed: $($_.Exception.Message)"
            }
        } else {
            Write-Log "DSH already running"
        }
    } else {
        Write-Log "DSH executable missing: $dshExe"
    }
}

$cmdRunning = [bool](Get-Process -Name cmd -ErrorAction SilentlyContinue)
if (-not $NoStartTarget) {
    if (-not $cmdRunning) {
        try {
            Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\cmd.exe') -ArgumentList '/k','title CR3 target'
            Write-Log "started cmd target"
            Start-Sleep -Milliseconds 800
        } catch {
            Write-Log "cmd start failed: $($_.Exception.Message)"
        }
    } else {
        Write-Log "cmd target already running"
    }
}

# Mark the session as resumable. The actual goal string is informational;
# DSH owns the persisted session/goal state.
$marker = [pscustomobject]@{
    time = (Get-Date).ToString('s')
    user = $env:USERNAME
    boot = "$boot"
    objective = 'Find more CR3-capable driver paths; do not stop'
    dsh_exe = $dshExe
    dsh_running = [bool](Get-Process -Name 'DeepSeek Harness' -ErrorAction SilentlyContinue)
    cmd_running = [bool](Get-Process -Name cmd -ErrorAction SilentlyContinue)
    note = 'resume helper only; does not load drivers'
}
$marker | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $stateFile -Encoding UTF8
Write-Log "resume end dsh=$($marker.dsh_running) cmd=$($marker.cmd_running)"
