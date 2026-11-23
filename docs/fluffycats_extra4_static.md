# AMD 性能/功耗驱动静态检查

## 检查结果
| 驱动 | 设备字符串 | 关键导入 |
|---|---|---|
| `AMDPowerProfiler.sys` | 无 | `IoCreateDevice` `MmMapIoSpace` `ZwOpenSection` `MmProbeAndLockPages` |
| `AMDPcore.sys` | 无 | `IoCreateDevice` `MmMapIoSpace` |
| `amdRxhci.sys` | 无 | `IoCreateDevice` `MmMapIoSpace` `MmProbeAndLockPages` |
| `AMDRyzenMasterDriver.sys` | 同名文件未找到（导入名可能带后缀） | — |

## 判断
- 这些驱动没有明显的 `\Device\...` 字符串；
- 更可能是 PnP / ACPI / 平台性能驱动；
- 没有证据表明存在用户态任意物理读接口；
- 暂不加载，避免平台/电源管理异常。

## 当前累计
```
新增驱动候选合计            575
已验证可用的新增读驱动        3
```
