# 安全的 EXE 内存读取方案（不再做高风险物理扫描）

物理扫描需要把旧版 vulnerable driver 当作“任意物理读”使用；CCHWApi 2.9 已证实会在 `rep movs` 中 BSOD 0x3B，HP map 路径也出现 0x1A MEMORY_MANAGEMENT。  
**不要在真机上继续用这些驱动做全内存扫描。** 下面按需求选择安全路径。

## 方案 A：同用户 EXE 内存读取（零驱动，最安全）
使用 Windows 文档化 API：
```
OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)
  -> NtQueryInformationProcess(ProcessBasicInformation) -> PEB
  -> ReadProcessMemory(PEB + 0x10) -> ImageBaseAddress
  -> ReadProcessMemory(ImageBase) -> MZ/PE 字节
```
脚本：[scripts/safe_read_exe.ps1](safe_read_exe.ps1)

```powershell
# 先打开一个 cmd.exe
pwsh -File scripts\safe_read_exe.ps1 -ProcessName cmd -Size 64

# 或读取指定 VA
pwsh -File scripts\safe_read_exe.ps1 -ProcessId <pid> -Va 0x7ff123450000 -Size 64
```

实测（同用户 cmd.exe）：
```
PID=10960 PEB=0x4777BEF000
VA=0x7FF69FD30000 bytes=32
HEX=4D 5A 90 00 03 00 00 00 ...
MZ_HEADER=PASS
```
- 不加载任何内核驱动；
- 不做 CR3/物理扫描；
- 不会触发驱动 bug/BSOD。

限制：目标进程必须与当前用户/完整性级别兼容；SYSTEM/其他用户/受保护进程会 `OpenProcess` 失败。

## 方案 B：跨权限/受保护进程，使用 KKYUM 直接进程读
`KKYUM.sys`：
- WHQL 签名（Microsoft Hardware Compatibility Publisher）；
- 静态审计 HVCI PASS；
- 已验证可在 HVCI 开启时加载，并读取目标进程 AWE 内存；
- IOCTL `0x22265C`：PID + 目标 VA -> 直接拷贝到调用者缓冲区；
- **不需要 CR3、不需要页表遍历、不需要物理扫描**。

加载 KKYUM 后：
```powershell
layers.exe --read-process --pid <target_pid> --va <target_va> --size 0x40
```
该路径已实现（`--read-process`），不会使用 CCHWApi/HP 等 map 驱动。

限制：需要先知道目标 VA（PEB/模块基址需要额外的进程查询；受保护进程可能需要更高权限）。

## 方案 C：需要 CR3
优先用专门的 locator，而不是物理扫描：
```powershell
layers.exe --locate --kkyum --pid <pid>     # KKYUM 内核 VA 遍历
layers.exe --locate --invary --pid <pid>    # Invary Sensor
layers.exe --locate --kprl --pid <pid>      # KPRL 0x239584 (HVCI 可能拒绝其签名)
```
这些路径是“读内核 VA/进程链表”或“IOCTL 直接返回 CR3”，不扫描物理 RAM。

**物理扫描仅建议在一次性 VM/快照中做**，并只使用已验证的 backend：
- CCHWApi 家族：已确认 BSOD，永久排除；
- HP map / 其他 map+ReadProcessMemory 驱动：高风险；
- 每次只测一个 backend + 一个物理范围；
- 保留 `CrashDumpEnabled=3` 以便 WinDbg 分析。

## 结论
- **要读 cmd.exe 内存：优先用方案 A（同用户）或方案 B（KKYUM）**；
- **要 CR3：优先 KKYUM/Invary 内核 VA locator**；
- 不再用旧版物理读驱动做全内存扫描；
- 真机崩溃问题可以从根本上避免，因为不再加载map 驱动。
