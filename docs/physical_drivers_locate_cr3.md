# 只用 11 个物理读驱动定位 CR3

## 目标
不使用 KKYUM/KPRL，不使用内核 VA 路径，只用物理读驱动：
- 物理扫描 EPROCESS；
- 匹配 PID + ImageFileName；
- 读取 `_EPROCESS.DirectoryTableBase`（+0x28）得到 CR3。

## 原理
`membridge --locate` 内置物理 EPROCESS 扫描：

```
按 1MB 物理块读取
 -> 搜索 ImageFileName（_EPROCESS+0x338）
 -> 校验 PID（_EPROCESS+0x1D0）
 -> 校验 CR3（+0x28 非零、页对齐、<2^48）
 -> 输出 EPROCESS_PA 和 CR3
```

脚本负责：
1. 用 `layers --ranges` 获取物理 RAM 范围；
2. 分块（512MB）调用 `membridge --locate`；
3. 每个 backend 依次尝试；
4. 找到即停；
5. 结果写入 `evidence/physical_cr3_locate.json`。

## 命令
必须管理员权限：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\physical_drivers_locate_cr3.ps1 -ProcessName cmd
```

指定 PID：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\physical_drivers_locate_cr3.ps1 -ProcessId 11392 -ProcessName cmd.exe
```

默认只尝试相对安全的直接读驱动：
```
gibepext, gibepext2, cchwapi_2_12, cchwapi_ext_4_5
```

要尝试全部 11 个（有崩溃风险）：

```powershell
... -IncludeAll11
```

## 安全策略
- 默认跳过已知会崩溃的映射型驱动：
  - `hp_swtools` / `swtoolsdriver` / `hp_swtools_v1`（0x4E）
  - `isharepp`（0x3B）
  - `tdeio`（0x4E）
- 使用 `-IncludeAll11` 前，确保在 VM 或可重启环境；
- 每次扫描进度写入 JSON，崩溃后可从日志判断进度。

## 当前验证状态
- `membridge --locate-selftest`：**PASS**
  ```
  EPROCESS_PA=0x200000 PID=4242 CR3=0x300000
  ```
- 物理全量扫描需要管理员权限；当前 DSH 会话为 Medium，未执行实机验证。

## 限制
- 全 RAM 扫描较慢；
- EPROCESS 偏移基于 Win11 26200/26100 构建，换 build 需要更新；
- 部分物理页不可读时会跳过；
- HVCI 下 EPROCESS 页通常仍可读（不依赖页表），但驱动本身可能崩溃；
- 找到 CR3 后，读 CR3 页/页表仍可能被 HVCI 阻挡。
