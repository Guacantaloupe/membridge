# Driver samples for membridge providers

These files are **test/research samples** of vendor-signed drivers used by the
`layers.exe` providers. They are kept here so a properly authorized test host
can load/verify the provider implementations.

> **HVCI / signing**: with Memory Integrity (HVCI) enabled, a driver must be
> Microsoft-signed (WHQL/attestation) **and** HVCI-compatible (no W^X sections,
> no dynamic code), and must not be on the vulnerable-driver blocklist.
> Many samples here are known BYOVD families (WinRing0, NTIOLib, atillk64,
> ASMMAP64, …). Check `scripts/hvci_check.ps1` and `docs/hvci.md` first.
> These files are not to be loaded on production machines.

| provider | driver file | device | notes |
|---|---|---|---|
| `cchwapi` | `drivers/CCHWApi_sys/CCHWApi.sys` | `\\.\CCHWAPI` | existing sample |
| `rtkio` | `drivers/rtkio/rtkio.sys` | `\\.\rtkio` | existing |
| `eneio64` | `drivers/eneio64/eneio64.sys` | `\\.\GLCKIo` | existing |
| `asrdrv` | `drivers/asrdrv/AsrDrv2.sys` | `\\.\AsrDrv109` | CVE-2020-15368 family; AES command protocol |
| `mrt` | `drivers/mrt/fekern_mrt.sys` | `\\.\fekern_00` | FireEye/MRT; SYSTEM-only |
| `cpuz` | `drivers/cpuz/cpuz1.sys` | `\\.\Global\CPUZ152` | physical dword R/W, page guard |
| `asupio64` | `drivers/asupio64/AsUpIO64.sys` | `\\.\AsUpdateio` | `PEUPDATER.EXE` gate; legacy 0x18 fallback |
| `asio2` | `drivers/asio2/AsIO2.sys` | `\\.\Asusgio2` | ASUSCERT + PE timestamp gate (bypass PE in bin) |
| `aswarpot` | `drivers/aswarpot/aswArPot.sys` | `\\.\aswSP_ArPot2` | arbitrary physical read/write |
| `winio64` | `drivers/winio64_1/WinIo64_1.sys` | `\\.\WinIo` | physical map read/write |
| `dumpit` | `drivers/dumpit/DumpIt.sys` | volume filter | physical read IOCTL 0x22C01E |
| `asmmap` | `drivers/asmmap64/asmmap64.sys` | `\\.\ASMMAP64` | physical map read/write |
| `atillk64` | `drivers/atillk64/atillk64.sys` | `\\.\atillk64` | physical read/write |
| `asio64` | `drivers/asio64/AsIO64.sys` | `\\.\Asusgio` | physical map read/write |
| `adv64drv` | `drivers/adv64drv/ADV64DRV.sys` | `\\.\ADV64DRV` | physical read |
| `ntiolib` | `drivers/ntiolib/NTIOLib.sys` | `\\.\NTIOLib_1_0_C` | physical read/write |
| `winring0` | `drivers/winring0/WinRing0.sys` | `\\.\WinRing0_1_0_1` | physical read/write |
| `amifldrv` | `drivers/amifldrv64/amifldrv64.sys` | `\\.\genericdrv` | byte read/write |
| `citmdrv` | `drivers/citmdrv/CITMDRV.sys` | `\\.\CITMDRV` | physical map to caller VA |
| `kprl` (locator) | `drivers/kprl/l1malwarebits.sys` | `\\.\KPRL` | CR3-by-PID IOCTL 0x239584 |
| `invarysensor` (locator) | `drivers/invarysensor/invarysensor.sys` | `\\.\invarysensor` | kernel-VA read IOCTL 0x9C402408 |
| `membridgephys` | `drivers/membridgephys/` | `\\.\MembridgePhys` | **source only**, HVCI-compatible design; must be Microsoft-signed |

## Driver submission

All provider drivers listed above are committed under `drivers/` in this
GitHub repository, together with the HVCI audit script and report. The custom
`membridgephys` driver source/INF/build script is included for signing and
future HVCI-enabled use.

Admin install helper (HVCI-aware error hints):

```
pwsh -File scripts\install_driver.ps1 -DriverPath .\drivers\aswarpot\aswArPot.sys -ServiceName aswArPot -DevicePath "\\.\aswSP_ArPot2" -DryRun
```
