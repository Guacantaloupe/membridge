# 稳定定位 CR3：多源一致性方案

## 目标
不是依赖单一 locator，而是用两个独立来源交叉验证：

1. **KKYUM**：EPROCESS/ActiveProcessLinks kernel-VA walk
2. **KPRL**：IOCTL `0x239584` CR3 by PID

两者返回同一 CR3 才判定为 `PASS`。

## 用法
必须以管理员 PowerShell 运行。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\locate_cr3_stable.ps1 -ProcessName cmd
```

或指定 PID：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\locate_cr3_stable.ps1 -ProcessId 11392
```

## 流程
1. 通过 `EnumDeviceDrivers` 获取 `ntoskrnl` 基址；
2. 加载 KKYUM，执行：
   ```
   layers --locate --kkyum --pid <pid> --nt-base <nt_base>
   ```
3. 加载 KPRL，执行：
   ```
   layers --locate --kprl --pid <pid>
   ```
4. 比较两个 CR3：
   - 相同：`PASS`
   - 只有一个成功：`PASS_WITH_WARNING`
   - 不同：`CONFLICT`
   - 都失败：`FAIL`
5. 结果写入 `evidence/stable_cr3.json`。

## 为什么更稳定
- 不依赖单个驱动的失败路径；
- KKYUM 必须显式传 `nt_base`，避免它在当前 token 下 `EnumDeviceDrivers` 失败；
- KKYUM 提供 EPROCESS 佐证；
- KPRL 的 CR3 IOCTL 简单直接；
- 两者不一致会显式报 `CONFLICT`，不会静默返回错误值。

## 当前限制
- 需要管理员权限加载内核驱动；
- KKYUM 需要 `nt_base`；
- 如果 KKYUM/KPRL 任一被安全软件拦截，会退化为单源 `PASS_WITH_WARNING`；
- 仍然不解决 HVCI 下读页表的问题。

## 失败码
| 退出码 | 含义 |
|---|---|
| 0 | PASS / PASS_WITH_WARNING |
| 2 | 不是管理员 |
| 3 | NT base 获取失败 |
| 4 | 目标进程不存在 |
| 5 | 所有 locator 失败 |
| 6 | 来源结果冲突 |
