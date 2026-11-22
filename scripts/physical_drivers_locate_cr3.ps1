param(
    [int]$ProcessId = 0,
    [string]$ProcessName = '',
    [string[]]$Backends = @('gibepext','gibepext2','cchwapi_2_12','cchwapi_ext_4_5'),
    [switch]$IncludeAll11
)
$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$layers = Join-Path $base 'bin\layers.exe'
$outJson = Join-Path $base 'evidence\physical_cr3_locate.json'

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host '[BLOCKED] Administrator required to load the physical drivers.'; exit 2
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
if ($IncludeAll11) {
    $Backends = @('gibepext','gibepext2','cchwapi_2_12','cchwapi_ext_4_5',
                  'cchwapi_2_9','ufu_win10','hp_swtools','swtoolsdriver',
                  'hp_swtools_v1','isharepp','tdeio')
}

if ($ProcessId -le 0) {
    if (-not $ProcessName) { $ProcessName = 'cmd' }
    $p = Get-Process -Name ($ProcessName -replace '\.exe$','') -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { Write-Host "[ERR] process $ProcessName not found"; exit 4 }
    $ProcessId = $p.Id
    $ProcessName = $p.ProcessName + '.exe'
}
if (-not $ProcessName) {
    $p = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if (-not $p) { Write-Host "[ERR] pid $ProcessId not found"; exit 4 }
    $ProcessName = $p.ProcessName + '.exe'
}
if ($ProcessName.Length -gt 15) { $ProcessName = $ProcessName.Substring(0,15) }
Write-Host "target PID=$ProcessId ImageFileName=$ProcessName"

# Physical RAM ranges via layers --ranges
$rangesOut = (& $layers --ranges 2>&1 | Out-String)
$ranges = @()
foreach ($m in [regex]::Matches($rangesOut, '\[\s*\d+\]\s*(0x[0-9A-Fa-f]+)\s*-\s*(0x[0-9A-Fa-f]+)')) {
    $start = [Convert]::ToUInt64($m.Groups[1].Value,16)
    $end   = [Convert]::ToUInt64($m.Groups[2].Value,16)
    if ($end -gt $start) { $ranges += [pscustomobject]@{ start=$start; end=$end; len=($end-$start) } }
}
if ($ranges.Count -eq 0) { Write-Host '[ERR] no physical memory ranges'; exit 5 }
Write-Host ("ranges={0}" -f $ranges.Count)

$services = @()
function Load-Driver([string]$name,[string]$path) {
    & sc.exe stop $name *> $null
    & sc.exe delete $name *> $null
    Start-Sleep -Milliseconds 100
    & sc.exe create $name binPath= $path type= kernel start= demand *> $null
    & sc.exe start $name *> $null
    Start-Sleep -Milliseconds 300
    $script:services += $name
}
function Unload-Drivers {
    foreach ($s in $script:services) {
        & sc.exe stop $s *> $null
        & sc.exe delete $s *> $null
    }
}

$attempts = @()
$found = $null
$chunkSize = 0x20000000   # 512 MB per locate call
try {
    foreach ($bid in $Backends) {
        if (-not $driverPaths.ContainsKey($bid)) { Write-Host "[SKIP] unknown $bid"; continue }
        $path = Join-Path $base $driverPaths[$bid]
        if (-not (Test-Path -LiteralPath $path)) { Write-Host "[SKIP] missing $path"; continue }
        $svc = "MbPhysLocate_$($bid -replace '[^A-Za-z0-9]','_')"
        Write-Host ">> backend=$bid"
        Load-Driver $svc $path
        $backendOk = $false
        foreach ($r in $ranges) {
            $pos = $r.start
            while ($pos -lt $r.end -and -not $found) {
                $len = [Math]::Min($chunkSize, $r.end - $pos)
                if ($len -lt 0x1000) { break }
                Write-Host ("   scan PA=0x{0:X} len=0x{1:X}" -f $pos,$len)
                $o = (& $membridge --backend $bid --no-load --locate --pid $ProcessId --name $ProcessName `
                        --scan-start ('0x{0:X}' -f $pos) --scan-size ('0x{0:X}' -f $len) 2>&1 | Out-String)
                $mCr3 = [regex]::Match($o, 'CR3=(0x[0-9A-Fa-f]+)')
                $mEp  = [regex]::Match($o, 'EPROCESS_PA=(0x[0-9A-Fa-f]+)')
                $attempts += [pscustomobject]@{
                    backend=$bid; pa_start=('0x{0:X}' -f $pos); size=('0x{0:X}' -f $len)
                    cr3=if($mCr3.Success){$mCr3.Groups[1].Value}else{''}
                    eprocess_pa=if($mEp.Success){$mEp.Groups[1].Value}else{''}
                    ok=$mCr3.Success
                }
                if ($mCr3.Success) {
                    $found = [pscustomobject]@{
                        backend=$bid; cr3=$mCr3.Groups[1].Value
                        eprocess_pa=if($mEp.Success){$mEp.Groups[1].Value}else{''}
                    }
                    $backendOk = $true
                    break
                }
                $report = [pscustomobject]@{
                    date=(Get-Date).ToString('s'); pid=$ProcessId; name=$ProcessName
                    status=if($found){'FOUND'}else{'SCANNING'}
                    found=$found; attempts=$attempts
                }
                $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
                $pos += $len
            }
            if ($found) { break }
        }
        if ($backendOk) { break }
    }
} finally {
    Unload-Drivers
}

$report = [pscustomobject]@{
    date=(Get-Date).ToString('s'); pid=$ProcessId; name=$ProcessName
    status=if($found){'FOUND'}else{'FAIL'}
    found=$found; attempts=$attempts
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
if ($found) {
    Write-Host ("FOUND backend={0} CR3={1} EPROCESS_PA={2}" -f $found.backend,$found.cr3,$found.eprocess_pa)
    exit 0
}
Write-Host "NOT_FOUND after $($attempts.Count) range scans"
exit 5
