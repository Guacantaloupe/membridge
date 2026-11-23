# MALPCI / VCANPCI 加载测试

## 结果
| 驱动 | 状态 |
|---|---|
| `MALPCI32_64(1).sys` | **RUNNING** |
| `vcanpciw.sys` | **RUNNING** |

无 BSOD。

## 设备
```
\Device\VCANPCID
```
`MALPCI` 没有出现 `\Device\MALPCIe`，可能依赖实际 PCIe 硬件。

## 结论
- 两个驱动均可加载；
- VCANPCI 创建设备，但属于 CAN 适配器驱动；
- MALPCI 无设备，缺硬件；
- 本轮没有新的物理读驱动。

## 安全
- 未发送 DeviceIoControl；
- 未批量扫描；
- 未主动卸载。
