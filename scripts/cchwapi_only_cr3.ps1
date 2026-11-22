# CCHWApi 2.9-only CR3 acquisition (no physical scan).
#
# Uses only CCHWApi 2.9:
#   1) IOCTL 0x80240020 ReadControlReg(CPU0, CR3) -> current CR3
#   2) IOCTL 0x8024003C ReadMemory -> page-table translation + kernel reads
#   3) ntoskrnl PsInitialSystemProcess + EPROCESS walk -> cmd.exe CR3
#
# No full RAM scan, no other kernel driver.
param(
    [switch]$StaticOnly,
    [switch]$DryRun,
    [int]$ProcessId = 0,
    [string]$TargetName = 'cmd.exe',
    [switch]$KeepTarget
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$layers = Join-Path $base 'bin\layers.exe'
$driver = Join-Path $base 'drivers\CCHWApi_sys\CCHWApi.sys'
$outJson = Join-Path $base 'evidence\cchwapi_only_cr3.json'
$service = 'MembridgeCCHWApi29'

if (-not (Test-Path -LiteralPath $layers)) { throw "missing layers.exe" }
if (-not (Test-Path -LiteralPath $driver)) { throw "missing CCHWApi.sys" }

Write-Host "== CCHWApi 2.9 only CR3 =="
if ($StaticOnly) {
    $sig = (Get-AuthenticodeSignature -LiteralPath $driver).Status.ToString()
    $ls = (& $layers --selftest-mock 2>&1 | Out-String)
    $report = [pscustomobject]@{
        mode = 'static'
        driver = $driver
        signature = $sig
        locate_selftest = if ($ls -match 'VERDICT=PASS') {'PASS'} else {'FAIL'}
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outJson -Encoding UTF8
    Write-Host "CCHWApi signature=$sig selftest=$($report.locate_selftest)"
    if ($sig -eq 'Valid' -and $report.locate_selftest -eq 'PASS') { Write-Host "STATIC_ONLY=PASS"; exit 0 }
    exit 2
}

if ($DryRun) {
    Write-Host "1) sc create/start $service -> $driver"
    Write-Host "2) start cmd.exe if needed"
    Write-Host "3) layers --locate --cchwapi-only --pid <cmd_pid>"
    Write-Host "   (ReadControlReg CR3 + CCHWApi page-table walk + EPROCESS walk; no physical scan)"
    Write-Host "4) stop/delete service"
    Write-Host "DRY_RUN=PASS"; exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load CCHWApi 2.9 and run the locator." -ForegroundColor Yellow
    exit 2
}

$startedTarget = $false
$serviceCreated = $false
try {
    if ($ProcessId -le 0) {
        $procName = $TargetName -replace '\.exe$',''
        $proc = Get-Process -Name $procName -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $proc) {
            $cmdExe = Join-Path $env:SystemRoot 'System32\cmd.exe'
            $proc = Start-Process -FilePath $cmdExe -ArgumentList '/k','title CchwapiCr3Target' -PassThru
            $startedTarget = $true
            Start-Sleep -Milliseconds 600
        }
        $ProcessId = $proc.Id
    }
    Write-Host "target PID=$ProcessId"

    Write-Host ">> loading CCHWApi 2.9..."
    & sc.exe create $service binPath= $driver type= kernel start= demand *> $null
    if ($LASTEXITCODE -eq 0) { $serviceCreated = $true }
    & sc.exe start $service *> $null
    Start-Sleep -Milliseconds 500

    Write-Host ">> low-risk smoke test: control-register CR3 only (no page walk)"
    $cur = (& $layers --cchwapi-cr3-current 2>&1 | Out-String)
    Write-Host $cur
    if ($cur -notmatch 'VERDICT=PASS') {
        throw "CCHWApi control-register CR3 read failed; aborting before page-table/EPROCESS walk"
    }

    $out = (& $layers --locate --cchwapi-only --pid $ProcessId 2>&1 | Out-String)
    Write-Host $out
    $mCr3 = [regex]::Match($out, 'CR3=(0x[0-9A-Fa-f]+)')
    $mEproc = [regex]::Match($out, 'EPROCESS=(0x[0-9A-Fa-f]+)')
    $report = [pscustomobject]@{
        date = (Get-Date).ToString('s')
        driver = $driver
        target_pid = $ProcessId
        cr3 = if ($mCr3.Success) { $mCr3.Groups[1].Value } else { '' }
        eprocess = if ($mEproc.Success) { $mEproc.Groups[1].Value } else { '' }
        verdict = if ($mCr3.Success) { 'PASS' } else { 'FAIL' }
        output = $out.Trim()
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outJson -Encoding UTF8
    Write-Host ("RESULT: {0} CR3={1} -> {2}" -f $report.verdict, $report.cr3, $outJson)
} finally {
    if ($serviceCreated) {
        & sc.exe stop $service *> $null
        Start-Sleep -Milliseconds 300
        & sc.exe delete $service *> $null
    }
    if ($startedTarget -and -not $KeepTarget) {
        Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    }
}
