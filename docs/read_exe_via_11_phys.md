# 让 11 个物理读驱动读取任意 EXE 内存

## 关键思路
之前失败的是 **EPROCESS 内核 VA → PA** 翻译（KVA Shadow 下用户 CR3 不映射完整内核）。

这个方案完全不翻译内核 VA，而是：

```
1) 用户态拿到目标 EXE 的 PEB/ImageBase（或用户指定任意 VA）
2) KKYUM 定位目标进程 CR3（内核 VA walk，只读，不扫描）
3) 用目标进程自己的 CR3 翻译目标 USER VA -> PA
   （用户地址空间一定在进程 CR3 中）
4) 11 个物理读驱动各自读该 PA 的页面
5) 与用户态 ReadProcessMemory 的基线字节对比
```

每个驱动只读一页（默认 0x40 字节），不扫描物理内存。

## 脚本
[scripts/read_exe_via_11_phys.ps1](read_exe_via_11_phys.ps1)

### 静态检查
```powershell
pwsh -File scripts\read_exe_via_11_phys.ps1 -StaticOnly
```

### 只测一个驱动
```powershell
# 管理员；默认目标 cmd.exe，自动取 ImageBase
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\read_exe_via_11_phys.ps1 -OnlyBackend gibepext
```

### 全部 11 个驱动
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\read_exe_via_11_phys.ps1
```

### 读取任意 EXE / 任意用户 VA
```powershell
# 先启动目标 exe，然后用 -ProcessId 或 -ProcessName
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\read_exe_via_11_phys.ps1 -ProcessName notepad -OnlyBackend gibepext

# 指定任意用户 VA
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\read_exe_via_11_phys.ps1 -ProcessId 1234 -Va 0x7FF600000000 -Size 0x40
```

- 同用户进程：脚本用 `OpenProcess + NtQueryInformationProcess + ReadProcessMemory` 获取 ImageBase 和基线；
- 不同用户/受保护进程：需要显式 `-Va`，并可用 `-SkipCompare` 只验证能读长度；
- 每次读取会被限制在单页内，避免跨页物理不连续。

## 输出
```
evidence\read_exe_via_11_phys.json
```
字段：`target_va`、`target_cr3`、`target_pa`、`translate_reader`、每个 backend 的 `bytes`/`verdict`、`new_dumps`。

## 风险
- 这些物理读驱动本身有 MDL/页表记账 bug（例如 0x4E double-unlock-MDL），**即使只读一次也可能在进程退出时崩溃**；
- 当前工作机已多次 BSOD，建议在一次性 VM/快照里运行；
- 11 个驱动逐个别测，崩溃则记录并排除。
