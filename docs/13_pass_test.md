# 13 个 HVCI-PASS legacy backend 独立读取 EXE 内存测试

目标：验证 13 个 HVCI-PASS legacy backend **各自独立加载驱动、读取真实 EXE 进程内存**。

## 13 个 PASS backend

| # | backend | 备注 |
|---|---|---|
| 1 | `cchwapi_2_9` | CCHWApi.sys 2.9.0.0 |
| 2 | `cchwapi_2_12` | CCHWApi(1).sys 2.12.0.0 |
| 3 | `cchwapi_ext_4_5` | CCHWApiExt(1).sys 4.5.0.0 |
| 4 | `hp_swtools` | HP SW TOOLS DRIVER |
| 5 | `swtoolsdriver` | swtoolsdriver.sys |
| 6 | `hp_swtools_v1` | HP SW TOOLS DRIVER V1 |
| 7 | `isharepp` | iSharePp.sys |
| 8 | `gibepext` | GIGABYTE EP |
| 9 | `gibepext2` | GIGABYTE EP variant |
| 10 | `tdeio` | TdeIo64.sys |
| 11 | `mrt` | FireEye fekern MRT |
| 12 | `ufu_win10` | **仅 32-bit PA / 0x1024 字节** |
| 13 | `winring0x64` | **仅低内存 0xC0000–0xFFFFF** |

> 13 个是 HVCI 审计的 PASS 子集；其中 `ufu_win10`、`winring0x64` 能力受限，动态测试可能因目标 AWE 页物理地址过高而失败。

## 测试目标
`test\test.exe` 是 AWE 测试进程，输出：
```
PAGE[00] PFN=0x... PA=0x...
```
其 AWE 页内容包含标记：
```
BYOV_TESTEXE_AWE_PAGE_00
```
每个 backend 用 `membridge.exe --backend <id> --pa <PA> --expect BYOV_TESTEXE_AWE_PAGE_00` 独立读取该 EXE 内存页；`VERDICT=PASS` 表示该驱动确实读到了目标 EXE 的物理内存。

## 用法
```powershell
# 1) 静态检查：13 个 backend 是否都在 membridge 注册（不需要管理员）
pwsh -File scripts\test_13_pass_exe_read.ps1 -StaticOnly

# 2) DryRun：打印管理员测试计划（不需要管理员）
pwsh -File scripts\test_13_pass_exe_read.ps1 -DryRun

# 3) 前置条件审计：13 个驱动文件存在/签名、HVCI、目标 EXE、是否管理员
pwsh -File scripts\test_13_pass_exe_read.ps1 -CheckPrereqs

# 4) 真实测试：需要管理员（启动 SYSTEM AWE 目标 + 加载 13 个驱动）
pwsh -File scripts\test_13_pass_exe_read.ps1
```
前置审计结果写入：
```
evidence\test_13_pass_prereqs.json
```
真实测试调用现有 `scripts/_elev_membridge_selftest.ps1 -Backends <13 ids>`，结果写入：
```
evidence\test_13_pass_exe_read.json
```
字段：`page_pa`、`total`、`pass`、`results[]`、`new_dumps`。

## 当前状态
- 静态检查：**13/13 PRESENT, STATIC_ONLY=PASS**；
- 前置条件审计（`-CheckPrereqs`）：
  - 13 个驱动文件**全部存在**；
  - 13 个签名**全部 Valid**；
  - `test\test.exe` 存在；
  - HVCI=1；blocklist 策略值 absent；
  - `is_admin=False` → **动态测试仍被阻塞**；
- 动态真实测试：**未执行**，当前会话是 Medium 非管理员，无法启动 SYSTEM AWE 任务/加载内核驱动；
- 在管理员环境运行第 4 条命令即可得到真实 13 个 backend 的 PASS/FAIL 与驱动加载错误。
