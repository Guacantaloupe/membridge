# Safe 11-driver CR3 acquisition without full physical scans.
#
# Idea:
#   1) Use KKYUM.sys (WHQL, HVCI-PASS, kernel-VA reader) to get cmd.exe CR3
#      and EPROCESS kernel VA. No physical RAM scan.
#   2) Translate EPROCESS VA -> PA with a small page-table walk (tdeio legacy
#      backend, only 4-5 tiny reads). No scan.
#   3) Each of the 11 drivers performs ONE 8-byte physical read at
#      EPROCESS_PA+0x28 and reads the CR3 value itself.
#
# Thus every driver independently gets CR3, but no driver scans GBs of RAM.
# CCHWApi crashed during full-range scanning; here it only reads 8 bytes.
#
# Usage:
#   pwsh -File scripts\safe_11_cr3.ps1 -StaticOnly
#   pwsh -File scripts\safe_11_cr3.ps1 -DryRun
#   # elevated:
#   pwsh -File scripts\safe_11_cr3.ps1
param(
    [switch]$StaticOnly,
    [switch]$DryRun,
    [int]$ProcessId = 0,
    [string]$TargetName = 'cmd.exe',
    [string]$OnlyBackend = '',
    [switch]$KeepTarget,
    [switch]$KeepDriver
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$layers = Join-Path $base 'bin\layers.exe'
$kkyum = Join-Path $base 'drivers\KKYUM\KKYUM.sys'
$outJson = Join-Path $base 'evidence\safe_11_cr3.json'
if ($OnlyBackend) { $outJson = Join-Path $base ("evidence\safe_11_cr3_{0}.json" -f $OnlyBackend) }

$backends = @(
    'cchwapi_2_9', 'cchwapi_2_12', 'cchwapi_ext_4_5',
    'hp_swtools', 'swtoolsdriver', 'hp_swtools_v1', 'isharepp',
    'gibepext', 'gibepext2', 'tdeio', 'ufu_win10'
)
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

foreach ($p in @($membridge,$layers,$kkyum)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "missing: $p" }
}
if ($OnlyBackend) {
    if ($backends -notcontains $OnlyBackend) { throw "unknown -OnlyBackend; use: $($backends -join ',')" }
    $backends = @($OnlyBackend)
}
$list = (& $membridge --list 2>&1 | Out-String)
$missing = @($backends | Where-Object { $list -notmatch [regex]::Escape($_) })
if ($missing.Count) { throw "missing backends: $($missing -join ',')" }

Write-Host "== safe 11-driver CR3 (no full scan) =="
Write-Host "backends: $($backends.Count)"

