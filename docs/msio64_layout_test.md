# MsIo64 多布局复测

## 加载
```
MsIo64.sys
STATE: RUNNING
```

## 设备
```
\\.\MsIo -> 可打开
```

## 布局测试
| layout | 结果 |
|---|---|
| `winring0` | `ReadMemory err=87` |
| `mrt` | `MRT_IOCTL_READ_PHYSICALMEMORY err=87` |
| `glckio2` | `whitelist registration err=87` |
| `eneio64` | `MAPPHYSTOLIN err=998` |

## 结论
- MsIo64 可加载、设备可打开；
- 现有 4 种物理读布局都不匹配；
- 需要单独逆向其 IOCTL；
- 无 BSOD。
