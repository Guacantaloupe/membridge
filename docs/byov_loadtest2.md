# byov_input 第二批加载与读测试

## 加载结果
只 `sc create/start`，不主动卸载。

| 驱动 | 状态 |
|---|---|
| `iomap.sys` | RUNNING |
| `uiomap.sys` | STOPPED（exit=183） |
| `umbus2.sys` | RUNNING |
| `sivx64.sys` | RUNNING |
| `system_monitor.sys` | RUNNING |
| `JYWinRing0.sys` | RUNNING |
| `mtxmem.sys` | RUNNING |
| `local_asussaio.sys` | STOPPED（exit=183） |
| `out_agmempci.sys` | RUNNING |
| `out_cptram.sys` | RUNNING |

8/10 RUNNING，无 BSOD。

## 新设备
```
\Device\IOMap
\Device\uiomap
\Device\JYWinRing0_130
\Device\mtxmemmanager
\Device\SIVDRIVER
\Device\SystemMonitor
\Device\CPTRAMV012
```

用户态打开：
```
\\.\IOMap              OPEN_OK
\\.\JYWinRing0_130     OPEN_OK
\\.\mtxmemmanager      OPEN_OK
\\.\SIVDRIVER          err=5
\\.\SystemMonitor      err=5
\\.\uiomap             err=5
\\.\CPTRAMV012         err=2
```

## 物理读测试
### JYWinRing0：成功
```
membridge --device \\.\JYWinRing0_130 --layout winring0 \
  --pa 0xF0000 --size 0x10 --dump
```
结果：
```
READ_OK bytes=0x10
0000: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF
VERDICT=PASS
```
限制：
- 只能低内存区域（WinRing0 语义）；
- 不能读任意 64-bit PA。

### IOMap：失败
```
IOCTL_WINIO_MAPPHYSTOLIN err=31
```

### mtxmemmanager：失败
```
eneio64 -> err=1
glckio2 -> whitelist registration err=1
```

## 结论
- `JYWinRing0_130` 是**可用的低内存物理读驱动**；
- `IOMap` / `mtxmemmanager` 设备可打开，但协议不匹配现有 layout；
- 其余多数可加载但设备受 ACL 限制；
- 无新 BSOD。
