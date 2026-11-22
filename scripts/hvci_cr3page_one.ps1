param(
    [Parameter(Mandatory=$true)][string]$Backend,
    [string]$Cr3Page = '0x17B222000',
    [int]$Size = 0x30
)
$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$driverPaths = @{
    'cchwapi_2_9'     = 'drivers\CCHWApi_sys\CCHWApi.sys'
    'cchwapi_2_12'    = 'drivers\CCHWApi(1)_sys\CCHWApi(1).sys'
    'cchwapi_ext_4_5' = 'drivers\CCHWApiExt(1)_sys\CCHWApiExt(1).sys'
    'hp_swtools'      = 'drivers\HP SW TOOLS DRIVER_sys\HP SW TOOLS DRIVER.sys'
    'swtoolsdriver'   = 'drivers\swtoolsdriver\swtoolsdriver.sys'
    'hp_swtools_v1'   = 'drivers\hp_swtools_v1\HP_SWTOOLS_DRIVER_V1.sys'
    'gibepext'        = 'drivers\gibepext\gibepext.sys'
    'gibepext2'       = 'drivers\gibepext2\gibepext2.sys'
    'tdeio'           = 'drivers\tdeio\TdeIo64.sys'
    'ufu_win10'       = 'drivers\ufu\win10_ufudev64.sys'
}
if (-not $driverPaths.ContainsKey($Backend)) { throw "unknown backend $Backend" }
$outJson = Join-Path $base ("evidence\hvci_cr3page_{0}.json" -f $Backend)
$svc = "MbHvci_$($Backend -replace '[^A-Za-z0-9]','_')"
$path = Join-Path $base $driverPaths[$Backend]

& sc.exe stop $svc *> $null
& sc.exe delete $svc *> $null
Start-Sleep -Milliseconds 100
& sc.exe create $svc binPath= $path type= kernel start= demand *> $null
& sc.exe start $svc *> $null
Start-Sleep -Milliseconds 300
$out = (& $membridge --backend $Backend --no-load --pa $Cr3Page --size $Size --dump 2>&1 | Out-String)
& sc.exe stop $svc *> $null
& sc.exe delete $svc *> $null

$verdict = if ($out -match 'VERDICT=PASS') { 'PASS' } else { 'FAIL' }
$hex = ''
$m = [regex]::Match($out, '(?s)READ_OK[^\r\n]*\r?\n\s*0000:\s*((?:[0-9A-F]{2}\s)+)')
if ($m.Success) { $hex = ($m.Groups[1].Value -replace '\s+',' ').Trim() }
$report = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    backend = $Backend
    cr3_page = $Cr3Page
    size = ('0x{0:X}' -f $Size)
    verdict = $verdict
    hex = $hex
    error = ([regex]::Match($out, '\[ERR\][^\r\n]*')).Value
    tail = (($out -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -Last 6) -join ' | ')
    dump = ((Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1).Name)
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outJson -Encoding UTF8
$report | Format-List
Write-Host ("RESULT={0} backend={1} evidence={2}" -f $verdict,$Backend,$outJson)
