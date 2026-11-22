param(
    [string]$Cr3Page = '0x1AE000',
    [int]$Size = 0x30
)
$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$outJson = Join-Path $base 'evidence\hvci_11_cr3page.json'

$backends = @('cchwapi_2_9','cchwapi_2_12','cchwapi_ext_4_5','hp_swtools','swtoolsdriver','hp_swtools_v1','isharepp','gibepext','gibepext2','tdeio','ufu_win10')
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
$dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$rows = @()
$i = 0
foreach ($bid in $backends) {
    $i++
    $svc = "MbHvci_$($bid -replace '[^A-Za-z0-9]','_')"
    $path = Join-Path $base $driverPaths[$bid]
    Write-Host ">> [$i/$($backends.Count)] $bid"
    & sc.exe stop $svc *> $null
    & sc.exe delete $svc *> $null
    Start-Sleep -Milliseconds 100
    & sc.exe create $svc binPath= $path type= kernel start= demand *> $null
    & sc.exe start $svc *> $null
    Start-Sleep -Milliseconds 300
    $out = (& $membridge --backend $bid --no-load --pa $Cr3Page --size $Size --dump 2>&1 | Out-String)
    & sc.exe stop $svc *> $null
    & sc.exe delete $svc *> $null
    $verdict = if ($out -match 'VERDICT=PASS') { 'PASS' } else { 'FAIL' }
    $err = ([regex]::Match($out, '\[ERR\][^\r\n]*')).Value
    $row = [pscustomobject]@{
        backend = $bid
        cr3_page = $Cr3Page
        size = ('0x{0:X}' -f $Size)
        verdict = $verdict
        error = $err
        tail = (($out -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -Last 4) -join ' | ')
    }
    $rows += $row
    Write-Host ("   => {0} {1}" -f $verdict, $err)
    $dumpsAfter = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    $newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
    $report = [pscustomobject]@{
        date = (Get-Date).ToString('s')
        hvci = $true
        cr3_page = $Cr3Page
        total = $rows.Count
        pass = @($rows | Where-Object { $_.verdict -eq 'PASS' }).Count
        new_dumps = ($newDumps -join ',')
        results = $rows
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
    if ($newDumps.Count) { Write-Host "[CRASH] new dump: $($newDumps -join ',')"; break }
}
$rows | Format-Table -AutoSize
Write-Host ("RESULT={0}/{1} PASS; evidence={2}" -f @($rows | Where-Object { $_.verdict -eq 'PASS' }).Count, $rows.Count, $outJson)
