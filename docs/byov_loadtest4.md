# byov_input 第四批：fekern 物理读 PASS

## 加载结果
| 驱动 | 状态 |
|---|---|
| `360rescon.sys` | RUNNING |
| `aac-driver.sys` | STOPPED（exit=19） |
| `asussaio.sys` | STOPPED（exit=183） |
| `CTFarmMonitor.sys` | RUNNING |
| `ctrl_isstrtc.sys` | RUNNING |
| `ctrl_jmkmd.sys` | RUNNING |
| `ctrl_mst.sys` | RUNNING |
| `ctrl_pawnio.sys` | RUNNING |
| `fekern.sys` | RUNNING |
| `FireEyeRealtime.sys` | STOPPED（exit=183） |

7/10 RUNNING，无 BSOD。

## 新设备
```
\Device\CTFarmMonitor
\Device\360rescon
\Device\ASUSSAIO
\Device\fekern_00
\Device\PawnIO
\Device\mst64_4.35.0
```
用户态打开：
```
\\.\PawnIO          OPEN_OK
\\.\CTFarmMonitor   OPEN_OK
\\.\mst64_4.35.0    OPEN_OK
\\.\360rescon       OPEN_OK
```

## fekern 物理读实测
FireEye fekern 变体，使用现有 `mrt` layout：
```
membridge --device \\.\fekern_00 --layout mrt \
  --pa 0x1000 --size 0x10 --dump
```
结果：
```
READ_OK bytes=0x10
0000: E9 4D 06 00 01 00 00 00 01 00 00 00 3F 00 18 10
VERDICT=PASS
```

这是本轮新增的**第二个真实可用的任意 PA 读驱动**：
- 可加载；
- 设备 `\\.\fekern_00` 可打开；
- `MRT_IOCTL_READ_PHYSICALMEMORY` 路径可用；
- 单页读 PASS；
- 无 BSOD。

## 当前已验证可用
1. **GLCKIO2**：单页 + 页表翻译；
2. **JYWinRing0**：低内存物理读；
3. **fekern**：任意 PA 单页物理读；
4. `PawnIO` / `CTFarmMonitor` / `mst64` / `360rescon`：设备可打开，协议待逆向。

## 安全
- 本轮只做单页读；
- 未做批量扫描；
- 未主动卸载驱动；
- 最新 dump 仍为 `112326-44078-01.dmp`。
