# 驱动能力分类（物理读 / EXE 任意 VA 读 / 内核 API）

## 扫描范围
```
仓库 .sys 总数       691
可解析 x64 驱动       656
```

## 分类统计（按导入表）
| 类别 | 筛选条件 | 数量 |
|---|---|---:|
| 物理内存 API | MmMapIoSpace / MmCopyMemory / MmMapLockedPagesSpecifyCache / MmGetPhysicalAddress / ZwOpenSection | 191（严格） |
| 进程内存 API | MmCopyVirtualMemory / (KeStackAttachProcess + PsLookupProcessByProcessId) / ZwReadVirtualMemory | 51 |
| 内核 API 相关 | MmGetSystemRoutineAddress 等 | 361（宽口径） |

注意：
- 导入表只能说明**代码路径存在**，不能证明用户态 IOCTL 可调用；
- 很多驱动是安全/反作弊/转储/硬件驱动。

## 1. 只物理内存读取
### 已验证可用
| 驱动 | 能力 | 状态 |
|---|---|---|
| GLCKIO2 | 任意 PA 单页 + 页表翻译 | 完整 PASS |
| fekern / MRT | 任意 PA 读 | 完整 PASS |
| JYWinRing0 | 低内存 PA 读 | PASS（低内存） |
| ArgusMonitor 2.40.2 | 单次 PA 读 0x9C402994 | PA=0x1000 PASS；CR3 页 err=31 |

### 候选
```
191 个物理 API 驱动
```
包括：
```
amifldrv64 / AsrDrv2 / aswArPot
byov_input\aac-driver / ctrl_jmkmd / ctrl_mst
device_candidates\Pce* / EPIX* / Tc*
device_string_candidates\Daq* / Pci9524* / prot_2k*
```

## 2. EXE / 进程任意 VA 读取
### 当前结论
没有单个已验证驱动直接提供任意进程 VA 读接口。

实际链路：
```
KPRL 取 PID 的 CR3
 -> GLCKIO2 / fekern 读 CR3 页
 -> 手动 PML4 -> PDPT -> PD -> PT
 -> 读取目标 EXE 任意 VA
```
已经实测读到 cmd.exe 的 MZ。

### 直接进程内存候选（51 个）
代表：
```
byov_input\l1malwarebits_big.sys
byov_input\fekern.sys
byov_input\cb_scansafe.sys
byov_input\hznprotect.sys
byov_input\nagisa.sys
byov_input\win10_AMS_Driver.sys
byov_input\dumpit64.sys
kprl\l1malwarebits.sys
KKYUM\KKYUM.sys
mrt\fekern_mrt.sys
dumpit\DumpIt.sys
device_candidates\PSKMAD_64.sys
device_string_candidates\arkmon(13).sys
```
这些具备内核访问进程内存的代码路径，但尚未逐个验证用户态 IOCTL。

## 3. 内核任意 API / 内核级操作
### 已知
| 驱动 | 能力 | 状态 |
|---|---|---|
| KPRL | 按 PID 取 CR3，内核级定位 | 已用于取 CR3 |
| KKYUM | 内核级原语候选 | 历史使用，未作为主路径 |
| CCHWApi | CR3/硬件 API | 仅当前 CPU |
| l1malwarebits_big | KeStackAttachProcess + MmCopyVirtualMemory | 静态候选 |
| mimidrv / mhyprot2 类 | 内核内存操作 | 当前不可用/被 Defender 处理 |

### 结论
- 内核 API 相关导入 361 个，但多数只是正常动态解析；
- 没有新的、已完整验证的“用户态任意内核 API 调用”驱动；
- KPRL 目前只用于 CR3 定位。

## 总表
| 能力 | 已验证 | 候选规模 | 备注 |
|---|---|---|---|
| 只物理内存读取 | GLCKIO2 / fekern / JYWinRing0 / Argus(待完善) | 191 | 直接 PA |
| EXE 任意 VA 读取 | 无单驱动；KPRL + PA 读 + 页表翻译可用 | 51 | 需要 CR3 |
| 内核任意 API | KPRL 取 CR3 | 361（宽） | 未完成用户态验证 |
