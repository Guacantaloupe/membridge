$ErrorActionPreference = 'Continue'
$base = Split-Path -Parent $PSScriptRoot          # ...\membridge
$log = Join-Path $base 'evidence\_elev_membridge_cr3_selftest.log'
$json = Join-Path $base 'evidence\membridge_cr3_selftest.json'
$exe = Join-Path $base 'bin\cr3read.exe'
$kkyum = Join-Path $base 'drivers\KKYUM\KKYUM.sys'
$eneio = Join-Path $base 'drivers\eneio64\eneio64.sys'
'=== membridge CR3 arbitrary-exe selftest ===' | Out-File $log -Encoding UTF8
$target = $null
try {
    $target = Start-Process -FilePath 'C:\Windows\System32\cmd.exe' -ArgumentList '/c timeout /t 120 /nobreak > nul' -PassThru
    Start-Sleep -Seconds 2
    "target cmd pid=$($target.Id)" | Out-File $log -Append -Encoding UTF8
    $out = & $exe --pid $target.Id --module cmd.exe --size 0x40 --dump --compare --kkyum-driver "$kkyum" --phys-driver "$eneio" --phys-mode eneio64 2>&1 | Out-String
    $out | Out-File $log -Append -Encoding UTF8
    "EXITCODE=$LASTEXITCODE" | Out-File $log -Append -Encoding UTF8

    $cr3 = ''
    $m = [regex]::Match($out, '\[OK\] CR3 = (0x[0-9A-Fa-f]+) \(EPROCESS\+0x([0-9A-Fa-f]+)\)')
    if ($m.Success) { $cr3 = $m.Groups[1].Value }
    $compare = if ($out -match 'COMPARE=MATCH') { 'MATCH' } elseif ($out -match 'COMPARE=MISMATCH') { 'MISMATCH' } else { 'UNAVAILABLE' }
    $verdict = if ($out -match 'VERDICT=PASS') { 'PASS' } else { 'FAIL' }
    $result = [pscustomobject]@{
        action = 'cr3-selftest'
        target_module = 'cmd.exe'
        target_pid = $target.Id
        cr3 = $cr3
        cr3_resolution = 'ntoskrnl!PsInitialSystemProcess -> EPROCESS list -> DirectoryTableBase (+0x28)'
        page_table_walk = 'x64 4-level (PML4/PDPT/PD/PT, 1G/2M large pages supported)'
        physical_reader = 'drivers/eneio64/eneio64.sys (IOCTL_WINIO_MAPPHYSTOLIN)'
        kernel_reader = 'drivers/KKYUM/KKYUM.sys (MmCopyVirtualMemory)'
        compare = $compare
        verdict = $verdict
        timestamp = (Get-Date).ToString('yyyy-MM-ddTHH:mm:ss')
    }
    $result | ConvertTo-Json -Depth 4 | Set-Content $json -Encoding UTF8
    "=== json ===" | Out-File $log -Append -Encoding UTF8
    Get-Content $json -Raw -Encoding UTF8 | Out-File $log -Append -Encoding UTF8
    '=== done ===' | Out-File $log -Append -Encoding UTF8
    if ($verdict -eq 'PASS') { exit 0 } else { exit 1 }
}
finally {
    if ($target) { Get-Process -Id $target.Id -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue }
}
