# 按导入函数筛选的新驱动候选

## 方法
新增：
```
scripts/rank_driver_imports.py
```
解析 1816 个驱动样本的 PE 导入表，按以下函数评分：

```
MmMapIoSpace
MmCopyMemory
MmGetPhysicalAddress
ZwOpenSection
MmMapLockedPages
MmProbeAndLockPages
MmMapLockedPagesSpecifyCache
HalGetBusDataByOffset
```

结果：
```
scored=1764
top=100
```

输出：
```
evidence/import_ranked_candidates.json
```

## 高价值候选
| 驱动 | 评分 | 说明 |
|---|---:|---|
| `windrvr1260.sys` | 26 | Jungo WinDriver 通用硬件访问驱动 |
| `windrvr1411(2).sys` | 26 | WinDriver 另一版本 |
| `EPIXXCW6.SYS` | 26 | EPIX 采集卡驱动 |
| `Pce9852x64.sys` | 25 | Advantech/PCE 硬件驱动 |
| `PSKMAD_64(1).sys` | 24 | 反作弊驱动，含物理内存 API |
| `seraphad8.sys` | 26 | 硬件访问驱动 |
| `Dalwdm(1).sys` | 26 | DALSA 相机驱动 |

另外还有大量 `Tc*`（Beckhoff TwinCAT）、`Pce9814/9852` 变体。

## 已加入项目
复制到：
```
drivers/import_candidates/
```
共 7 个：

- windrvr1260.sys
- windrvr1411(2).sys
- EPIXXCW6.SYS
- Pce9852x64.sys
- PSKMAD_64(1).sys
- seraphad8.sys
- Dalwdm(1).sys

签名全部 Valid，SHA256 清单：
```
evidence/import_candidate_additions.json
```

## 下一步
1. 对 WinDriver (`windrvr*`) 做加载测试；
2. 如果可加载，重点逆向其 IOCTL（WinDriver 历史上支持用户态物理/PCI 访问）；
3. 对 `Pce9852x64`、`EPIXXCW6` 做设备枚举；
4. 继续按导入函数排名扩展候选。
