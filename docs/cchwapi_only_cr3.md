# 只使用 CCHWApi 2.9 获取 CR3（无全内存扫描）

目标：不依赖 KKYUM/KPRL/Invary、也不扫描物理 RAM，只用 **CCHWApi 2.9** 获取 cmd.exe CR3。

## 原理
CCHWApi 2.9 除物理读外，还暴露控制寄存器读接口：

| IOCTL | 功能 |
|---|---|
| `0x80240020` | `kHWAPIReadControlReg(CPU, Register, &Value)`：CPU0 + Register 3 → 读当前 **CR3** |
| `0x8024003C` | `kHWAPIReadMemory(PA, Size, &Data)`：物理读 |

组合成纯 CCHWApi 的 CR3 locator：

```
1) ReadControlReg(CPU0, CR3) -> current_cr3
   （当前进程的 CR3；内核页表在所有进程中共享）

2) 用 CCHWApi 物理读做软件页表遍历
   ntoskrnl VA -> PA -> 读取 PsInitialSystemProcess

3) 用 CCHWApi 页表遍历读取 EPROCESS.ActiveProcessLinks
   遍历到目标 cmd.exe PID

4) 读目标 EPROCESS + 0x28 -> 目标 CR3
```

全程只有：
- 1 次控制寄存器读；
- 少量页表页读（每级 8 字节）；
- 少量 EPROCESS 字段读。

**不做全物理内存扫描**，因此不会触发 CCHWApi 在大块 `rep movs` 中的崩溃路径。

## 两步执行
1. **低风险冒烟测试**：layers --cchwapi-cr3-current
   - 只做 ReadControlReg(CR3)，不做页表/EPROCESS 遍历；
2. 冒烟 PASS 后再执行完整 locator。
脚本会自动先做第 1 步，失败则直接中止，避免继续扩大风险。

## 实现
- `layers.exe --locate --cchwapi-only --pid <pid>`
  - 控制寄存器请求：`cchwapi_control_request(CPU, REG)`
  - 物理读：复用 `cchwapi` provider
  - 内核 VA 读：`cchwapi_kread` = CCHWApi 物理读 + 页表翻译
  - 进程遍历：`ntoskrnl!PsInitialSystemProcess` + `ActiveProcessLinks`
- 脚本：`scripts/cchwapi_only_cr3.ps1`
  - 加载 CCHWApi 2.9 服务
  - 启动 cmd.exe（如无）
  - 执行 locator
  - 清理服务

## 用法
```powershell
# 静态检查
pwsh -File scripts\cchwapi_only_cr3.ps1 -StaticOnly

# 流程预览
pwsh -File scripts\cchwapi_only_cr3.ps1 -DryRun

# 真实执行（管理员）
pwsh -File scripts\cchwapi_only_cr3.ps1
```

输出：
```
evidence\cchwapi_only_cr3.json
```
字段：`target_pid`、`cr3`、`eprocess`、`verdict`、`output`。

## 注意
- CCHWApi 2.9 已确认在**大范围扫描**时可能 BSOD（0x3B，CCHWApi+0x9457）；
- 本方案只做控制寄存器读和少量页表/EPROCESS 读，风险显著降低，但仍需实测；
- 若仍崩溃，保留 `CrashDumpEnabled=3` 的 dump 用 WinDbg 定位；
- 建议在一次性 VM 中验证。
