param([string]$Backends = '')
$ErrorActionPreference = 'Continue'
$base = Split-Path -Parent $PSScriptRoot   # ...\msdriver-0day
$env:MEMBRIDGE_BASE = $base
$log = Join-Path $base 'evidence\_elev_membridge_selftest.log'
'=== BYOV memread framework self-test ===' | Out-File $log -Encoding UTF8
$exe = Join-Path $base 'bin\membridge.exe'
$aweTarget = Join-Path $base 'test\test.exe'
$aweDir = Join-Path $env:ProgramData 'membridge'
New-Item -ItemType Directory -Force -Path $aweDir -ErrorAction SilentlyContinue | Out-Null
$aweLog = Join-Path $aweDir 'framework_awe_stdout.log'
$dumpDir = 'C:\Windows\Minidump'
$dumpsBefore = @(Get-ChildItem $dumpDir -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$backendIds = @('cchwapi_2_9','cchwapi_2_12','cchwapi_ext_4_5','sjtwinio','hp_swtools','swtoolsdriver','isharepp','rtkio','rtkiow8x64','eneio64','glckio2','hp_swtools_v1','winio64_elan','winio64_1','gibepext','fastdumpx64','directio','gibepext2','fastdumpx64_v2','tdeio','icachex')
if ($Backends) { $backendIds = $Backends -split ',' }

# ---- start AWE target as SYSTEM ----
if (Test-Path $aweLog) { Remove-Item $aweLog -Force -ErrorAction SilentlyContinue }
& schtasks.exe /end /tn MembridgeAwe *> $null
& schtasks.exe /delete /tn MembridgeAwe /f *> $null
$tr = 'cmd.exe /c ""' + $aweTarget + '" 600 > "' + $aweLog + '" 2>&1"'
& schtasks.exe /create /tn MembridgeAwe /tr "$tr" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f | Out-File $log -Append -Encoding UTF8
& schtasks.exe /run /tn MembridgeAwe | Out-File $log -Append -Encoding UTF8
$pagePa = [uint64]0
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    if (Test-Path $aweLog) {
        $t = Get-Content $aweLog -Raw -Encoding UTF8
        $m = [regex]::Match($t, 'PAGE\[00\] PFN=0x([0-9A-Fa-f]+) PA=0x([0-9A-Fa-f]+)')
        if ($m.Success) { $pagePa = [Convert]::ToUInt64($m.Groups[2].Value, 16); break }
    }
}
"awe page PA = 0x{0:X}" -f $pagePa | Out-File $log -Append -Encoding UTF8

$results = @()
if ($pagePa -ne 0) {
    foreach ($id in $backendIds) {
        $out = Join-Path $base ("evidence\selftest_" + $id + ".log")
        "`n--- backend: $id ---" | Out-File $log -Append -Encoding UTF8
        $argList = @('--backend', $id, '--pa', ('0x{0:X}' -f $pagePa), '--size', '0x100',
                     '--expect', 'BYOV_TESTEXE_AWE_PAGE_00', '--dump')
        if ($id -eq 'sjtwinio') {
            $alt = Join-Path $base 'drivers\SjtWinIo_sys\SjtWinIo_copy.sys'
            if (Test-Path $alt) { $argList += @('--driver-path', $alt) }
        }
        $proc = Start-Process -FilePath $exe -ArgumentList $argList -Wait -PassThru `
            -RedirectStandardOutput $out -RedirectStandardError ($out + '.err') -WindowStyle Hidden
        $txt = if (Test-Path $out) { Get-Content $out -Raw -Encoding UTF8 } else { '' }
        $txt | Out-File $log -Append -Encoding UTF8
        $verdict = 'UNKNOWN'
        $vm = [regex]::Match($txt, 'VERDICT=(\S+)')
        if ($vm.Success) { $verdict = $vm.Groups[1].Value }
        $results += [pscustomobject]@{ backend = $id; exit = $proc.ExitCode; verdict = $verdict; evidence = $out }
        "  => $verdict (exit=$($proc.ExitCode))" | Out-File $log -Append -Encoding UTF8
    }
}
# ---- cleanup ----
& schtasks.exe /end /tn MembridgeAwe *> $null
& schtasks.exe /delete /tn MembridgeAwe /f *> $null
Get-Process -Name test -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
$dumpsAfter = @(Get-ChildItem $dumpDir -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$newDumps = @(Compare-Object $dumpsBefore $dumpsAfter | Where-Object { $_.SideIndicator -eq '=>' } | ForEach-Object { $_.InputObject })
$summary = [pscustomobject]@{
    date = (Get-Date).ToString('s')
    page_pa = ('0x{0:X}' -f $pagePa)
    results = $results
    new_dumps = ($newDumps -join ',')
}
$summary | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $base 'evidence\membridge_selftest.json') -Encoding UTF8
"`n=== summary ===" | Out-File $log -Append -Encoding UTF8
$results | ForEach-Object { "$($_.backend) => $($_.verdict)" } | Out-File $log -Append -Encoding UTF8
"new dumps: $($newDumps -join ',')" | Out-File $log -Append -Encoding UTF8
'=== done ===' | Out-File $log -Append -Encoding UTF8





