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
