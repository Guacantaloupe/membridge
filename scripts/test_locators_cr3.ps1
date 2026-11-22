# Compare CR3 locator drivers (no physical RAM scan).
#
# Tests these drivers one at a time:
#   KKYUM        WHQL, HVCI-PASS, kernel-VA + ActiveProcessLinks -> CR3
#   Invary       invarysensor.sys kernel-VA read + process list -> CR3
#   KPRL         l1malwarebits.sys IOCTL 0x239584 -> CR3 (vendor-signed; HVCI may reject)
#
# Each locator is started as a kernel service, used with layers --locate,
# then stopped/deleted. No physical scan is performed.
#
# Usage:
#   pwsh -File scripts\test_locators_cr3.ps1 -StaticOnly
#   pwsh -File scripts\test_locators_cr3.ps1 -DryRun
#   # elevated:
#   pwsh -File scripts\test_locators_cr3.ps1
#   pwsh -File scripts\test_locators_cr3.ps1 -Locator kkyum
param(
    [switch]$StaticOnly,
    [switch]$DryRun,
    [string]$Locator = '',
    [int]$ProcessId = 0,
    [string]$TargetName = 'cmd.exe',
    [switch]$KeepDriver
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$layers = Join-Path $base 'bin\layers.exe'
$outJson = Join-Path $base 'evidence\locators_cr3.json'

$locators = @(
    [pscustomobject]@{ name='kkyum';  driver='drivers\KKYUM\KKYUM.sys';                 arg='--locate --kkyum';  device='\\.\KKYUM' },
    [pscustomobject]@{ name='invary'; driver='drivers\invarysensor\invarysensor.sys';   arg='--locate --invary'; device='\\.\invarysensor' },
    [pscustomobject]@{ name='kprl';   driver='drivers\kprl\l1malwarebits.sys';          arg='--locate --kprl';   device='\\.\KPRL' }
)
if ($Locator) { $locators = @($locators | Where-Object { $_.name -eq $Locator }) }
if ($locators.Count -eq 0) { throw "unknown -Locator; use kkyum|invary|kprl" }

if (-not (Test-Path -LiteralPath $layers)) { throw "missing layers.exe" }
foreach ($l in $locators) {
    $p = Join-Path $base $l.driver
    if (-not (Test-Path -LiteralPath $p)) { throw "missing driver: $p" }
}

Write-Host "== CR3 locator drivers (no physical scan) =="
$locators | Format-Table name, driver, arg -AutoSize

if ($StaticOnly) {
    $rows = @()
    foreach ($l in $locators) {
        $p = Join-Path $base $l.driver
        $rows += [pscustomobject]@{
            locator = $l.name
            driver = $l.driver
            exists = (Test-Path -LiteralPath $p)
            signature = (Get-AuthenticodeSignature -LiteralPath $p).Status.ToString()
        }
    }
    $report = [pscustomobject]@{
        mode='static'
        layers_selftest = if ((& $layers --selftest-mock 2>&1 | Out-String) -match 'VERDICT=PASS') { 'PASS' } else { 'FAIL' }
        locators=$rows
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outJson -Encoding UTF8
    $rows | Format-Table -AutoSize
    if (@($rows | Where-Object { -not $_.exists -or $_.signature -ne 'Valid' }).Count -eq 0) {
        Write-Host "STATIC_ONLY=PASS"; exit 0
    }
    exit 2
}

if ($DryRun) {
    foreach ($l in $locators) {
        Write-Host ("1) sc create/start <svc> -> {0}" -f $l.driver)
        Write-Host ("2) layers {0} --pid <cmd_pid>" -f $l.arg)
        Write-Host ("3) sc stop/delete <svc>")
    }
    Write-Host "DRY_RUN=PASS"; exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load locator drivers." -ForegroundColor Yellow
    exit 2
}

$startedTarget = $false
if ($ProcessId -le 0) {
    $procName = $TargetName -replace '\.exe$',''
    $proc = Get-Process -Name $procName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) {
        $cmdExe = Join-Path $env:SystemRoot 'System32\cmd.exe'
        $proc = Start-Process -FilePath $cmdExe -ArgumentList '/k','title LocatorCr3Target' -PassThru
        $startedTarget = $true
        Start-Sleep -Milliseconds 600
    }
    $ProcessId = $proc.Id
}
Write-Host "target PID=$ProcessId"

$dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$rows = @()
foreach ($l in $locators) {
    $svc = 'MembridgeLoc_' + $l.name
    $created = $false
    try {
        Write-Host ">> $($l.name): loading $($l.driver)"
        $old = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $createOut = (& sc.exe create $svc binPath= (Join-Path $base $l.driver) type= kernel start= demand 2>&1 | Out-String)
        $createRc = $LASTEXITCODE
        if ($createRc -eq 0) { $created = $true }
        $startOut = (& sc.exe start $svc 2>&1 | Out-String)
        $startRc = $LASTEXITCODE
        $ErrorActionPreference = $old
        if ($createRc -ne 0) { Write-Host "[create rc=$createRc] $($createOut.Trim())" }
        if ($startRc -ne 0) { Write-Host "[start rc=$startRc] $($startOut.Trim())" }
        Start-Sleep -Milliseconds 500
        $locArgs = @($l.arg.Split(' ')) + @('--pid', $ProcessId)
        $out = (& $layers @locArgs 2>&1 | Out-String)
        Write-Host $out
        $m = [regex]::Match($out, 'CR3=(0x[0-9A-Fa-f]+)')
        $rows += [pscustomobject]@{
            locator = $l.name
            driver = $l.driver
            load_status = if ($createRc -eq 0 -and $startRc -eq 0) { 'STARTED' } elseif ($startRc -eq 0) { 'STARTED' } else { "CREATE=$createRc START=$startRc" }
            load_error = (($createOut + ' ' + $startOut).Trim())
            cr3 = if ($m.Success) { $m.Groups[1].Value } else { '' }
            verdict = if ($m.Success) { 'PASS' } else { 'FAIL' }
            output = $out.Trim()
        }
    } finally {
        if ($created -and -not $KeepDriver) {
            & sc.exe stop $svc *> $null
            Start-Sleep -Milliseconds 300
            & sc.exe delete $svc *> $null
        }
    }
}
$dumpsAfter = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
$pass = @($rows | Where-Object { $_.verdict -eq 'PASS' }).Count
$report = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    target_pid = $ProcessId
    total = $rows.Count
    pass = $pass
    results = $rows
    new_dumps = ($newDumps -join ',')
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outJson -Encoding UTF8
$rows | Select-Object locator, cr3, verdict | Format-Table -AutoSize
Write-Host ("RESULT: {0}/{1} locators got CR3 -> {2}" -f $pass,$rows.Count,$outJson)
if ($startedTarget -and -not $KeepDriver) { Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue }
