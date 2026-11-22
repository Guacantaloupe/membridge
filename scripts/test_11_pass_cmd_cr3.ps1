# Test the 11 real-PASS drivers independently obtaining cmd.exe CR3.
#
# The 11 IDs are the backends that PASSed the real EXE-memory read test
# (mrt and winring0x64 are excluded: mrt needs SYSTEM ACL, winring0x64 is
# low-memory-only).
#
# Each backend performs its own physical EPROCESS scan with its own driver:
#   membridge --backend <id> --locate --pid <cmd_pid> --name cmd.exe \
#       --scan-start <start> --scan-size <size>
# and must print CR3=0x...
#
# Usage:
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -StaticOnly
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -DryRun
#   # elevated, with cmd.exe open:
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1
#   # faster targeted range:
#   pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -ScanStart 0x100000 -ScanSize 0x40000000
#
# WARNING: full physical scan is slow (GBs per backend). Use -MaxRanges or
# -ScanStart/-ScanSize to bound the work. Loading 11 kernel drivers can BSOD
# on a test box only; minidumps are checked before/after.
[CmdletBinding()]
param(
    [switch]$StaticOnly,
    [switch]$DryRun,
    [string]$TargetName = 'cmd.exe',
    [string]$ScanStart = '',
    [string]$ScanSize = '',
    [int]$MaxRanges = 0,
    [int]$MaxBackends = 0,
    [switch]$NoAutoStart,
    [switch]$KeepTarget,
    [switch]$CheckPrereqs,
    [switch]$AllowUnsafeCchwapi
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$layers = Join-Path $base 'bin\layers.exe'
$outJson = Join-Path $base 'evidence\test_11_pass_cmd_cr3.json'

$backends = @(
    'cchwapi_2_9', 'cchwapi_2_12', 'cchwapi_ext_4_5',
    'hp_swtools', 'swtoolsdriver', 'hp_swtools_v1', 'isharepp',
    'gibepext', 'gibepext2', 'tdeio', 'ufu_win10'
)
$unsafeCchwapi = @('cchwapi_2_9', 'cchwapi_2_12', 'cchwapi_ext_4_5')
if (-not $AllowUnsafeCchwapi) {
    $backends = @($backends | Where-Object { $unsafeCchwapi -notcontains $_ })
    Write-Warning "CCHWApi family excluded by default (CCHWApi.sys 2.9 caused BSOD 0x3B/AV_CCHWApi during physical scan). Use -AllowUnsafeCchwapi only on a disposable test VM."
}
$driverPaths = @{
    'cchwapi_2_9'     = 'drivers\CCHWApi_sys\CCHWApi.sys'
    'cchwapi_2_12'    = 'drivers\CCHWApi(1)_sys\CCHWApi(1).sys'
    'cchwapi_ext_4_5' = 'drivers\CCHWApiExt(1)_sys\CCHWApiExt(1).sys'
    'hp_swtools'      = 'drivers\HP SW TOOLS DRIVER_sys\HP SW TOOLS DRIVER.sys'
    'swtoolsdriver'   = 'drivers\swtoolsdriver\swtoolsdriver.sys'
    'hp_swtools_v1'   = 'drivers\hp_swtools_v1\HP_SWTOOLS_DRIVER_V1.sys'
    'isharepp'        = 'drivers\isharepp\iSharePp.sys'
    'gibepext'        = 'drivers\gibepext\gibepext.sys'
    'gibepext2'       = 'drivers\gibepext2\gibepext2.sys'
    'tdeio'           = 'drivers\tdeio\TdeIo64.sys'
    'ufu_win10'       = 'drivers\ufu\win10_ufudev64.sys'
}

foreach ($p in @($membridge, $layers)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "missing: $p" }
}
$list = (& $membridge --list 2>&1 | Out-String)
$missing = @($backends | Where-Object { $list -notmatch [regex]::Escape($_) })
if ($missing.Count) { throw "missing backend IDs: $($missing -join ',')" }
Write-Host "== 11 PASS drivers -> cmd.exe CR3 =="
Write-Host "backends: $($backends.Count) target=$TargetName"

function Get-Ranges {
    if ($ScanStart -and $ScanSize) {
        return @([pscustomobject]@{ start = $ScanStart; size = $ScanSize })
    }
    $text = (& $layers --ranges 2>&1 | Out-String)
    $rs = @()
    foreach ($m in [regex]::Matches($text, '0x([0-9A-Fa-f]+)\s*-\s*0x([0-9A-Fa-f]+)\s+len=0x([0-9A-Fa-f]+)')) {
        $rs += [pscustomobject]@{
            start = '0x' + $m.Groups[1].Value
            size  = '0x' + $m.Groups[3].Value
        }
    }
    if ($MaxRanges -gt 0) { $rs = @($rs | Select-Object -First $MaxRanges) }
    return $rs
}

$ranges = @(Get-Ranges)
if ($MaxBackends -gt 0) { $backends = @($backends | Select-Object -First $MaxBackends) }

