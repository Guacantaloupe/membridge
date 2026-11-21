# membridge

**membridge** 是一个"统一物理内存读取框架"：同一套读取代码 + 可切换的**不同 Windows 内核驱动后端**。
每个后端封装一个已签名的、可在 Windows 上正常安装/加载的 `.sys` 驱动，对外提供一致的
`read_phys(pa, size) -> bytes` 能力（64 位物理地址）。

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
| `ufu_win10` | win10_ufudev64.sys | ReadMemory `0x8010206E` | 32-bit（受限） | 可加载 |
| `winring0x64` | WinRing0x64.sys | READ_MEMORY `0x9C406104` | 低 1MiB（受限） | 可加载 |

**6 个 64-bit 后端均通过 AWE 端到端自测**：框架启动 test.exe（AWE 目标），逐后端读取其物理页并校验页内标记
（结果：6/6 PASS，见 `evidence/membridge_selftest.json`）。

## 2. 快速开始

需要**管理员权限**（安装/卸载驱动服务）。

```powershell
# 列出后端
.\scripts\membridge.ps1 -Action list

# 用指定后端读取物理内存（PA 为十六进制）
.\scripts\membridge.ps1 -Action read -Backend cchwapi_2_12 -Pa 0x100000 -Size 0x100 -Dump

# 端到端自测：启动 AWE 目标 → 逐后端读取 → 校验标记（默认 6 个 64-bit 后端）
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

## 3. 驱动安装方式说明（Windows 正常安装）

| 类型 | 后端 | 安装方式 |
|---|---|---|
| 控制类驱动 | cchwapi_2_9 / cchwapi_2_12 / cchwapi_ext_4_5 / hp_swtools / swtoolsdriver / ufu_win10 / winring0x64 | 框架自动执行 `sc create <服务名> binPath=<驱动路径> type=kernel` + `sc start`；读取后自动 `stop/delete`，与手工 `sc.exe` 安装内核驱动完全相同 |
| filter 类驱动 | sjtwinio | 框架自动：`sc create`（不启动）→ 通过 SetupAPI 创建标准 ROOT 枚举 PnP 设备 → 在设备实例键写入 `Service` → disable/enable 设备 → PnP 以函数驱动方式加载 → 出现 `\\.\SJTWINIO0200`；读取后移除 root 设备并 stop/delete 服务 |

所有驱动均为已签名内核驱动，Windows 会按正常驱动加载流程校验签名/加载策略。
（当前测试主机为 HVCI 开启环境，驱动可正常加载。）

## 4. 目录结构

```
membridge/
├── README.md
├── build.ps1                      # 用 MinGW-w64 gcc 重新构建 bin/membridge.exe
├── bin/membridge.exe              # 统一读取工具（C，静态链接 setupapi）
├── src/membridge.c                # 源码
├── scripts/
│   ├── membridge.ps1              # 用户入口（list / read / selftest）
│   ├── _elev_membridge_read.ps1   # 管理员读取子脚本
│   └── _elev_membridge_selftest.ps1  # 管理员自测子脚本
├── drivers/
│   ├── CCHWApi_sys/CCHWApi.sys
│   ├── CCHWApi(1)_sys/CCHWApi(1).sys
│   ├── CCHWApiExt(1)_sys/CCHWApiExt(1).sys
│   ├── SjtWinIo_sys/SjtWinIo.sys (+ SjtWinIo_copy.sys 字节一致副本)
│   ├── HP SW TOOLS DRIVER_sys/HP SW TOOLS DRIVER.sys
│   ├── swtoolsdriver/swtoolsdriver.sys
│   ├── ufu/win10_ufudev64.sys
│   └── winring0/WinRing0x64.sys
├── test/test.exe                  # AWE 自测目标（SYSTEM 运行，打印物理页地址与标记）
├── docs/backends.json             # 后端注册表（路径/设备/layout/sha256）
└── evidence/                      # 自测结果 JSON / 日志
```

## 5. 构建

需要 MinGW-w64（`gcc`）：

```powershell
.\build.ps1
# 等价于：
# gcc -O2 -municode -o bin\membridge.exe src\membridge.c -lsetupapi -ladvapi32
```

## 6. 已验证环境与结果

- 测试主机：Windows 11 x64，**HVCI 开启**；
- `evidence/membridge_selftest.json`：6 个 64-bit 后端全部 `PASS`（AWE 目标物理页标记校验通过）；
- 自测日志：`evidence/selftest_<backend>.log`；
- 自测过程中会在临时目录/项目中写日志，并在结束后自动停止/删除自己创建的驱动服务。

## 7. 已知限制

- 需要管理员权限；驱动加载受目标系统 HVCI/签名策略/驱动 blocklist 影响（不同 Windows 版本策略版本不同）；
- `cchwapi_ext_4_5` 驱动无卸载例程：首次加载后可能驻留到重启（框架会尝试 stop/delete；失败时需重启清理）；
- `sjtwinio` 是 filter 驱动：若同一镜像已被遗留实例加载，可使用字节一致的 `SjtWinIo_copy.sys`（自测已自动使用）或重启后再用原路径；
- `ufu_win10` 仅支持 32 位物理地址；`winring0x64` 仅低 1MiB 物理内存——它们不适合"全部内存"场景；
- 框架未实现：PID→CR3、页表遍历、虚拟地址翻译；如需读取进程全部虚拟内存需自行扩展。

## 8. 安全声明

本项目仅用于**已授权的安全研究、驱动漏洞验证、取证与教学**。使用者需自行确保目标系统授权；
物理内存读写能力可导致系统崩溃或数据泄露，**建议在隔离 VM 中使用**；作者不对任何滥用负责。
