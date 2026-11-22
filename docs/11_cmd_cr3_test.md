# 11 个真实 PASS 驱动独立获取 cmd.exe CR3

来源：真实 EXE 内存读取测试中 **11/13 PASS** 的驱动（排除 `mrt`：SYSTEM-only ACL；排除 `winring0x64`：仅低内存）。

## 11 个 backend
```
cchwapi_2_9       cchwapi_2_12      cchwapi_ext_4_5
hp_swtools        swtoolsdriver     hp_swtools_v1
isharepp          gibepext          gibepext2
tdeio             ufu_win10
```

## 原理
每个 backend 独立加载自己的驱动，用 `membridge --locate` 做物理 EPROCESS 扫描：

```
membridge.exe --backend <id> --locate --pid <cmd_pid> --name cmd.exe \
    --scan-start <物理范围-start> --scan-size <物理范围-size>
```

扫描逻辑（Win11 26200）：
- 搜索 `_EPROCESS.ImageFileName`（+0x338）== `cmd.exe`；
- 校验 `UniqueProcessId`（+0x1D0）== cmd.exe PID；
- 读 `DirectoryTableBase`（+0x28）→ CR3；
- 找到后输出 `CR3=0x... / VERDICT=PASS`。

## 用法
```powershell
# 静态检查
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -StaticOnly
# 该模式同时运行 membridge --locate-selftest（合成 EPROCESS 扫描，验证扫描器）
membridge.exe --locate-selftest

# 前置条件审计（驱动文件/签名、崩溃转储设置、磁盘、管理员、目标）
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -CheckPrereqs

# 查看物理范围和命令计划
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -DryRun

# 真实测试（管理员；脚本可自动启动 cmd.exe 目标）
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -MaxBackends 1 -MaxRanges 1
```

### 限制扫描范围（避免全 RAM 太慢）
```powershell
# 只扫 0x100000 开始 1GB
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -ScanStart 0x100000 -ScanSize 0x40000000

# 只跑前 3 个 backend 做快速验证
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -MaxBackends 3

# 只扫前 2 个物理范围
pwsh -File scripts\test_11_pass_cmd_cr3.ps1 -MaxRanges 2
```

## 结果
写入：
```
evidence\test_11_pass_cmd_cr3.json
```
字段：`target_pid`、`total`、`pass`、`results[] (backend/cr3/verdict/last_output)`、`new_dumps`。

## 注意
- 必须先启动 `cmd.exe`；脚本会取第一个 `cmd` 进程 PID；
- 每个 backend 全 RAM 扫描会非常慢，建议先用 `-ScanStart/-ScanSize` 或 `-MaxRanges` 缩小范围；
- `ufu_win10` 文档上为 32-bit PA 驱动，可能无法扫描高地址范围；
- 加载 11 个内核驱动有 BSOD 风险；脚本会记录 before/after minidump；仅在授权测试环境使用。
