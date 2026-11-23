# byov_input 第五批：cb_* 驱动

## 加载结果
| 驱动 | 状态 |
|---|---|
| `cb_arkmon.sys` | RUNNING |
| `cb_aswarpot.sys` | CREATE FAILED（Access denied） |
| `cb_fekern.sys` | STOPPED（exit=183） |
| `cb_gsdriver.sys` | RUNNING |
| `cb_khwinfo.sys` | RUNNING |
| `cb_livekdd.sys` | RUNNING |
| `cb_mark.sys` | RUNNING |
| `cb_scansafe.sys` | RUNNING |
| `cb_smr522.sys` | RUNNING |
| `cb_tdcore.sys` | RUNNING |

8/10 RUNNING，无 BSOD。

## 新设备
```
\Device\khwinfo_dev
\Device\arkmon_022405_MbByov5_1
\Device\TDCoreDevice
\Device\Symantec-SMR522
\GLOBAL??\arkmon
\GLOBAL??\arkmon_installer
```

用户态打开：
```
\\.\TDCoreDevice      OPEN_OK
\\.\Symantec-SMR522   OPEN_OK
\\.\khwinfo_dev       err=5
```

## 物理读尝试
```
TDCore + mrt      -> err=1
TDCore + eneio64  -> err=1
khwinfo_dev       -> open err=5
```
均失败，无 BSOD。

## 结论
- `cb_arkmon` / `cb_gsdriver` / `cb_khwinfo` / `cb_livekdd` / `cb_mark` / `cb_scansafe` / `cb_smr522` / `cb_tdcore` 可加载；
- TDCore / Symantec-SMR522 设备可打开；
- 物理读协议需要单独逆向；
- `cb_aswarpot` 在当前环境创建服务被拒绝。
