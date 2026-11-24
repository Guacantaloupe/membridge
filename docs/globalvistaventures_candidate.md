# GlobalVistaVentures_v3 新候选

## 基本信息
- 文件：`GlobalVistaVentures_v3.sys`
- MD5：`a3e41b5caaabf71f04d110b158fbf2e2`
- SHA256：`7ccb02b9dc8bb46e5ea32fb8f4c93bac195bcbb0c2fc02d3af28f4208afe7c41`
- 大小：32104
- 签名：**Microsoft Windows Hardware Compatibility Publisher（WHQL 有效）**
- 加载：
```
sc start -> 0 (RUNNING)
```
- 设备：
```
\Device\GlobalVistaVentures
\DosDevices\GlobalVistaVentures
\\.\GlobalVistaVentures -> OPEN_OK
```

## 导入
```
KeStackAttachProcess
PsLookupProcessByProcessId
ZwAllocateVirtualMemory
ZwFreeVirtualMemory
ZwQueryVirtualMemory
MmCopyVirtualMemory
IoCreateDevice
IoCreateSymbolicLink
MmCopyMemory
MmGetVirtualForPhysical
```

## 关键字符串
```
Virtual Address translating: %p
Failed to get physical!
Failed to readphysicalAddress
[WriteProcessMemoryPhysical] Invalid ProcessID: 0
[WriteProcessMemoryPhysical] Getting PEPROCESS for PID: %p
[WriteProcessMemoryPhysical] Failed to get PEPROCESS
[WriteProcessMemoryPhysical] Failed to translate virtual address: %p
[GetAllPages] VirtualAddress: 0x%p | CR3: 0x%llx
[AllocateAndExposeMemory] Current CR3 for ProcessID: 0x%llx
[AllocateAndExposeMemory] Current CR3 for TargetProcessID: 0x%llx
[AllocateAndExposeMemory] Processing virtual address: 0x%p
```

## IOCTL 分发
与 Kinkajou 相同的命令族：
```
0x221A4C -> 0x14000150C
0x221A50 -> 0x140001A64
0x221A54 -> 0x140001420
0x221A58 -> 0x140001B60
0x221A5C -> 0x1400018B8
0x221A60 -> 0x140001808
0x221A64 -> 0x140001D7A
0x221C00 -> 0x1400015B4
0x221C04 -> 0x1400019E4
0x221C08 -> 0x1400017B0
```

## 已确认
- `0x221A60` 处理函数是目标进程 VA 释放：
```
PsLookupProcessByProcessId
KeAttachProcess
ZwFreeVirtualMemory
KeDetachProcess
```
- `0x221A5C` 处理函数涉及分配/暴露内存，并输出 `[AllocateAndExposeMemory] ... CR3 ...` 调试字符串；
- 驱动会内部读取目标 CR3；
- 尚未确认用户态 IOCTL 是否直接返回 CR3；
- 未盲发 IOCTL。

## 结论
- `GlobalVistaVentures_v3` 是新的 **WHQL 签名、可加载、设备可打开** 的候选；
- 它具有 CR3 读取和物理内存翻译能力；
- 是最有希望的新 CR3 路径之一；
- 无 BSOD。
