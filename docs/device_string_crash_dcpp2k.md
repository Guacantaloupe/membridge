# device_string_candidates 加载崩溃：Dcpp2k.sys

## 崩溃
```
Dump: 112326-41453-01.dmp
BugCheck: 0x9A
P1: 0xED
PROCESS: System
MODULE_NAME: Dcpp2k
IMAGE_NAME: Dcpp2k.sys
FAILURE_BUCKET_ID: 0x9A_Dcpp2k!unknown_function
```

## 触发
加载测试：
```
arkmon(13).sys
Dcpp2k.sys
Pci9524(14).sys
```
崩溃发生在 `Dcpp2k.sys` 启动阶段。

## 结论
`Dcpp2k.sys` 是**不安全驱动**：
- 加载即触发 0x9A（非法锁持有者）；
- 不应再在真机加载；
- 已从后续测试中排除。

## 其它候选
- `arkmon(13).sys`：包含 IObitUnlocker / mimidrv / mhyprot2 等设备字符串，风险高，暂不再测；
- `Pci9524(14).sys`：未测到；
- `SDDisk2k` / `flash` / `TdkLib64` / `Daq*`：待测。

## 安全规则
1. `Dcpp2k` 永久排除；
2. 含有 `mimidrv` / `mhyprot2` / `PROCEXP152` 字符串的驱动先静态分析；
3. 每次只加载一个未知驱动；
4. 不主动卸载；
5. 崩溃后从 dump 确认模块再继续。

## 当前状态
- 启动：11:28:26
- 最新 dump：`112326-41453-01.dmp`（11:29:00）
- 之后无新 BSOD
