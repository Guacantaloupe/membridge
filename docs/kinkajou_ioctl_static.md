# Kinkajou IOCTL 静态分析

## 设备
```
\Device\Kinkajou
\DosDevices\Kinkajou
```
加载：
```
sc start -> 0
\\.\Kinkajou -> OPEN_OK
```

## IOCTL 分发
```
0x221A4C -> 0x14000117C
0x221A50 -> 0x1400013C8
0x221A54 -> 0x140001000
0x221A58 -> 0x1400015A8
0x221A5C -> 0x140001630
0x221A60 -> 0x1400012A0
0x221A64 -> 0x14000134C
0x221C00 -> 0x140001248
0x221C04 -> 0x140001208
0x221C08 -> 0x140001410
```

## 已确认的 0x221A54
输入结构：
```
+0x00  qword  address
+0x08  dword  size
+0x0C  dword  pid
```
行为：
```
PsLookupProcessByProcessId
KeAttachProcess
ZwAllocateVirtualMemory
ZwLockVirtualMemory
按页修改 PTE 标志 (or 2 / bts 0xB)
KeDetachProcess
```
这是**进程 VA/PTE 操作**，不是 CR3 查询。

## 0x221C00
```
返回 [0]=1, [8]=0xdeadbeef, info=0x10
```
这是版本/魔数握手。

## 0x221A60
```
KeAttachProcess
ZwFreeVirtualMemory
KeDetachProcess
```
释放目标进程 VA。

## 代码中的 CR3
在另一内部函数：
```
KeAttachProcess
mov rbx, cr3
```
用于页表操作；**没有发现 IOCTL 把 CR3 返回给用户态**。

## 结论
- Kinkajou 可以加载/打开；
- IOCTL 主要做目标进程 VA/PTE 操作；
- 没有确认可用的 CR3 查询接口；
- 不盲发 IOCTL；
- 无 BSOD。
