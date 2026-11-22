param(
    [int]$ProcessId = 0,
    [string]$ProcessName = 'cmd',
    [switch]$KeepDrivers
)
$ErrorActionPreference = 'Continue'
$base = Split-Path $PSScriptRoot -Parent
$layers = Join-Path $base 'bin\layers.exe'
$kkyum = Join-Path $base 'drivers\KKYUM\KKYUM.sys'
$kprl = Join-Path $base 'drivers\kprl\l1malwarebits.sys'
$outJson = Join-Path $base 'evidence\stable_cr3.json'

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host '[BLOCKED] Administrator required.'; exit 2
}

# --- NT base via EnumDeviceDrivers (P/Invoke; works under this token) ---
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class MbNtBase {
    [DllImport("psapi.dll", SetLastError=true)]
    public static extern bool EnumDeviceDrivers(IntPtr[] drivers, uint cb, out uint needed);
    [DllImport("psapi.dll", SetLastError=true)]
    public static extern bool GetDeviceDriverBaseNameA(IntPtr drv, StringBuilder name, int size);
}
'@
$need = 0
[void][MbNtBase]::EnumDeviceDrivers($null, 0, [ref]$need)
if ($need -le 0) { Write-Host '[ERR] EnumDeviceDrivers failed'; exit 3 }
$arr = New-Object IntPtr[] ([int]($need / 8))
$got = 0
[void][MbNtBase]::EnumDeviceDrivers($arr, $need, [ref]$got)
$ntBase = 0
for ($i = 0; $i -lt [int]($got / 8); $i++) {
    $sb = New-Object System.Text.StringBuilder 260
    [void][MbNtBase]::GetDeviceDriverBaseNameA($arr[$i], $sb, 260)
    if ($sb.ToString() -match '^ntoskrnl|^ntkrnlmp') { $ntBase = $arr[$i].ToInt64(); break }
}
if ($ntBase -eq 0) { Write-Host '[ERR] ntoskrnl base not found'; exit 3 }
$ntHex = '0x{0:X}' -f $ntBase

# --- target ---
if ($ProcessId -le 0) {
    $p = Get-Process -Name ($ProcessName -replace '\.exe$','') -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { Write-Host "[ERR] process $ProcessName not found"; exit 4 }
    $ProcessId = $p.Id
}
Write-Host "target PID=$ProcessId NT_BASE=$ntHex"

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
    if ($KeepDrivers) { return }
    foreach ($s in $script:services) {
        & sc.exe stop $s *> $null
        & sc.exe delete $s *> $null
    }
}

$results = @()
try {
    # 1) KKYUM: kernel-VA EPROCESS walk (needs explicit NT base)
    Load-Driver 'MbStableKkyum' $kkyum
    $o = & $layers --locate --kkyum --pid $ProcessId --nt-base $ntHex 2>&1 | Out-String
    $mCr3 = [regex]::Match($o, 'CR3=(0x[0-9A-Fa-f]+)')
    $mEp  = [regex]::Match($o, 'EPROCESS=(0x[0-9A-Fa-f]+)')
    $results += [pscustomobject]@{
        source='kkyum'; ok=$mCr3.Success
        cr3=if($mCr3.Success){$mCr3.Groups[1].Value}else{''}
        eprocess=if($mEp.Success){$mEp.Groups[1].Value}else{''}
        output=($o.Trim() -split "`r?`n" | Select-Object -Last 2) -join ' | '
    }

    # 2) KPRL: CR3 by PID IOCTL
    Load-Driver 'MbStableKprl' $kprl
    $o = & $layers --locate --kprl --pid $ProcessId 2>&1 | Out-String
    $mCr3 = [regex]::Match($o, 'CR3=(0x[0-9A-Fa-f]+)')
    $results += [pscustomobject]@{
        source='kprl'; ok=$mCr3.Success
        cr3=if($mCr3.Success){$mCr3.Groups[1].Value}else{''}
        eprocess=''
        output=($o.Trim() -split "`r?`n" | Select-Object -Last 2) -join ' | '
    }
} finally {
    Unload-Drivers
}

$results | Format-Table -AutoSize
$valid = @($results | Where-Object { $_.ok -and $_.cr3 })
$consensus = ''
if ($valid.Count -ge 2) {
    $distinct = @($valid | Select-Object -ExpandProperty cr3 -Unique)
    if ($distinct.Count -eq 1) { $consensus = 'PASS' } else { $consensus = 'CONFLICT' }
} elseif ($valid.Count -eq 1) {
    $consensus = 'PASS_WITH_WARNING'
} else {
    $consensus = 'FAIL'
}
$report = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    pid = $ProcessId
    nt_base = $ntHex
    consensus = $consensus
    cr3 = if ($valid.Count) { $valid[0].cr3 } else { '' }
    sources = $results
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
Write-Host ("CONSENSUS={0} CR3={1} evidence={2}" -f $consensus, $report.cr3, $outJson)
if ($consensus -eq 'FAIL') { exit 5 }
if ($consensus -eq 'CONFLICT') { exit 6 }
exit 0
