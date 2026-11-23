# ArgusMonitor 静态逆向初步

## 设备
```
\\.\ArgusMonitorCTLD -> OPEN_OK
```

## 关键导入
```
MmMapIoSpace
MmUnmapIoSpace
MmMapLockedPagesSpecifyCache
MmUnmapLockedPages
IoAllocateMdl
IoFreeMdl
MmBuildMdlForNonPagedPool
MmIsAddressValid
IoBuildDeviceIoControlRequest
IofCallDriver
HalGetBusDataByOffset
HalSetBusDataByOffset
ZwCreateFile / ZwReadFile / ZwWriteFile
```

## IOCTL 分发
反汇编 `ArgusMonitor.sys` 发现大量：
```
0x9C402014
0x9C40208C
0x9C4020A4
0x9C402110
0x9C402180
0x9C402194
0x9C4021C8
0x9C402324
0x9C402448
0x9C4024F0
0x9C402504
0x9C402544
0x9C4025D0
0x9C4026AC
0x9C4026F8
0x9C402770
0x9C4029B4
0x9C4029D4
0x9C4029E8
...
```
属于 `0x9C40xxxx` 家族，但不是标准 WinRing0/OpenLibSys 代码布局。

## 物理映射 helper
在 `0x14000B8F8` 发现一个函数：
- 调用 `MmMapIoSpace`;
- 若全局标志 `0x14000F290` 非零，改走 `MmMapLockedPagesSpecifyCache`；
- 被多处调用（0x1400083DE、0x1400087CD、0x14000885D 等）。

说明驱动确实具备**物理地址映射能力**，但需要找到正确的 IOCTL 输入结构。

## 当前判断
- ArgusMonitor 是新发现的**可打开设备 + 有 MmMapIoSpace 能力**的驱动；
- 现有 MRT/WinRing0/ENEIO 布局不匹配；
- 需要进一步定位“PA + size → 映射/读取”的 IOCTL；
- 未验证前不做 fuzz，避免未知 IOCTL 触发硬件访问。

## 下一步
1. 从 dispatch 表建立 IOCTL → handler 映射；
2. 找 handler 中调用 `0x14000B8F8` 的分支；
3. 确认输入结构（PA、size、cache）后做单页测试；
4. 最后必须用 CR3 页交叉验证。
## Dispatch → physical helper 映射结果
新增脚本：
```
scripts/analyze_argus_dispatch.py
```
解析结果：
- 41 个 IOCTL 对；
- 9 个直接调用 `0x14000B8F8` 的函数；
- 所有 41 个 IOCTL handler 都通过 `0x140008708` 间接触达该 helper。

说明：
- `0x140008708` 更像通用缓冲区/映射包装；
- 不能仅凭调用关系确定哪个 IOCTL 接受用户 PA；
- helper 也可能用于固定硬件窗口，而不是任意物理地址。

## 判断
- 仍需定位具体 handler 的参数解析；
- 不进行 IOCTL fuzz；
- ArgusMonitor 暂保留为“可打开但协议未知”的强候选。
