# byov_input 新驱动加载与设备探测

## 加载测试
本轮只 `sc create/start`，不主动 `stop/delete`。

| 驱动 | 状态 |
|---|---|
| `asio3.sys` | RUNNING |
| `AsIO3_1.sys` | STOPPED（exit=183，可能已在运行/重名） |
| `AsrDrv1.sys` | RUNNING |
| `astra64.sys` | RUNNING |
| `atszio64.sys` | RUNNING |
| `ctrl_asus.sys` | RUNNING |
| `ctrl_asmio.sys` | RUNNING |

7 个中 6 个成功 RUNNING，无 BSOD。

## 新设备对象
加载后发现：
```
\Device\ATSZIO
\Device\ASUSSAIO
\Device\Asusgio3
\Device\AsrDrv203
\Device\Astra32Device0 .. Astra32Device15
\Device\LLDPCTRL
\Device\DamCtrl
\Device\AsusHITBIService
\Device\AsusTPPC / TPGM / HITTP / TPCC
\Device\AsusHidService
\Device\UMDFCtrlDev-...
```

## 用户态打开结果
成功：
```
\\.\ASUSSAIO      OPEN_OK
\\.\ATSZIO        OPEN_OK
\\.\AsrDrv203     OPEN_OK
\\.\Astra32Device0 OPEN_OK
```
失败：
```
\\.\Asusgio3      err=5 (ACCESS_DENIED)
```

## 物理读尝试
用现有 `eneio64` 布局试读 `PA=0x1000`：

```
ASUSSAIO -> mapped linear address unusable
ATSZIO   -> IOCTL_WINIO_MAPPHYSTOLIN err=87
```

说明它们的 IOCTL 协议与 WinIO map 不同，需要单独逆向；但没有触发 BSOD。

## 结论
- `asio3` / `AsrDrv1` / `astra64` / `atszio64` / `ctrl_*` 可加载；
- `ASUSSAIO`、`ATSZIO`、`AsrDrv203`、`Astra32Device*` 设备可打开；
- 但还不能直接当物理读驱动使用；
- 需要继续逆向各自 dispatch/IOCTL。

## 安全
- 本轮没有做大范围扫描；
- 没有主动卸载任何新驱动；
- 最新 dump 仍为 `112326-44078-01.dmp`（10:51:36）。
