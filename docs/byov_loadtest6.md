# byov_input 第六批：loc_* / out_* / AMS

## 加载结果
| 驱动 | 状态 |
|---|---|
| `loc_bertreader.sys` | RUNNING |
| `loc_bsitf.sys` | RUNNING |
| `loc_gna.sys` | RUNNING |
| `loc_gscaux.sys` | RUNNING |
| `loc_vboxsup.sys` | STOPPED（exit=183） |
| `nagisa.sys` | STOPPED（0x800B010C） |
| `ols_jywinring0.sys` | STOPPED（exit=183） |
| `out_nvflash.sys` | RUNNING |
| `out_stlwa.sys` | RUNNING |
| `win10_AMS_Driver.sys` | RUNNING |

7/10 RUNNING，无 BSOD。

## 新设备
```
\Device\NVFLASH
\Device\bsitf
\GLOBAL??\STLWA_64sync
\Device\VBoxDrv
\Device\VBoxNetLwf
\Device\VBoxUSBMon
```

打开：
```
\\.\NVFLASH        OPEN_OK
\\.\STLWA_64sync   OPEN_OK
\\.\bsitf          err=5
```

## 物理读尝试
```
NVFLASH + eneio64 -> err=50
NVFLASH + mrt     -> err=50
bsitf             -> open err=5
```
均失败，无 BSOD。

## 结论
- 7/10 可加载；
- NVFLASH / STLWA_64sync 设备可打开；
- 协议与现有 layout 不匹配；
- 无新 BSOD。
