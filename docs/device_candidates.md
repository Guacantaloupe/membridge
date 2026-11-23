# 设备+物理 API 双特征候选（22 个）

## 方法
新增脚本：
```
scripts/rank_device_drivers.py
```
在 fluffycats 语料中同时筛选：

- `IoCreateDevice` / `IoCreateDeviceSecure`
- `IoCreateSymbolicLink` / `IoCreateUnprotectedSymbolicLink`
- `MmMapIoSpace` / `MmGetPhysicalAddress` / `MmMapLockedPagesSpecifyCache` / `ZwOpenSection`

这样优先找：
> 既创建用户态设备对象，又具备物理内存访问 API 的驱动。

结果：
```
total=1680
top=60
```

## 已复制 22 个
目录：
```
drivers/device_candidates/
```

### TwinCAT 家族（12）
```
CLib_Tc3_System.sys
TcRteFilter(.sys / 1 / 2 / 4 / 5)
TcAnalytics(8/9/10/12/14/16).sys
```

### WinDriver / PCE / EPIX / 其它（10）
```
windrvr1260(1).sys
windrvr1251.sys
windrvr1411(1).sys
EPIXXCW6(2).SYS
Pce9852x64(2).sys
Pce9852x64(3).sys
Pce9814x64(2).sys
Pce9814x64(3).sys
PSKMAD_64.sys
vmx86.sys
```

签名均 Valid。

## 当前累计新增
```
corpus_samples             13
physmem_drivers            13
physmem_drivers_variants   29
import_candidates          12
namazso_extra               2
byov_input                 64
security_candidates         8
namazso_rest               21
fluffycats_extra           25
device_candidates          22
新增合计                  209
```

## 下一步
1. 抽样加载 TwinCAT / WinDriver 变体；
2. 优先设备名明确的：
   - `WinDriver`（`WINDRVR...`）
   - `Pce...`
   - `EPIX...`
3. 保持单页读，禁止批量扫描。
