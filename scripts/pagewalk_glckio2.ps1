param(
    [Parameter(Mandatory=$true)][string]$Cr3,
    [Parameter(Mandatory=$true)][string]$Va,
    [int]$Size = 0x40,
    [string]$Device = '\\.\GLCKIo2'
)
$ErrorActionPreference = 'Stop'
$base = Split-Path $PSScriptRoot -Parent
$membridge = Join-Path $base 'bin\membridge.exe'

function Read-PA([uint64]$pa, [int]$size) {
    $o = & $membridge --device $Device --layout glckio2 --pa ('0x{0:X}' -f $pa) --size $size --dump 2>&1 | Out-String
    $m = [regex]::Match($o, '0000:\s*((?:[0-9A-F]{2}\s+)+)')
    if (-not $m.Success) { throw "read fail pa=0x{0:X}: {1}" -f $pa, $o }
    $hex = $m.Groups[1].Value -replace '\s',''
    $bytes = New-Object byte[] ($hex.Length/2)
    for ($i=0; $i -lt $bytes.Length; $i++) { $bytes[$i] = [Convert]::ToByte($hex.Substring($i*2,2),16) }
    return ,$bytes
}

$cr3v = [Convert]::ToUInt64($Cr3,16)
$vav  = [Convert]::ToUInt64($Va,16)
$idx4 = ($vav -shr 39) -band 0x1FF
$idx3 = ($vav -shr 30) -band 0x1FF
$idx2 = ($vav -shr 21) -band 0x1FF
$idx1 = ($vav -shr 12) -band 0x1FF

$pml4e = [BitConverter]::ToUInt64((Read-PA ($cr3v + $idx4*8) 8),0)
$pdpte = [BitConverter]::ToUInt64((Read-PA (($pml4e -band 0x000FFFFFFFFFF000) + $idx3*8) 8),0)
$pde   = [BitConverter]::ToUInt64((Read-PA (($pdpte -band 0x000FFFFFFFFFF000) + $idx2*8) 8),0)
if ($pde -band 0x80) {
    $pa = ($pde -band 0x000FFFFFFFE00000) + ($vav -band 0x1FFFFF)
    $page = '2MB'
} else {
    $pte = [BitConverter]::ToUInt64((Read-PA (($pde -band 0x000FFFFFFFFFF000) + $idx1*8) 8),0)
    $pa = ($pte -band 0x000FFFFFFFFFF000) + ($vav -band 0xFFF)
    $page = '4KB'
}
$data = Read-PA $pa $Size
[pscustomobject]@{
    cr3 = ('0x{0:X}' -f $cr3v)
    va = ('0x{0:X}' -f $vav)
    pml4e = ('0x{0:X}' -f $pml4e)
    pdpte = ('0x{0:X}' -f $pdpte)
    pde = ('0x{0:X}' -f $pde)
    page = $page
    pa = ('0x{0:X}' -f $pa)
    hex = (($data | ForEach-Object { $_.ToString('X2') }) -join '')
} | Format-List
