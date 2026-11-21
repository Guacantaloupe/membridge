# membridge

**membridge** 是一个"统一内存读取框架"：同一套代码 + 可切换的**不同 Windows 内核驱动后端**。

1. **物理内存读取**：每个后端封装一个已签名的、可在 Windows 上正常安装/加载的 `.sys` 驱动，对外提供一致的
   `read_phys(pa, size) -> bytes` 能力（64 位物理地址）。
2. **任意进程 / exe 虚拟内存读取**：内置 **CR3 页表遍历**（x64 4 级页表），可以从目标进程的
   `DirectoryTableBase`（CR3）出发，把任意用户态虚拟地址翻译成物理地址，再用物理读后端读取内容。

> ⚠️ 仅限授权的安全研究/测试环境使用。框架只做小尺寸、定点读取，不做全内存扫描。
> 物理内存读/写能力可能被滥用，请在隔离 VM 中测试。

## 1. 后端驱动（可切换）

所有驱动均为 **Windows 可正常安装/加载的内核驱动**（Authenticode 有效签名；控制类通过标准
`sc create/start` 安装，filter 类通过标准 PnP root 设备绑定安装；均由框架自动完成）。

| backend id | 驱动 | 读原语 | 物理地址宽度 | 已验证 |
|---|---|---|---|---|
| `cchwapi_2_9` | CCHWApi.sys 2.9.0.0 | kHWAPIReadMemory `0x8024003C` | 64-bit | ✅ PASS |
| `cchwapi_2_12` | CCHWApi(1).sys 2.12.0.0 | kHWAPIReadMemory `0x8024003C` | 64-bit | ✅ PASS |
| `cchwapi_ext_4_5` | CCHWApiExt(1).sys 4.5.0.0 | ReadWriteMemStruct `0x802400FC` | 64-bit | ✅ PASS |
| `sjtwinio` | SjtWinIo.sys（filter 驱动） | MapPhys `0x9C40A4C0` + ReadMapped `0x9C4064D0` | 64-bit | ✅ PASS |
| `hp_swtools` | HP SW TOOLS DRIVER.sys | MapPhysicalMemory `0x9C406104`（u64 PA + size → 用户态 VA） | 64-bit | ✅ PASS |
| `swtoolsdriver` | swtoolsdriver.sys（HP 同族） | MapPhysicalMemory `0x9C406104` | 64-bit | ✅ PASS |
| `hp_swtools_v1` | HP_SWTOOLS_DRIVER_V1.sys（HP SW TOOLS V1 变体，SHA256 `23C18666…`，LOLDrivers 未收录） | MapPhysicalMemory `0x9C406104` | 64-bit | ✅ PASS |
| `winio64_elan` | WinIo64_ELAN.sys（ELAN WinIO64 变体，SHA256 `535CB6A6…`，LOLDrivers 未收录） | IOCTL_WINIO_MAPPHYSTOLIN `0x80102040` | 64-bit | ✅ PASS |
| `winio64_1` | WinIo64_1.sys（WinIO64 另一变体，SHA256 `37BD7F1C…`，LOLDrivers 未收录） | IOCTL_WINIO_MAPPHYSTOLIN `0x80102040` | 64-bit | ✅ PASS |
| `gibepext` | gibepext.sys（GIGABYTE EP 固件/取证驱动，SHA256 `5EACC166…`，LOLDrivers 未收录） | CRC32 门禁 IOCTL_READ_PHYSMEM `0x22E00C` | 64-bit | ✅ PASS |
| `fastdumpx64` | fastdumpx64.sys 2.3.0.92（FastDump，LOLDrivers 未收录） | IOCTL `0x9C400014`（u64 PA + size → 数据在输出 +0x34） | 64-bit（只读） | ✅ PASS |
| `isharepp` | iSharePp.sys（iSCSIDisk PnP 驱动） | OlsApi READ_MEMORY `0x9C406104` | 64-bit（只读已验证） | ✅ PASS |
| `rtkio` | rtkio.sys（Realtek） | IOCTL_PHYMEM_MAP `0x80002000`（u64 PA + size → 用户态 VA） | 64-bit | ✅ PASS |
| `rtkiow8x64` | rtkiow8x64.sys（Realtek W8 变体） | IOCTL_PHYMEM_MAP `0x80002000` | 64-bit | ✅ PASS |
| `eneio64` | eneio64.sys（ASUS/GIGABYTE ENE WinIO） | IOCTL_WINIO_MAPPHYSTOLIN `0x80102040` | 64-bit | ✅ PASS |
| `glckio2` | glckio2.sys（ASUS GLCKIO2，未命中官方 blocklist） | 白名单自注册（AES 加密 PID）+ IOCTL_WINIO_MAPPHYSTOLIN `0x80102040` | 64-bit | ✅ PASS |
| `ufu_win10` | win10_ufudev64.sys | ReadMemory `0x8010206E` | 32-bit（受限） | 可加载 |
| `winring0x64` | WinRing0x64.sys | READ_MEMORY `0x9C406104` | 低 1MiB（受限） | 可加载 |

