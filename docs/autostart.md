# 开机自动启动 Harness 与提权目标

## 已实现
- 计划任务：**`DSH_Harness_Autostart`**
- 触发：用户 `oifnjalw\ook` 登录时（AtLogOn）
- 权限：**RunLevel = Highest**（管理员令牌，不需要改 UAC 设置）
- 动作：
  ```
  powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden `
    -File ...\scripts\autostart_dsh.ps1 -StartTarget
  ```
- `autostart_dsh.ps1` 会：
  1. 启动 `DeepSeek Harness.exe`（如果未运行）；
  2. 启动 `cmd.exe` 目标（`-StartTarget`）；
  3. 写入状态和日志：
     - `evidence\autostart_state.json`
     - `evidence\autostart.log`

## 安装/卸载
```powershell
# 管理员 PowerShell
cd C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\scripts
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install_autostart.ps1 -StartTarget

# 卸载
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\uninstall_autostart.ps1
```

当前任务状态：
```
TaskName              State
DSH_Harness_Autostart Ready
RunLevel              Highest
LastTaskResult        0
```

手动触发测试：
```powershell
Start-ScheduledTask -TaskName DSH_Harness_Autostart
```
日志确认：
```
autostart begin
DSH already running
started cmd target
autostart end
```

## 一驱动一次启动尝试（boot runner）— 已禁用
任务 **`DSH_Cr3_BootRunner`** 已安装但当前 **Disabled**：
- 在真机上多次扫描导致 0x3B/0x4E/0x4A BSOD；
- 为避免自动崩溃循环，已执行：
  ```powershell
  Disable-ScheduledTask -TaskName DSH_Cr3_BootRunner
  ```
- 保留任务定义，需要时手动启用：
  ```powershell
  Enable-ScheduledTask -TaskName DSH_Cr3_BootRunner
  ```
- 只有 `DSH_Harness_Autostart` 仍保持 Ready（只启动 Harness + cmd 目标，不跑驱动扫描）。

### 物理驱动扫描安全策略
- **不再在真机自动运行任何物理驱动扫描；**
- 11 驱动测试只在一次性 VM/快照中进行；
- 真机只保留 Harness 自启动、目标进程、手动、单驱动、单次测试；
- 任何会自动加载多个物理驱动的脚本都必须先确认运行环境是 VM。

状态文件：
```
evidence\boot_cr3_progress.json
evidence\boot_cr3_runner.log
```

## UAC
保持：
```
EnableLUA = 1
ConsentPromptBehaviorAdmin = 0
PromptOnSecureDesktop = 0
```
任务使用 Highest 令牌，因此 DSH 和 cmd 目标以管理员权限启动，但不修改 UAC 策略；手动运行其他程序时 UAC 仍开启。

## 注意
- 自动启动只负责 Harness + 目标进程，**不会自动执行 11 驱动物理读测试**；
- 物理读驱动测试仍然需要单独运行，避免每次重启自动触发 BSOD。
- 需要自动执行测试时，可以在此基础上增加一个带崩溃保护/一次性标志的 runner。
