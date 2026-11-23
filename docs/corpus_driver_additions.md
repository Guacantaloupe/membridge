# 新增语料驱动：从 1816 个驱动样本中筛选

## 来源
`msdriver-0day\evidence\fluffycats_drivers` 语料：

- `.sys` 文件：**1816**
- 已复制到项目：`drivers\corpus_samples\`

筛选脚本：
```
scripts/scan_driver_corpus.py
```
它按物理内存/映射相关 API 字符串评分，并排除项目已有 SHA256。

## 已加入的 9 个驱动
| 文件 | 大小 | 签名 | 关键特征 |
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

SHA256 清单：
```
evidence\driver_corpus_additions.json
```

## 候选价值
- **ALSysIO64**：已知的物理内存读驱动，最值得优先做 IOCTL 逆向；
- `asmmap.sys` / `WinIo64.sys`：map 型物理内存；
- `ComputerZ` / `GPU-Z` / `HWMonitor` / `RtsTpx` / `Sigdrv`：厂商硬件访问驱动，可能含物理/IO 原语。

## 下一步
1. 对 `ALSysIO64.sys` 做 IOCTL/设备名逆向；
2. 用 `membridge` layout 接入至少一个新增驱动；
3. 管理员权限下测试设备打开/读 PA；
4. 继续扫描 `fluffycats_drivers` 其余候选（1816 个总量）。

## 限制
当前 DSH 是 Medium 权限，无法加载/测试这些驱动；本次只做静态筛选、复制、签名与 SHA256 校验。