**16 个 64-bit 后端均通过 AWE 端到端自测**：框架启动 test.exe（AWE 目标），逐后端读取其物理页并校验页内标记
（结果：16/16 PASS，见 `evidence/membridge_selftest.json`）。

## 2. 快速开始

需要**管理员权限**（安装/卸载驱动服务）。

```powershell
# 列出后端
.\scripts\membridge.ps1 -Action list

# 用指定后端读取物理内存（PA 为十六进制）
.\scripts\membridge.ps1 -Action read -Backend cchwapi_2_12 -Pa 0x100000 -Size 0x100 -Dump

# 端到端自测：启动 AWE 目标 → 逐后端读取 → 校验标记（默认 16 个 64-bit 后端）
.\scripts\membridge.ps1 -Action selftest

# 只测部分后端
.\scripts\membridge.ps1 -Action selftest -Backends cchwapi_2_9,hp_swtools
```

也可以直接调用底层 exe（同样需要管理员）：

```powershell
.\bin\membridge.exe --list
.\bin\membridge.exe --backend sjtwinio --pa 0x1E04AF000 --size 0x100 --dump
.\bin\membridge.exe --device \\.\CCHWAPI --layout cchwapi --pa 0x100000 --size 0x40
```

`--expect <ascii>` 可做内容校验：读取结果包含该字符串时输出 `VERDICT=PASS`。
退出码：0=PASS，2=读取成功但标记不匹配，3=设备打开失败，5=IOCTL 读失败，6=参数错误，7=需要管理员。

## 3. 任意进程 / exe 读（CR3 页表遍历）

```powershell
# 读取 cmd.exe 模块头（自动解析 PID 对应 CR3、遍历页表；--Compare 用 ReadProcessMemory 对照）
.\scripts\membridge.ps1 -Action cr3 -ProcessId 1234 -Module cmd.exe -Size 0x40 -Dump -Compare

# 读取任意用户态虚拟地址（十六进制）
.\scripts\membridge.ps1 -Action cr3 -ProcessId 1234 -Va 0x7FF7F7201000 -Size 0x100 -Dump -Compare

# 端到端 CR3 自测（自动启动 cmd.exe 目标）
.\scripts\membridge.ps1 -Action cr3selftest
```

实现链路（`src/cr3read.c`）：

1. 通过 `EnumDeviceDrivers` 取得 `ntoskrnl` 基址，解析本机 `ntoskrnl.exe` 导出表定位 `PsInitialSystemProcess`；
2. 用 **KKYUM 驱动**（`drivers/KKYUM/KKYUM.sys`，`MmCopyVirtualMemory`）读取内核结构，动态识别
   `_EPROCESS` 字段偏移（`UniqueProcessId` `+0x1D0`、`ActiveProcessLinks` `+0x1D8`、`ImageFileName` `+0x338`），
   遍历进程链表找到目标 PID；
