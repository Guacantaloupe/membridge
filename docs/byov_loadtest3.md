# byov_input 第三批加载与设备测试

## 加载
| 驱动 | 状态 |
|---|---|
| `ctrl_rstpx.sys` | RUNNING |
| `DellBV.sys` | RUNNING |
| `dumpit64.sys` | STOPPED（exit=2） |
| `esdd64.sys` | RUNNING |
| `hznprotect.sys` | STOPPED（exit=2） |
| `MsIo64.sys` | RUNNING |
| `out_asus_109.sys` | STOPPED（exit=183） |
| `out_asuss31.sys` | STOPPED（exit=183） |
| `out_srswdrv.sys` | RUNNING |
| `Rtx_HalExt.sys` | STOPPED（exit=2） |

5/10 RUNNING，无 BSOD。

## 新设备
```
\Device\MsIo
\Device\DellBV
\Device\esdd
\Device\srswdrv
\Device\RtsTpx
\Device\LLDPCTRL
\Device\DamCtrl
```
用户态都可枚举到对应符号链接。

## 物理读尝试
用现有 layout 试读 `PA=0x1000`：

```
MsIo    + eneio64  -> err=998
esdd    + eneio64  -> err=87
DellBV  + glckio2  -> whitelist registration err=1
srswdrv + eneio64  -> err=1
```

均失败，但没有 BSOD，说明：
- 设备可打开；
- IOCTL 协议与现有 WinIO/HP/GLCKIO2 layout 不同；
- 需要单独逆向。

## 结论
- `ctrl_rstpx`、`DellBV`、`esdd64`、`MsIo64`、`out_srswdrv` 可加载；
- 设备对象可枚举；
- 物理读协议待逆向。
