# Crash-aware one-driver-per-boot CR3 acquisition runner.
#
# At each logon, this attempts the next physical driver that has not completed:
#   membridge --backend <id> --locate --pid <cmd> --name cmd.exe --scan-start ... --scan-size ...
# If the machine BSODs during a scan, the next boot marks that backend CRASHED and
# moves to the next one. After 3 consecutive crashes it stops itself.
param(
    [switch]$Once,
    [int]$MaxConsecutiveCrashes = 3,
    [string]$ScanStart = '0xA73000',
    [string]$ScanSize = '0x6CC78000'
)

$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$outDir = Join-Path $base 'evidence'
$progressFile = Join-Path $outDir 'boot_cr3_progress.json'
$logFile = Join-Path $outDir 'boot_cr3_runner.log'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Write-RunLog([string]$msg) {
    Add-Content -LiteralPath $logFile -Value ("[{0}] {1}" -f (Get-Date).ToString('s'), $msg) -Encoding UTF8
}

$isAdmin = (New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Write-RunLog ("runner start user={0} admin={1}" -f $env:USERNAME, $isAdmin)

$backends = @(
    'gibepext','gibepext2','tdeio','ufu_win10','isharepp',
    'swtoolsdriver','hp_swtools','hp_swtools_v1',
    'cchwapi_2_12','cchwapi_ext_4_5','cchwapi_2_9'
)

# Load/init progress.
if (Test-Path $progressFile) {
    try { $progress = Get-Content $progressFile -Raw -Encoding UTF8 | ConvertFrom-Json } catch { $progress = $null }
}
if (-not $progress) {
    $progress = [pscustomobject]@{
        created = (Get-Date).ToString('s')
        results = @()
        running = $null
        consecutive_crashes = 0
        blocked = $false
    }
}
$boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime

# Previous running attempt without finish + new boot => crashed.
if ($progress.running) {
    $r = $progress.running
    $results = @($progress.results)
    $results += [pscustomobject]@{
        backend = $r.backend
        status = 'CRASHED'
        started = $r.started
        finished = (Get-Date).ToString('s')
        boot = "$boot"
        note = 'no finish marker after reboot'
    }
    $progress.results = $results
    $progress.running = $null
    $progress.consecutive_crashes = [int]$progress.consecutive_crashes + 1
    Write-RunLog ("marked CRASHED backend={0} consecutive={1}" -f $r.backend, $progress.consecutive_crashes)
}
if ([int]$progress.consecutive_crashes -ge $MaxConsecutiveCrashes) {
    $progress.blocked = $true
    $progress | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $progressFile -Encoding UTF8
    Write-RunLog "blocked after $MaxConsecutiveCrashes consecutive crashes"
    exit 2
}
if ($progress.blocked) { Write-RunLog 'already blocked'; exit 2 }

$done = @($progress.results | ForEach-Object { $_.backend })
$next = $backends | Where-Object { $done -notcontains $_ } | Select-Object -First 1
if (-not $next) {
    Write-RunLog 'all backends attempted'
    $progress | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $progressFile -Encoding UTF8
    exit 0
}

# Target process.
$target = Get-Process -Name cmd -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $target) {
    Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\cmd.exe') -ArgumentList '/k','title CR3 boot target' | Out-Null
    Start-Sleep -Seconds 1
    $target = Get-Process -Name cmd -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $target) { Write-RunLog 'no cmd target'; exit 3 }

Write-RunLog ("attempt backend={0} pid={1} scan={2}+{3}" -f $next,$target.Id,$ScanStart,$ScanSize)
$progress.running = [pscustomobject]@{
    backend=$next
    pid=$target.Id
    started=(Get-Date).ToString('s')
    boot="$boot"
}
$progress | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $progressFile -Encoding UTF8

$mbArgs = @('--backend',$next,'--locate','--pid',"$($target.Id)",'--name','cmd.exe',
            '--scan-start',$ScanStart,'--scan-size',$ScanSize)
$ErrorActionPreference = 'Continue'
$output = (& $membridge @mbArgs 2>&1 | Out-String)
$code = $LASTEXITCODE
if ($output) { Write-RunLog ("output backend={0}: {1}" -f $next, (($output -split "`r?`n" | Select-Object -Last 8) -join ' | ')) }
$status = if ($code -eq 0) { 'COMPLETED' } else { "FAILED($code)" }

$results = @($progress.results)
$results += [pscustomobject]@{
    backend=$next
    status=$status
    started=$progress.running.started
    finished=(Get-Date).ToString('s')
    boot="$boot"
}
$progress.results = $results
$progress.running = $null
if ($status -like 'FAILED*') { $progress.consecutive_crashes = [int]$progress.consecutive_crashes + 1 }
else { $progress.consecutive_crashes = 0 }
$progress | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $progressFile -Encoding UTF8
Write-RunLog ("finished backend={0} status={1}" -f $next,$status)
