# Diagnostic: can each physical driver read the target CR3 page?
# This does NOT scan memory and does NOT modify anything.
param(
    [int]$ProcessId = 0,
    [string]$TargetName = 'cmd.exe'
)

$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$layers = Join-Path $base 'bin\layers.exe'
$kkyum = Join-Path $base 'drivers\KKYUM\KKYUM.sys'
$outJson = Join-Path $base 'evidence\cr3_page_probe.json'
$svc = 'MembridgeCr3PageProbe'

$backends = @('cchwapi_2_9','cchwapi_2_12','cchwapi_ext_4_5','hp_swtools','swtoolsdriver',
              'hp_swtools_v1','isharepp','gibepext','gibepext2','tdeio','ufu_win10')

if ($ProcessId -le 0) {
    $proc = Get-Process -Name ($TargetName -replace '\.exe$','') -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) { throw "target not running: $TargetName" }
    $ProcessId = $proc.Id
}
$created = $false
try {
    & sc.exe create $svc binPath= $kkyum type= kernel start= demand *> $null
    if ($LASTEXITCODE -eq 0) { $created = $true }
    & sc.exe start $svc *> $null
    Start-Sleep -Milliseconds 400
    $loc = (& $layers --locate --kkyum --pid $ProcessId 2>&1 | Out-String)
    Write-Host $loc
    $m = [regex]::Match($loc, 'CR3=(0x[0-9A-Fa-f]+)')
    if (-not $m.Success) { throw 'KKYUM locate failed' }
    $cr3 = $m.Groups[1].Value
    $cr3Page = '0x{0:X}' -f ([Convert]::ToUInt64($cr3,16) -band 0xFFFFFFFFF000)
    Write-Host "target PID=$ProcessId CR3=$cr3 page=$cr3Page"

    $rows = @()
    foreach ($bid in $backends) {
        $out = (& $membridge --backend $bid --pa $cr3Page --size 8 --dump 2>&1 | Out-String)
        $verdict = [regex]::Match($out, 'VERDICT=(\w+)').Groups[1].Value
        $err = [regex]::Match($out, '\[ERR\].*').Value.Trim()
        $rows += [pscustomobject]@{ backend=$bid; verdict=$verdict; error=$err }
        Write-Host ("{0,-18} {1,-12} {2}" -f $bid,$verdict,$err)
    }
    $report = [pscustomobject]@{
        date=(Get-Date).ToString('s')
        target_pid=$ProcessId
        cr3=$cr3
        cr3_page=$cr3Page
        results=$rows
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outJson -Encoding UTF8
    Write-Host "saved $outJson"
} finally {
    if ($created) { & sc.exe stop $svc *> $null; & sc.exe delete $svc *> $null }
}
