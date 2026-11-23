# 驱动资产总清单

## 当前规模
```
.dri 目录数        57
总 .sys 文件       639
新增候选           约 600
```

## 按来源分类
### 早期物理读驱动
```
CCHWApi / CCHWApiExt
gibepext / gibepext2
HP SW TOOLS DRIVER / hp_swtools_v1
tdeio
isharepp
ufu
winring0
GLCKIO2
directio
fastdumpx64
icachex
rtkio / rtkiow8x64
eneio64
SjtWinIo
winio64
```

### BYOVD 输入语料
```
byov_input                 64
corpus_samples             13
physmem_drivers            13
physmem_drivers_variants   29
import_candidates          12
namazso_extra               2
namazso_rest               21
security_candidates         8
device_candidates          22
device_string_candidates   75
```

### 新语料
```
installed_thirdparty       84
driverstore_thirdparty    114
fluffycats_extra           25
fluffycats_extra2          30
fluffycats_extra3          30
fluffycats_extra4          30
fluffycats_extra5          25
```

## 已验证可用的新增读驱动
| 驱动 | backend | 验证 |
|---|---|---|
| `GLCKIO2` | `glckio2` | 完整页表翻译 PASS |
| `fekern` | `mrt` / `fekern_byov` | CR3 页 + 完整页表翻译 PASS |
| `JYWinRing0` | `winring0` | 低内存 PASS |

## 已排除/假阳性
```
SMIBIOS64_W10      假阳性，不按 PA 返回
biostoolcommon    假阳性，返回 Data\ProgramFile
Dcpp2k            加载 0x9A 崩溃
SDDisk2k          启动挂起/整机重启
```

## 风险驱动
```
DLFdeEd / dlpfde   磁盘过滤，未加载
Daq*               缺硬件，未加载
Pci9524            当前 start=2
```

## 安全规则
1. 单页读，禁止批量物理扫描；
2. 每次只加载一个未知驱动；
3. 不主动卸载新驱动；
4. 崩溃后从 dump 确认模块；
5. 新驱动必须用 CR3 页交叉验证。
