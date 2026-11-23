# fluffycats_extra 第二批物理候选

## 加载结果
| 驱动 | 状态 |
|---|---|
| `_8501E05BDFE247FD8BDE70051C0A14D9.sys` | **RUNNING** |
| `$(LSTR_37)_heimdall.sra_10_0.sys` | **RUNNING** |
| `$(LSTR_37)_nep.sra_10_0.sys` | **RUNNING** |
| `$SYSDIR_drivers_kscsidisk64.sys` | **RUNNING** |
| `AdaptiveTransport.sys` | STOPPED（exit=2） |

4/5 RUNNING，无 BSOD。

## 设备字符串
四个成功加载的驱动都没有可见的：
```
\Device\...
\DosDevices\...
```
说明更可能是：
- PnP 过滤驱动；
- 固件/存储过滤；
- 或设备名在运行时动态生成。

本轮没有发现可直接打开的物理读设备。

## 结论
- fluffycats_extra 里的高导入评分驱动，很多是 PnP/过滤驱动；
- 评分高只说明它们使用物理内存 API，不代表对用户态开放任意 PA 读；
- 需要按服务/设备栈进一步定位，而不是只靠加载。