if ($StaticOnly) {
    $rows = @()
    foreach ($bid in $backends) {
        $path = Join-Path $base $driverPaths[$bid]
        $exists = Test-Path -LiteralPath $path
        $sig = if ($exists) { (Get-AuthenticodeSignature -LiteralPath $path).Status.ToString() } else { 'MISSING' }
        $rows += [pscustomobject]@{ backend=$bid; exists=$exists; signature=$sig }
    }
    $ls = (& $membridge --locate-selftest 2>&1 | Out-String)
    $report = [pscustomobject]@{
        mode='static'
        kkyum = (Get-AuthenticodeSignature -LiteralPath $kkyum).Status.ToString()
        locate_selftest = if ($ls -match 'VERDICT=PASS') {'PASS'} else {'FAIL'}
        backends = $rows
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
    $rows | Format-Table -AutoSize
    Write-Host ("KKYUM={0} locate_selftest={1}" -f $report.kkyum, $report.locate_selftest)
    if (@($rows | Where-Object { -not $_.exists -or $_.signature -ne 'Valid' }).Count -eq 0 -and $report.locate_selftest -eq 'PASS') {
        Write-Host "STATIC_ONLY=PASS"; exit 0
    }
    exit 2
}

if ($DryRun) {
    Write-Host "1) sc create/start KKYUM"
    Write-Host "2) layers --locate --kkyum --pid <cmd_pid>   (CR3 + EPROCESS VA, kernel-VA walk; no physical scan)"
    Write-Host "3) layers --translate-only --reader legacy --legacy-backend tdeio --cr3 <CR3> --va <EPROCESS_VA>  (small page-table walk only)"
    Write-Host "4) for each backend: membridge --backend <id> --pa <EPROCESS_PA+0x28> --size 8  (one 8-byte read)"
    Write-Host "DRY_RUN=PASS"; exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load KKYUM and the 11 drivers." -ForegroundColor Yellow
    exit 2
}

$startedTarget = $false
$service = 'MembridgeKKYUM'
$serviceCreated = $false
function Stop-Kkyum {
    if ($serviceCreated -and -not $KeepDriver) {
        & sc.exe stop $service *> $null
        Start-Sleep -Milliseconds 300
        & sc.exe delete $service *> $null
    }
}

try {
    if ($ProcessId -le 0) {
        $procName = $TargetName -replace '\.exe$',''
        $proc = Get-Process -Name $procName -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $proc) {
            $cmdExe = Join-Path $env:SystemRoot 'System32\cmd.exe'
            $proc = Start-Process -FilePath $cmdExe -ArgumentList '/k','title MembridgeSafeCR3' -PassThru
            $startedTarget = $true
            Start-Sleep -Milliseconds 600
        }
        $ProcessId = $proc.Id
    }
    Write-Host "target PID=$ProcessId"
    $dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)

    Write-Host ">> loading KKYUM..."
    & sc.exe create $service binPath= $kkyum type= kernel start= demand *> $null
    if ($LASTEXITCODE -ne 0) {
        # maybe already exists
        & sc.exe start $service *> $null
    } else {
        $serviceCreated = $true
        & sc.exe start $service *> $null
    }
    Start-Sleep -Milliseconds 500

    Write-Host ">> KKYUM locator (kernel VA walk, no physical scan)"
    $loc = (& $layers --locate --kkyum --pid $ProcessId 2>&1 | Out-String)
    Write-Host $loc
    $mCr3 = [regex]::Match($loc, 'CR3=(0x[0-9A-Fa-f]+)')
    $mEproc = [regex]::Match($loc, 'EPROCESS=(0x[0-9A-Fa-f]+)')
    if (-not $mCr3.Success) { throw "KKYUM locate failed" }
    $refCr3 = $mCr3.Groups[1].Value
    $eprocVa = if ($mEproc.Success) { $mEproc.Groups[1].Value } else { '' }
    Write-Host "reference CR3=$refCr3 EPROCESS_VA=$eprocVa"

    Write-Host ">> translate EPROCESS VA -> PA (small page-table walk only)"
    $t = (& $layers --translate-only --reader legacy --legacy-backend tdeio --cr3 $refCr3 --va $eprocVa 2>&1 | Out-String)
    Write-Host $t
    $mPa = [regex]::Match($t, 'PA=(0x[0-9A-Fa-f]+)')
    if (-not $mPa.Success) { throw "translate EPROCESS failed" }
    $eprocPa = [Convert]::ToUInt64($mPa.Groups[1].Value, 16)
    $cr3Pa = $eprocPa + 0x28
    Write-Host ("EPROCESS_PA=0x{0:X} CR3_FIELD_PA=0x{1:X}" -f $eprocPa, $cr3Pa)

    $rows = @()
    foreach ($bid in $backends) {
        Write-Host ">> $bid (one 8-byte physical read)"
        $out = (& $membridge --backend $bid --pa ('0x{0:X}' -f $cr3Pa) --size 8 --dump 2>&1 | Out-String)
        $hex = [regex]::Matches($out, '\b([0-9A-Fa-f]{2})\b') | ForEach-Object { $_.Groups[1].Value }
        $verdict = 'FAIL'
        $got = ''
        if ($hex.Count -ge 8) {
            $bytes = @($hex | Select-Object -Last 8 | ForEach-Object { [Convert]::ToByte($_,16) })
            $val = [BitConverter]::ToUInt64([byte[]]$bytes, 0)
            $got = ('0x{0:X}' -f $val)
            $want = [Convert]::ToUInt64($refCr3, 16)
            if ($val -eq $want) { $verdict = 'PASS' }
        }
        $rows += [pscustomobject]@{ backend=$bid; cr3=$got; reference=$refCr3; verdict=$verdict }
        Write-Host ("   => {0} {1}" -f $verdict, $got)
    }

    $pass = @($rows | Where-Object {$_.verdict -eq 'PASS'}).Count
    $dumpsAfter = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    $newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
    $report = [pscustomobject]@{
        date=(Get-Date).ToString('s')
        target_pid=$ProcessId
        reference_cr3=$refCr3
        eprocess_va=$eprocVa
        eprocess_pa=('0x{0:X}' -f $eprocPa)
        total=$rows.Count
        pass=$pass
        only_backend=$OnlyBackend
        new_dumps=($newDumps -join ',')
        results=$rows
    }
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outJson -Encoding UTF8
    $rows | Format-Table -AutoSize
    Write-Host ("RESULT: {0}/{1} drivers read CR3 independently (one 8-byte read each) -> {2}" -f $pass,$rows.Count,$outJson)
} finally {
    Stop-Kkyum
    if ($startedTarget -and -not $KeepTarget) {
        Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    }
}