3. 从目标 `EPROCESS->Pcb.DirectoryTableBase`（`+0x28`）取得 **CR3**；
4. 用 **eneio64 驱动**（`drivers/eneio64/eneio64.sys`，section 映射 `IOCTL_WINIO_MAPPHYSTOLIN`）读取页表页，
   执行 **x64 4 级页表遍历**（PML4 → PDPT → PD → PT，支持 1 GiB / 2 MiB 大页），把目标 VA 翻译为物理地址；
5. 用物理读后端读取最终内容；`--Compare` 会同时用 `ReadProcessMemory` 读取同一地址做对照。

> 说明：本机 HVCI 环境下，`MmMapIoSpace` 类后端（cchwapi/rtkio）读取**部分页表物理页**会失败；
> `eneio64` 的 section 映射路径可以读取这些页表页，因此 CR3 模式默认使用 `eneio64` 作为页表物理读后端。
> 页表 CR3 由 `KKYUM` 内核读辅助解析；两个驱动都由 `cr3read.exe` 自动加载/清理。

自测结果（`evidence/membridge_cr3_selftest.json`）：目标 `cmd.exe` 模块头读出 `MZ`，
`COMPARE=MATCH`，`VERDICT=PASS`。

## 4. 驱动安装方式说明（Windows 正常安装）

| 类型 | 后端 | 安装方式 |
|---|---|---|
| 控制类驱动 | cchwapi_2_9 / cchwapi_2_12 / cchwapi_ext_4_5 / hp_swtools / swtoolsdriver / hp_swtools_v1 / isharepp / rtkio / rtkiow8x64 / eneio64 / winio64_elan / winio64_1 / glckio2 / gibepext / fastdumpx64 / ufu_win10 / winring0x64 | 框架自动执行 `sc create <服务名> binPath=<驱动路径> type=kernel` + `sc start`；读取后自动 `stop/delete`，与手工 `sc.exe` 安装内核驱动完全相同 |
| filter 类驱动 | sjtwinio | 框架自动：`sc create`（不启动）→ 通过 SetupAPI 创建标准 ROOT 枚举 PnP 设备 → 在设备实例键写入 `Service` → disable/enable 设备 → PnP 以函数驱动方式加载 → 出现 `\\.\SJTWINIO0200`；读取后移除 root 设备并 stop/delete 服务 |
| CR3 辅助驱动 | KKYUM.sys（内核结构读）、eneio64.sys（页表物理读） | `cr3read.exe` 自动 `sc create/start`；结束后尝试 stop/delete（KKYUM 可能驻留到重启） |

所有驱动均为已签名内核驱动，Windows 会按正常驱动加载流程校验签名/加载策略。
（当前测试主机为 HVCI 开启环境，驱动可正常加载。）

## 5. 目录结构

```
membridge/
├── README.md
├── build.ps1                      # 用 MinGW-w64 gcc 重新构建 bin/membridge.exe + bin/cr3read.exe
├── bin/membridge.exe              # 统一物理读工具（C，静态链接 setupapi）
├── bin/cr3read.exe                # CR3 页表遍历 / 任意进程 VA 读工具
├── src/membridge.c                # 物理读框架源码
├── src/cr3read.c                  # CR3 解析 + 页表遍历源码
├── scripts/
│   ├── membridge.ps1              # 用户入口（list / read / selftest / cr3 / cr3selftest）
│   ├── _elev_membridge_read.ps1   # 管理员读取子脚本
│   ├── _elev_membridge_selftest.ps1   # 管理员后端自测子脚本
│   ├── _elev_membridge_cr3.ps1        # 管理员 CR3 读取子脚本
│   └── _elev_membridge_cr3_selftest.ps1  # 管理员 CR3 自测子脚本
├── drivers/
│   ├── CCHWApi_sys/CCHWApi.sys
│   ├── CCHWApi(1)_sys/CCHWApi(1).sys
│   ├── CCHWApiExt(1)_sys/CCHWApiExt(1).sys
│   ├── SjtWinIo_sys/SjtWinIo.sys (+ SjtWinIo_copy.sys 字节一致副本)
│   ├── HP SW TOOLS DRIVER_sys/HP SW TOOLS DRIVER.sys
│   ├── swtoolsdriver/swtoolsdriver.sys
│   ├── hp_swtools_v1/HP_SWTOOLS_DRIVER_V1.sys
│   ├── isharepp/iSharePp.sys
│   ├── rtkio/rtkio.sys
│   ├── rtkiow8x64/rtkiow8x64.sys
│   ├── eneio64/eneio64.sys
│   ├── glckio2/glckio2.sys
│   ├── gibepext/gibepext.sys
│   ├── fastdumpx64/fastdumpx64.sys
│   ├── winio64_elan/WinIo64_ELAN.sys
│   ├── winio64_1/WinIo64_1.sys
│   ├── KKYUM/KKYUM.sys
│   ├── ufu/win10_ufudev64.sys
│   └── winring0/WinRing0x64.sys
├── test/test.exe                  # AWE 自测目标（SYSTEM 运行，打印物理页地址与标记）
├── docs/backends.json             # 后端注册表（路径/设备/layout/sha256）
└── evidence/                      # 自测结果 JSON / 日志
```

