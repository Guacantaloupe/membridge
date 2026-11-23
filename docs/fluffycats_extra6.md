# fluffycats 语料继续导入（第六组 25 个）

## 新增
```
ApsX64.sys
apx3600.sys
aqnicnvm64.sys / (1)
ArcaSD.sys
ArgusMonitor.sys / (1)–(6) / (9)–(11) / (13)
arkmon(2)/(5)/(11)/(12)/(14)/(17)/(19)/(24)
ArkVSSDisk.sys
```
签名均 Valid。

## 说明
- `ArgusMonitor`：硬件监控类驱动，可能访问 MSR/IO；
- `arkmon`：此前发现含 mimidrv/mhyprot2 字符串，风险高，**不加载**；
- `ArkVSSDisk`：磁盘/卷过滤类，**不加载**；
- `apx3600` / `aqnicnvm64`：网卡相关。

## 当前累计
```
原有新增                  625
fluffycats_extra6            25
新增合计                  650
```
