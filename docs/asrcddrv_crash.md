# AsrCDDrv CR 访问测试导致系统崩溃

## 测试
驱动：
```
drivers/asrcddrv/AsrCDDrv.sys
```
签名为 ASRock Ink.，可加载：
```
STATE: RUNNING
\\.\AsrCDDrv -> OPEN_OK
```

发送的 IOCTL（来自 CVE-2018-10709 ASRock CR 访问）：
```
0x22286C  READ CR
0x222870  WRITE CR
```
输入结构：
```
{ uint64 reg=3; uint64 value=0; }
```

## 崩溃
```
Dump: 112426-43437-01.dmp
时间: 2026-11-24 06:52:29
BugCheck: 0x20001
BUGCHECK_P1: 0x26
PROCESS: System
MODULE_NAME: nt
IMAGE_NAME: ntkrnlmp.exe
FAILURE_BUCKET_ID: 0x20001_26_0_nt!HvlSkCrashdumpCallbackRoutine
```
结论：
- 在 HVCI/虚拟化安全环境下，直接访问 CR 寄存器导致系统崩溃；
- 不能确定是 READ CR 还是 WRITE CR 触发，但两者都不再测试；
- **AsrCDDrv 永久标记为不安全，禁止 CR 访问测试**。

## 后续规则
1. 不再对 CR 寄存器做任何写入；
2. 读取 CR 的驱动也必须在隔离环境/可回滚环境验证；
3. 当前系统上只保留 KPRL/CCHWApi 的安全 CR3 路径。
