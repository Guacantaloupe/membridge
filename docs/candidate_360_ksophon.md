# 360Hvm64 / KSophon 测试记录

## 360Hvm64.sys
- 签名：WHQL 有效
- 设备：
```
\Device\360Hvm
\DosDevices\360Hvm
\Device\360SelfProtection
```
- 导入：
```
PsLookupProcessByProcessId
KeStackAttachProcess
MmGetPhysicalAddress
MmGetPhysicalMemoryRanges
```
- 已知 IOCTL：`0x22240c`（per-CPU DPC broadcast）
- 属于 360 hypervisor/自保护驱动；未在当前系统加载，避免与 VBS/Hyper-V 冲突。
- 本轮未测试。

## KSophon.sys（GameDriverX64 反作弊）
- 签名：Proxima Beta，有效
- 设备字符串：
```
\Device\KSophon
\DosDevices\KSophon
```
- 加载：
```
sc start -> 1016 (0x3F8)
```
- 事件：
```
An I/O operation initiated by the registry failed unrecoverably...
```
- 不可用。

## 结论
- 360Hvm64 未加载，避免 hypervisor 冲突；
- KSophon 加载失败（registry I/O 1016）；
- 本轮没有新的可用 CR3 接口；
- 无 BSOD。

## 当前候选可用性
| 候选 | 状态 |
|---|---|
| KPRL | 已验证可用 |
| AppVkgr | 加载 error31 |
| Kinkajou | 可加载，无 CR3 输出 |
| vsapint | 可加载，设备 ACL 拒绝 |
| pmxdrv | 证书链失败 |
| aswVmm | 系统阻止 0x4FB |
| ACE-BASE | 证书吊销 |
| KSophon | 加载失败 1016 |
| 360Hvm64 | 未测试，hypervisor 风险 |
| KSLD | Defender 访问门 |
| PSKD64 | 协议未解开 |
