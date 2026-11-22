# MembridgePhys — HVCI-compatible physical memory provider (source)

Minimal WDM driver that exposes `\\.\MembridgePhys` with:

| IOCTL | Direction | Request |
|---|---|---|
| `0x80002000` | read | `{u64 phys; u32 size; u32 reserved}` → `size` bytes output |
| `0x80002004` | write | `{u64 phys; u32 size; u32 reserved; u8 data[size]}` |
| `0x80002008` | query | output `u32 version` |

It is written to be **HVCI-compatible**: no writable+executable sections, no
dynamic code, no self-modifying code, only `MmMapIoSpace` + `RtlCopyMemory`.

## Important: HVCI signing requirement

With Hypervisor-Enforced Code Integrity (Memory Integrity) enabled, a driver
must be **Microsoft-signed** (WHQL or attestation) to load. Test-signing and
unsigned builds do **not** load while HVCI is on.
Steps:

1. Build `MembridgePhys.sys` with the Windows Driver Kit (WDK) for Windows 11
   24H2/25H2 (KMDF not required; WDM).
2. Submit the `.sys`/`.inf` to Microsoft Partner Center for **attestation
   signing** (or WHQL), then install the signed package.
3. Install as a kernel service (admin):
   ```
   sc.exe create MembridgePhys type= kernel start= demand binPath= C:\path\MembridgePhys.sys
   sc.exe start MembridgePhys
   layers.exe --reader membridgephys --read-phys --pa 0x10000000 --size 16
   ```

Without Microsoft signing, use one of the already-signed third-party
providers while HVCI is enabled (see `docs/hvci.md` / `--hvci`).

## Build (when WDK is installed)

```powershell
# From an elevated "x64 Native Tools Command Prompt for VS 2022" with WDK:
cl /kernel /GS- /Gw /Gy /Oi /Os /D_AMD64_ /DAMD64 /c membridgephys.c
link /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /OUT:MembridgePhys.sys membridgephys.obj ntoskrnl.lib
```

`build.ps1` attempts to locate the WDK/MSVC and prints the exact commands if
the WDK kernel headers are not installed.

## HVCI audit

Run the repository scanner to see section/signature hazards:

```powershell
pwsh -File ..\..\scripts\hvci_check.ps1
```
