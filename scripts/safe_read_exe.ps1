# Safe EXE memory read using only documented user-mode APIs.
# No kernel driver, no physical scan, no CR3 walk -> cannot crash the kernel.
#
# For a same-user process this reads:
#   PEB (via NtQueryInformationProcess) -> ImageBaseAddress -> PE header bytes.
# If OpenProcess/ReadProcessMemory is denied (different user/SYSTEM/protected),
# use the KKYUM direct-process-read path instead.
#
# Usage:
#   pwsh -File scripts\safe_read_exe.ps1 -ProcessName cmd
#   pwsh -File scripts\safe_read_exe.ps1 -ProcessId 1234
#   pwsh -File scripts\safe_read_exe.ps1 -ProcessName cmd -Va 0x7ff123450000 -Size 64
param(
    [string]$ProcessName = 'cmd',
    [int]$ProcessId = 0,
    [string]$Va = '',
    [int]$Size = 64
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MbSafeRead {
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

if ($ProcessId -le 0) {
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) { throw "process not found: $ProcessName (start it first or pass -ProcessId)" }
    $ProcessId = $proc.Id
}
$PROCESS_QUERY_INFORMATION = 0x0400
$PROCESS_VM_READ = 0x0010
$h = [MbSafeRead]::OpenProcess($PROCESS_QUERY_INFORMATION -bor $PROCESS_VM_READ, $false, $ProcessId)
if ($h -eq [IntPtr]::Zero) {
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed err=$err (needs same-user or KKYUM path)"
}
try {
    $pbi = New-Object MbSafeRead+PROCESS_BASIC_INFORMATION
    $ret = 0
    $st = [MbSafeRead]::NtQueryInformationProcess($h, 0, [ref]$pbi, [Runtime.InteropServices.Marshal]::SizeOf($pbi), [ref]$ret)
    if ($st -ne 0) { throw ("NtQueryInformationProcess status=0x{0:X8}" -f $st) }
    $peb = $pbi.PebBaseAddress.ToInt64()
    Write-Host ("PID={0} PEB=0x{1:X}" -f $ProcessId, $peb)

    if ($Va) {
        $addr = [Convert]::ToInt64($Va, 16)
    } else {
        $buf = New-Object byte[] 8
        $got = [IntPtr]::Zero
        if (-not [MbSafeRead]::ReadProcessMemory($h, [IntPtr]($peb + 0x10), $buf, [IntPtr]$buf.Length, [ref]$got)) {
            throw "ReadProcessMemory(PEB.ImageBaseAddress) failed"
        }
        $addr = [BitConverter]::ToInt64($buf, 0)
    }
    $data = New-Object byte[] $Size
    $got = [IntPtr]::Zero
    if (-not [MbSafeRead]::ReadProcessMemory($h, [IntPtr]$addr, $data, [IntPtr]$data.Length, [ref]$got)) {
        throw "ReadProcessMemory(0x{0:X}) failed" -f $addr
    }
    Write-Host ("VA=0x{0:X} bytes={1}" -f $addr, $got.ToInt64())
    Write-Host ("HEX=" + (($data | ForEach-Object { $_.ToString('X2') }) -join ' '))
    if ($data.Length -ge 2 -and $data[0] -eq 0x4D -and $data[1] -eq 0x5A) {
        Write-Host "MZ_HEADER=PASS"
    }
} finally {
    [void][MbSafeRead]::CloseHandle($h)
}
