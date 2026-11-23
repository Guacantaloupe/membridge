# fluffycats 语料新增 25 个候选

## 来源
```
msdriver-0day\evidence\fluffycats_drivers
```
1816 个 `.sys` 样本中，去重后新增 25 个 x64、签名 Valid 的驱动：

```
drivers/fluffycats_extra/
```

## 新增清单
```
$(LSTR_37)_heimdall.sra_10_0.sys
$(LSTR_37)_nep.sra_10_0.sys
$0_IntelFirmwareInterface_IntelFirmwareInterface.sys
$SYSDIR_drivers_kboot64(1).sys
$SYSDIR_drivers_kboot64(2).sys
$SYSDIR_drivers_kboot64.sys
$SYSDIR_drivers_kscsidisk64.sys
$WINDIR_system32_drivers_WfNicPnp64.sys
.rsrc_1033_CORE_248(3).sys
.rsrc_1033_RCDATA_10101.sys
.rsrc_RCDATA_141.sys
_8501E05BDFE247FD8BDE70051C0A14D9.sys
_driver_SYS64_ShdcPci.sys
052e0dd77bc0c5f55a6bc028bc5b5e30_innodim64.sys
1.sys
2.0.0.620_bedrock64.sys
3933112b2.sys
503111e.sys
AdaptiveTransport(1..4).sys
AdaptiveTransport.sys
addrv2785x64(1).sys
addrv2785x64(10).sys
```

## 说明
- 这些是**候选**，尚未逐个加载/验证；
- 多数文件名来自样本重命名，实际功能需通过 PE/字符串/驱动加载确认；
- 大文件/重复驱动已过滤。

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
新增合计                  187
```
