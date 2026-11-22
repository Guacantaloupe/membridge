# 安全的 11 驱动 CR3 获取方案（不做全内存扫描）

## 背景
之前的 11 驱动方案让每个驱动独立扫描几 GB 物理内存找 `_EPROCESS`，触发旧驱动 bug：
- CCHWApi 2.9：BSOD 0x3B，`CCHWApi+0x9457 rep movs`；
- HP map 路径：0x1A MEMORY_MANAGEMENT。

结论：**不能让 11 个旧驱动各自扫描全物理内存**。  
改成“**先定位，再让每个驱动只读 CR3 字段 8 字节**”。

## 新流程
```
1) KKYUM.sys（WHQL，HVCI PASS）
   layers --locate --kkyum --pid <cmd_pid>
   -> CR3 + EPROCESS 内核 VA
   （只走内核 VA/进程链表，不扫描物理内存）

2) 页表翻译 EPROCESS VA -> PA
   layers --translate-only --reader legacy --legacy-backend tdeio \
       --cr3 <CR3> --va <EPROCESS_VA>
   -> EPROCESS_PA
   （只读 4-5 个页表项，不做全内存扫描）

3) 11 个驱动各自读 EPROCESS_PA + 0x28 的 8 字节
   membridge --backend <id> --pa <EPROCESS_PA+0x28> --size 8
   -> 每个驱动独立得到 CR3
```

- 每个驱动**只做一次 8 字节物理读**，不再遍历 GB 级物理内存；
- KKYUM 负责安全定位，tdeio 只做页表翻译；
- CCHWApi/HP 等不会再接受大范围扫描请求；
- 如果某个驱动连 8 字节读取都不稳定，脚本会报 FAIL，不会继续扩大操作。

## 用法
```powershell
# 静态检查（不需要管理员）
pwsh -File scripts\safe_11_cr3.ps1 -StaticOnly

# 查看流程
pwsh -File scripts\safe_11_cr3.ps1 -DryRun

# 真实执行（管理员；会加载 KKYUM + 11 个驱动）
pwsh -File scripts\safe_11_cr3.ps1
```

结果：
```
evidence\safe_11_cr3.json
```
字段：`reference_cr3`、`eprocess_va`、`eprocess_pa`、`results[]`（每个 backend 的 `cr3`/`verdict`）。

## 安全注意事项
- 仍需要加载 KKYUM 和 11 个驱动，但每驱动只有一次 8 字节读；
- 第一次测试请确认 `CrashDumpEnabled=3`；
- 若某驱动仍崩溃，可从 `C:\Windows\Minidump` 的 dump 直接定位；
- CCHWApi 家族已确认在**大范围扫描**下崩溃，小范围单次读取是否安全仍需实测；
- 建议最终在一次性 VM/快照里验证。
