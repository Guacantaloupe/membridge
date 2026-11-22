# 不使用 KPRL / KKYUM 的内存读取

## 结论
同用户进程（cmd.exe）内存读取可以只用 Windows 文档化 API 完成：

```
OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)
  -> NtQueryInformationProcess(ProcessBasicInformation) -> PEB
  -> ReadProcessMemory(PEB + 0x10) -> ImageBaseAddress
  -> ReadProcessMemory(ImageBase) -> MZ/PE 头
```

- **不需要 KPRL**
- **不需要 KKYUM**
- **不需要内核驱动**
- **不需要 CR3/页表**

## 实测
目标 cmd.exe PID 11392：
```
PID=11392 PEB=0x1154C0000
VA=0x7FF7746F0000 bytes=64
HEX=4D 5A 90 00 03 00 00 00 04 00 00 00 FF FF 00 00
    B8 00 00 00 00 00 00 00 40 00 00 00 ...
MZ_HEADER=PASS
```

脚本：
```
scripts/safe_read_exe.ps1
```

命令：
```powershell
pwsh -File scripts\safe_read_exe.ps1 -ProcessId 11392 -Size 0x40
```

## 限制
- 目标进程必须与当前用户/完整性级别兼容；
- SYSTEM、其他用户、受保护进程会 `OpenProcess` 失败；
- 跨权限场景仍需要内核驱动或其他机制。
