# 鏂板璇枡椹卞姩锛氫粠 1816 涓┍鍔ㄦ牱鏈腑绛涢€?
## 鏉ユ簮
`msdriver-0day\evidence\fluffycats_drivers` 璇枡锛?
- `.sys` 鏂囦欢锛?*1816**
- 宸插鍒跺埌椤圭洰锛歚drivers\corpus_samples\`

绛涢€夎剼鏈細
```
scripts/scan_driver_corpus.py
```
瀹冩寜鐗╃悊鍐呭瓨/鏄犲皠鐩稿叧 API 瀛楃涓茶瘎鍒嗭紝骞舵帓闄ら」鐩凡鏈?SHA256銆?
## 宸插姞鍏ョ殑 9 涓┍鍔?| 鏂囦欢 | 澶у皬 | 绛惧悕 | 鍏抽敭鐗瑰緛 |
|---|---:|---|---|
| `ALSysIO64.sys` | 73392 | Valid | `ReadPhysical` / `WritePhysical` |
| `ALSysIO64Legacy.sys` | 80560 | Valid | `MmMapIoSpace` |
| `asmmap.sys` | 18048 | Valid | `MmGetPhysicalAddress` |
| `ComputerZ.Sys` | 46888 | Valid | `MmMapIoSpace` |
| `GPU-Z-v3.sys` | 66440 | Valid | `MmMapIoSpace` |
| `HWAuidoOs2Ec.sys` | 38824 | Valid | `MmMapIoSpace` |
| `RtsTpx.sys` | 32048 | Valid | `MmMapIoSpace` |
| `Sigdrv2785x64.sys` | 31952 | Valid | `MmMapIoSpace` |
| `WinIo64.sys` | 25320 | Valid | `MapPhysicalMemory` |
| `AsrDrv.sys` | 68208 | Valid | `MmMapIoSpace` / `MmGetPhysicalAddress` |
| `DPSysInfo.sys` | 70656 | Valid | `MmMapIoSpace` |
| `SysInfoDriverAMD.sys` | 31472 | Valid | `MmMapIoSpace` |
| `GPU-Z-v2.sys` | 49704 | Valid | `MmMapIoSpace` |

SHA256 娓呭崟锛?```
evidence\driver_corpus_additions.json
```

## 鍊欓€変环鍊?- **ALSysIO64**锛氬凡鐭ョ殑鐗╃悊鍐呭瓨璇婚┍鍔紝鏈€鍊煎緱浼樺厛鍋?IOCTL 閫嗗悜锛?- `asmmap.sys` / `WinIo64.sys`锛歮ap 鍨嬬墿鐞嗗唴瀛橈紱
- `ComputerZ` / `GPU-Z` / `HWMonitor` / `RtsTpx` / `Sigdrv`锛氬巶鍟嗙‖浠惰闂┍鍔紝鍙兘鍚墿鐞?IO 鍘熻銆?
## 涓嬩竴姝?1. 瀵?`ALSysIO64.sys` 鍋?IOCTL/璁惧鍚嶉€嗗悜锛?2. 鐢?`membridge` layout 鎺ュ叆鑷冲皯涓€涓柊澧為┍鍔紱
3. 绠＄悊鍛樻潈闄愪笅娴嬭瘯璁惧鎵撳紑/璇?PA锛?4. 缁х画鎵弿 `fluffycats_drivers` 鍏朵綑鍊欓€夛紙1816 涓€婚噺锛夈€?
## 闄愬埗
褰撳墠 DSH 鏄?Medium 鏉冮檺锛屾棤娉曞姞杞?娴嬭瘯杩欎簺椹卞姩锛涙湰娆″彧鍋氶潤鎬佺瓫閫夈€佸鍒躲€佺鍚嶄笌 SHA256 鏍￠獙銆?
