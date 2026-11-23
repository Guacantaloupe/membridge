param(
    [string]$Out = 'evidence\driver_loadtest.json'
)
$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
Set-Location $base
$dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$files = @()
$files += Get-ChildItem (Join-Path $base 'drivers\physmem_drivers') -File -Filter '*.sys' -ErrorAction SilentlyContinue
$files += Get-ChildItem (Join-Path $base 'drivers\corpus_samples') -File -Filter '*.sys' -ErrorAction SilentlyContinue
$rows = @()
$i = 0
foreach ($f in $files) {
    $i++
    $svc = "MbLoad_$($i)"
    & sc.exe stop $svc *> $null
    & sc.exe delete $svc *> $null
    & sc.exe create $svc binPath= $f.FullName type= kernel start= demand *> $null
    $create = $LASTEXITCODE
    & sc.exe start $svc *> $null
    $start = $LASTEXITCODE
    Start-Sleep -Milliseconds 250
    $q = (& sc.exe query $svc 2>&1 | Out-String)
    $state = ''
    $m = [regex]::Match($q, 'STATE\s*:\s*\d+\s+(\w+)')
    if ($m.Success) { $state = $m.Groups[1].Value }
    $exit = ''
    $me = [regex]::Match($q, 'WIN32_EXIT_CODE\s*:\s*(\d+)')
    if ($me.Success) { $exit = $me.Groups[1].Value }
    & sc.exe stop $svc *> $null
    & sc.exe delete $svc *> $null
    $row = [pscustomobject]@{
        name = $f.Name
        path = $f.FullName
        create = $create
        start = $start
        state = $state
        win32_exit = $exit
    }
    $rows += $row
    Write-Host ("{0}: state={1} start={2} exit={3}" -f $f.Name, $state, $start, $exit)
    $report = [pscustomobject]@{
        date = (Get-Date).ToString('s')
        total = $rows.Count
        running = @($rows | Where-Object { $_.state -eq 'RUNNING' }).Count
        new_dumps = (@(Compare-Object $dumpsBefore @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name) | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject }) -join ',')
        results = $rows
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $base $Out) -Encoding UTF8
    $newDumps = @(Compare-Object $dumpsBefore @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name) | Where-Object { $_.SideIndicator -eq '=>' })
    if ($newDumps.Count) { Write-Host '[CRASH] new dump'; break }
}
Write-Host ("RESULT: {0}/{1} drivers started RUNNING" -f @($rows | Where-Object { $_.state -eq 'RUNNING' }).Count, $rows.Count)