## 6. 构建

需要 MinGW-w64（`gcc`）：

```powershell
.\build.ps1
# 等价于：
# gcc -O2 -municode -o bin\membridge.exe src\membridge.c -lsetupapi -ladvapi32
# gcc -O2 -o bin\cr3read.exe src\cr3read.c -lpsapi -ladvapi32
```

## 7. 已验证环境与结果

- 测试主机：Windows 11 x64，**HVCI 开启**；
- `evidence/membridge_selftest.json`：10 个 64-bit 物理读后端全部 `PASS`（AWE 目标物理页标记校验通过）；
- `evidence/membridge_cr3_selftest.json`：CR3 页表遍历自测 `PASS`（cmd.exe 模块头 `MZ`，与 ReadProcessMemory 对照 MATCH）；
- 自测日志：`evidence/selftest_<backend>.log`、`evidence/_elev_membridge_cr3_selftest.log`；
- 自测过程中会在临时目录/项目中写日志，并在结束后自动停止/删除自己创建的驱动服务。

## 8. 已知限制

- 需要管理员权限；驱动加载受目标系统 HVCI/签名策略/驱动 blocklist 影响（不同 Windows 版本策略版本不同）；
- `cchwapi_ext_4_5` 驱动无卸载例程：首次加载后可能驻留到重启（框架会尝试 stop/delete；失败时需重启清理）；
- `sjtwinio` 是 filter 驱动：若同一镜像已被遗留实例加载，可使用字节一致的 `SjtWinIo_copy.sys`（自测已自动使用）或重启后再用原路径；
- `isharepp` **只集成了只读路径**（OlsApi READ_MEMORY `0x9C406104`）；该驱动还存在物理写 / MSR / PCI 等 IOCTL（如 `0x9C40A108`），框架不会调用；写路径在测试中曾出现 0x50/挂起，未验证，请勿在框架外随意调用；
- `ufu_win10` 仅支持 32 位物理地址；`winring0x64` 仅低 1MiB 物理内存——它们不适合"全部内存"场景；
- CR3 模式要求目标进程存活，且仅实现 **x64 4 级页表**（5 级页表未实现）；同时依赖 KKYUM 内核读 + eneio64 section 映射，KKYUM 可能驻留到重启；
- CR3 自测的目标必须是普通 Win32 进程（如 cmd.exe）；部分打包应用（如 Windows 11 新版 Notepad）可能无法用 Toolhelp 获取模块基址，可改用 `-Va` 显式指定。

## 9. 安全声明

本项目仅用于**已授权的安全研究、驱动漏洞验证、取证与教学**。使用者需自行确保目标系统授权；
物理内存读写能力可导致系统崩溃或数据泄露，**建议在隔离 VM 中使用**；作者不对任何滥用负责。
