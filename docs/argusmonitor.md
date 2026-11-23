# ArgusMonitor：发现新的可打开设备 `\\.\ArgusMonitorCTLD`

## 驱动
```
drivers/fluffycats_extra6/ArgusMonitor.sys
```
加载：
```
STATE: RUNNING
```
签名 Valid。

## 设备
```
\Device\ArgusMonitorCTL
\GLOBAL??\ArgusMonitorCTLD
\\.\ArgusMonitorCTLD -> OPEN_OK
```

## 现有布局测试
| layout | 结果 |
|---|---|
| `mrt` | `err=1` |
| `winring0` | `ReadMemory err=1` |
| `eneio64` | `MAPPHYSTOLIN err=1` |

均失败，无 BSOD。

## 结论
- **新发现一个用户态可打开的设备**；
- 它不是现有 MRT/WinRing0/ENEIO 协议；
- 需要单独逆向 `ArgusMonitor.sys` 的 IOCTL；
- 驱动本身是硬件监控类，可能提供 MSR/IO/传感器读取；
- 暂未发现物理读入口。

## 后续
1. 静态提取 `ArgusMonitor.sys` 的 IOCTL 常量/字符串；
2. 只做小范围、已知安全 IOCTL 测试；
3. 未验证前不做批量扫描。
