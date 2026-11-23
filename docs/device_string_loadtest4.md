# TdkLib64 / DAQ2208 加载测试

## 结果
| 驱动 | 状态 |
|---|---|
| `TdkLib64.sys` | **RUNNING** |
| `Daq2208(4).sys` | **RUNNING** |

无 BSOD。

## 设备
```
\Device\TdkLib
\\.\TdkLib -> OPEN_OK
```
DAQ2208 没有出现 `\Device\DAQ2208`，可能依赖实际 DAQ 硬件。

## 物理读尝试
```
TdkLib + mrt -> err=1
```
失败，无 BSOD。

## 结论
- `TdkLib64` 可加载且设备可打开，但不是物理读协议；
- `Daq2208` 可加载但无设备（缺硬件）；
- 本轮没有新增物理读路径。
