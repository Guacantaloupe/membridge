# 内核 VA 读取绕过受保护 CR3 页（KKYUM 直接进程读）

## 结论
物理 CR3 页被 VBS/HVCI 保护、物理驱动 `IOCTL err=31` 时，仍可通过 **KKYUM 内核 VA 读取**
直接读取目标进程用户内存，不需要 CR3 页表翻译。

## 实测
目标：cmd.exe PID 11392
```
ImageBase = 0x7FF7746F0000
```
KKYUM 直接进程读：
```
[OK] KKYUM direct process read PID=11392 VA=0x7FF7746F0000 size=64:
4D 5A 90 00 03 00 00 00 04 00 00 00 FF FF 00 00
B8 00 00 00 00 00 00 00 40 00 00 00 00 00 00 00
...
VERDICT=PASS
```
- 返回的正是 cmd.exe 的 MZ/PE 头；
- 不经过物理 CR3 页；
- 不依赖 11 个物理读驱动；
- 无需 CR3 解密。

## 命令
```powershell
# 1. 加载 KKYUM
sc.exe create MembridgeKKYUM binPath= <path>\KKYUM.sys type= kernel start= demand
sc.exe start MembridgeKKYUM

# 2. 直接读目标进程用户 VA
layers.exe --read-process --pid <cmd_pid> --va 0x7FF7746F0000 --size 0x40
```

## 与 PFN 恢复的关系
- PFN 恢复已实现候选枚举，但 24H2 的 `_MMPFN.u1/u5` 解码未完全逆出；
- 受保护 CR3 页的物理读取不可用；
- **KKYUM 直接进程读绕过了这个问题**，可以直接完成“读目标 EXE 内存”的实际目标。

## 限制
- 需要目标用户 VA（PEB/ImageBase）；
- 受保护/跨权限进程可能需要更高权限；
- 物理驱动“独立读 CR3”仍需另外解决，但内存读取路径已可用。
