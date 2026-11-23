# fluffycats 语料继续导入（第四组 30 个）

## 新增
```
amdkfd.sys / (5)–(9)
AMDPcore.sys / (1)–(5)
AMDPowerProfiler.sys / (1)–(7)
amdRxhci.sys / (1) / (2)
AMDRyzenMasterDriver.sys / (1)–(7)
```
签名均 Valid。

筛选：
- x64；
- ≤1MB；
- SHA256 去重；
- 未做加载测试。

## 说明
- `AMDRyzenMasterDriver` 可能暴露 CPU 频率/电压控制接口；
- `AMDPowerProfiler` / `AMDPcore` 是性能/功耗驱动；
- 需要静态检查是否有任意 MSR/物理内存 IOCTL。

## 当前累计
```
原有新增                  545
fluffycats_extra4            30
新增合计                  575
```
