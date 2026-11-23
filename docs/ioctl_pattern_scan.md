# 全驱动 IOCTL 常量扫描

## 方法
新增：
```
scripts/scan_ioctl_patterns.py
```
全量扫描 `drivers/` 下 639 个 `.sys`，查找已知物理读/映射 IOCTL 的小端常量：

```
MRT_READ      0x70F8800C -> 0C 80 F8 70
WinRing0 READ 0x9C406104 -> 04 61 40 9C
WinRing0 RW   0x9C4060CC -> CC 60 40 9C
ENEIO MAP     0x80102040 -> 40 20 10 80
```

## 结果
### MRT_READ
```
byov_input\cb_fekern.sys
byov_input\fekern.sys
mrt\fekern_mrt.sys
```
→ 仅 fekern 家族。

### WinRing0 READ
17 个，包括：
```
byov_input\360rescon.sys
byov_input\cb_khwinfo.sys
byov_input\CTFarmMonitor.sys
byov_input\JYWinRing0.sys
byov_input\ols_jywinring0.sys
HP SW TOOLS DRIVER.sys
hp_swtools_v1\HP_SWTOOLS_DRIVER_V1.sys
icachex\iCacheX.sys
isharepp\iSharePp.sys
installed_thirdparty\RtkBtfilter.sys
physmem_drivers\OpenLibSys...
```

### ENEIO MAP
19 个，包括：
```
byov_input\asussaio.sys
byov_input\ctrl_asus.sys
byov_input\local_asussaio.sys
byov_input\MsIo64.sys
byov_input\out_asuss31/33/109.sys
corpus_samples\WinIo64.sys
eneio64\eneio64.sys
glckio2\glckio2.sys
namazso_extra\GLCKIO2...
installed_thirdparty\rtwlane.sys
```

## RtkBtfilter 复测
`RtkBtfilter.sys` 含 WinRing0 READ 常量，加载：
```
STATE: RUNNING
```
设备枚举：
- 没有出现新的 `WinRing0`/`RtkBt` 设备；
- 仅有系统原有的 `BtFilter`。

结论：
- 常量命中不等于实现该 IOCTL；
- RtkBtfilter 只是蓝牙过滤驱动；
- 未发现新的物理读入口。

## 判断
- MRT 兼容驱动仍只有 fekern 家族；
- ENIEO/WinRing0 常量广泛存在于 ASUS/WinIO 驱动，但多数不是任意 PA 读；
- 扫描模式本身可作为后续筛选工具。
