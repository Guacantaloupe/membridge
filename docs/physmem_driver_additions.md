# 新增 physmem_drivers 驱动（namazso/physmem_drivers）

## 来源
从 [namazso/physmem_drivers](https://github.com/namazso/physmem_drivers) 下载了 15 个新的 64 位驱动：

```
drivers/physmem_drivers/
```

全部为 MZ/PE、Authenticode `Valid`（下载时 Windows Defender 拦截了其中一份的读取，但最终文件已落盘；提交前应再核对哈希）。

## 清单
| 文件 | 大小 | 关键特征 |
|---|---:|---|
| `Agent64_05f052...sys` | 22200 | `MmGetPhysicalAddress` |
| `AsrDrv10.sys` | 21328 | `MmGetPhysicalAddress` / `MmMapIoSpace` |
| `AsrDrv101.sys` | 22280 | 同上 |
| `BS_Def64_004015...sys` | 20288 | `MmGetPhysicalAddress` / `MmMapIoSpace` |
| `gdrv.sys` | 26792 | `MmMapIoSpace` / `MmGetPhysicalAddress` |
| `HOSTNT.sys` | 13864 | 无显式物理内存字符串 |
| `HwRwDrv_21ccdd...sys` | 14592 | `MmMapIoSpace` |
| `inpoutx64_2d83cc...sys` | 16016 | I/O 端口驱动 |
| `iomem64_3D23BD...sys` | 17632 | `MmMapIoSpace` |
| `Mhyprot2.sys` | 1269880 | 反作弊驱动，非典型物理读 |
| `MsIo64.sys` | 25616 | `MapPhysicalMemory` / `PhysicalMemory` |
| `OpenLibSys_913...sys` | 14544 | `MmMapIoSpace` |
| `Se64a.sys` | 14032 | 硬件工具驱动 |
| `SysInfo.sys` | 18128 | `MmMapIoSpace` |
| `WCPU.sys` | 14904 | 硬件工具驱动 |

SHA256 清单：
```
evidence/physmem_driver_additions.json
```

## 优先级
1. `MsIo64.sys`：已知 MapPhysicalMemory；
2. `iomem64`：物理内存映射；
3. `gdrv` / `SysInfo` / `AsrDrv`：厂商物理/IO 工具；
4. `OpenLibSys` / `inpoutx64`：I/O/MSR 原语；
5. `Mhyprot2`：反作弊驱动，优先级低且体积大。

## 下一步
- 逐个 `sc create/start` + `CreateFile` 探设备；
- 对 `MsIo64` / `iomem64` 逆向 IOCTL；
- 接入 `membridge` layout；
- 继续从 namazso 仓库补更多变体。