if ($CheckPrereqs) {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($id)
    $isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $cc = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl' -ErrorAction SilentlyContinue
    $disk = Get-PSDrive C
    $targetProcess = Get-Process -Name ($TargetName -replace '\.exe$', '') -ErrorAction SilentlyContinue | Select-Object -First 1
    $ls = (& $membridge --locate-selftest 2>&1 | Out-String)
    $rows = @()
    foreach ($bid in $backends) {
        $path = Join-Path $base $driverPaths[$bid]
        $exists = Test-Path -LiteralPath $path
        $sig = if ($exists) { (Get-AuthenticodeSignature -LiteralPath $path).Status.ToString() } else { 'MISSING' }
        $rows += [pscustomobject]@{ backend = $bid; driver = $driverPaths[$bid]; exists = $exists; signature = $sig }
    }
    $allPresent = @($rows | Where-Object { -not $_.exists }).Count -eq 0
    $allValid = @($rows | Where-Object { $_.signature -ne 'Valid' }).Count -eq 0
    $selftestOk = $ls -match 'VERDICT=PASS'
    $report = [pscustomobject]@{
        date = (Get-Date).ToString('s')
        is_admin = $isAdmin
        crash_dump_enabled = $cc.CrashDumpEnabled
        auto_reboot = $cc.AutoReboot
        minidump_dir = $cc.MinidumpDir
        disk_free_gb = [math]::Round($disk.Free / 1GB, 2)
        target_running = [bool]$targetProcess
        locate_selftest = if ($selftestOk) { 'PASS' } else { 'FAIL' }
        drivers_present = $allPresent
        drivers_signed = $allValid
        drivers = $rows
        dynamic_blocker = if ($isAdmin) { '' } else { 'Administrator required to load kernel drivers' }
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $base 'evidence\test_11_pass_cmd_cr3_prereqs.json') -Encoding UTF8
    $rows | Format-Table -AutoSize
    Write-Host ("PREREQ: admin={0} dump={1} autoreboot={2} freeGB={3} target={4} locate_selftest={5}" -f `
        $isAdmin, $cc.CrashDumpEnabled, $cc.AutoReboot, $report.disk_free_gb, $report.target_running, $report.locate_selftest)
    Write-Host ("DRIVERS: present={0} signed={1}" -f $allPresent, $allValid)
    if ($allPresent -and $allValid -and $selftestOk) {
        Write-Host "CHECK_PREREQS=PASS"
        exit 0
    }
    exit 2
}

if ($StaticOnly) {
    $ls = (& $membridge --locate-selftest 2>&1 | Out-String)
    if ($ls -notmatch 'VERDICT=PASS') {
        Write-Host $ls
        throw "membridge --locate-selftest failed"
    }
    Write-Host "locate selftest: PASS"
    $report = [pscustomobject]@{
        mode = 'static'
        backends = $backends
        ranges = $ranges
        locate_selftest = 'PASS'
        note = 'dynamic CR3 test requires Administrator and a running cmd.exe'
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
    Write-Host "STATIC_ONLY=PASS"
    exit 0
}

if ($DryRun) {
    Write-Host "ranges:"
    $ranges | ForEach-Object { Write-Host ("  {0} + {1}" -f $_.start, $_.size) }
    Write-Host "commands:"
    foreach ($id in $backends) {
        foreach ($r in $ranges) {
            Write-Host ("  membridge --backend {0} --locate --pid <cmd_pid> --name {1} --scan-start {2} --scan-size {3}" -f $id, $TargetName, $r.start, $r.size)
        }
    }
    Write-Host "DRY_RUN=PASS"
    exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load the 11 drivers." -ForegroundColor Yellow
    exit 2
}

$procName = $TargetName -replace '\.exe$', ''
$target = Get-Process -Name $procName -ErrorAction SilentlyContinue | Select-Object -First 1
$startedTarget = $false
if (-not $target) {
    if ($NoAutoStart) {
        throw "target process not running: $TargetName (start cmd.exe first, or omit -NoAutoStart)"
    }
    Write-Host "no $TargetName running; starting one..."
    $cmdExe = Join-Path $env:SystemRoot 'System32\cmd.exe'
    $target = Start-Process -FilePath $cmdExe -ArgumentList '/k', 'title MembridgeCmdTarget' -PassThru
    Start-Sleep -Milliseconds 600
    $startedTarget = $true
}
if (-not $target) { throw "failed to start target: $TargetName" }

$dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$results = @()
foreach ($bid in $backends) {
    Write-Host ">> $bid"
    $found = $null
    $last = ''
    foreach ($r in $ranges) {
        $argsList = @('--backend', $bid, '--locate', '--pid', $target.Id.ToString(),
                      '--name', $TargetName, '--scan-start', $r.start, '--scan-size', $r.size)
        $out = (& $membridge @argsList 2>&1 | Out-String)
        $last = $out
        $m = [regex]::Match($out, 'CR3=(0x[0-9A-Fa-f]+)')
        if ($m.Success) {
            $found = $m.Groups[1].Value
            break
        }
    }
    $results += [pscustomobject]@{
        backend = $bid
        cr3 = if ($found) { $found } else { '' }
        verdict = if ($found) { 'PASS' } else { 'LOCATE_FAILED' }
        last_output = $last.Trim()
    }
    Write-Host ("   => {0} {1}" -f $results[-1].verdict, $results[-1].cr3)
}

$dumpsAfter = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
$pass = @($results | Where-Object { $_.verdict -eq 'PASS' }).Count
$report = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    target_pid = $target.Id
    target = $TargetName
    total = $results.Count
    pass = $pass
    results = $results
    new_dumps = ($newDumps -join ',')
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outJson -Encoding UTF8
$results | Select-Object backend, verdict, cr3 | Format-Table -AutoSize
Write-Host ("RESULT: {0}/{1} CR3 PASS -> {2}" -f $pass, $results.Count, $outJson)
if ($startedTarget -and -not $KeepTarget) {
    Stop-Process -Id $target.Id -Force -ErrorAction SilentlyContinue
    Write-Host "stopped auto-started $TargetName (PID=$($target.Id))"
}
if ($pass -eq $results.Count) { exit 0 } else { exit 2 }
