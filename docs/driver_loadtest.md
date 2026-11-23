# 新增驱动加载测试结果

## 目的
对新加入的 27 个候选驱动做最小加载测试，不发送 DeviceIoControl，不做物理读。

脚本：
```
scripts/loadtest_driver_samples.ps1
```
结果：
```
evidence/driver_loadtest.json
```

## 结果概览
- 候选：27
- 成功 RUNNING：**12**
- 被签名/吊销策略阻止：**14**（`0x800B010C`）
- 启动错误：**1**（`DPSysInfo.sys`，start=2）
- 未触发 BSOD

## 可加载的 12 个驱动
| 驱动 | 来源 |
|---|---|
| `HOSTNT.sys` | namazso |
| `Se64a.sys` | namazso |
| `ALSysIO64.sys` | fluffycats |
| `ALSysIO64Legacy.sys` | fluffycats |
| `AsrDrv.sys` | fluffycats |
| `ComputerZ.Sys` | fluffycats |
| `GPU-Z-v2.sys` | fluffycats |
| `GPU-Z-v3.sys` | fluffycats |
| `HWAuidoOs2Ec.sys` | fluffycats |
| `RtsTpx.sys` | fluffycats |
| `Sigdrv2785x64.sys` | fluffycats |
| `SysInfoDriverAMD.sys` | fluffycats |

## 被阻止的驱动
以下驱动 `sc start` 返回 `0x800B010C`（Windows 驱动签名/吊销策略拒绝）：
```
Agent64
AsrDrv10
AsrDrv101
BS_Def64
gdrv
inpoutx64
iomem64
Mhyprot2
MsIo64
OpenLibSys
SysInfo
WCPU
asmmap
WinIo64
```

## 失败
```
DPSysInfo.sys: start=2
```

## 下一步
1. 对 12 个可加载驱动枚举设备对象；
2. 对 `ALSysIO64`、`AsrDrv`、`ComputerZ`、`GPU-Z` 优先逆向 IOCTL；
3. 用 `membridge` 接入至少一个新增驱动；
4. 每次只发单次小读取，避免重演 HP/tdeio 的 0x4E。
