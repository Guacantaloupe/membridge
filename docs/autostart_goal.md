# 重启后自动恢复 CR3 目标的持久化配置

## 目标
保证电脑每次重启/重新登录后，DeepSeek Harness 与 CR3 目标进程自动恢复，
使会话可以继续执行“找到更多可获取 CR3 的驱动路径”这一 goal。

## 持久化组件

### 1. 计划任务 `DSH_Harness_Autostart`
- 触发：当前用户登录时（`oifnjalw\ook`），延迟 **20 秒**；
- 动作：
```
C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe
  -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden
  -File "C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\scripts\resume_cr3_goal.ps1"
```
- 设置：
  - `IgnoreNew`：避免重复启动；
  - 失败重启 3 次，间隔 1 分钟；
  - 电池模式不阻止；
  - 错过触发时尽快启动。
- 实测：
```
LastTaskResult = 0
DSH 已在运行 -> 不重复启动
cmd target 已在运行 -> 不重复启动
```

### 2. 启动文件夹快捷方式
```
%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\DeepSeek Harness CR3 Goal.lnk
```
指向同一个 PowerShell 恢复脚本，作为计划任务之外的冗余。

### 3. 恢复脚本
```
scripts/resume_cr3_goal.ps1
```
行为：
- 若 `DeepSeek Harness.exe` 未运行则启动；
- 若 `cmd.exe` 目标未运行则启动；
- 写日志：
```
evidence/resume_cr3_goal.log
```
- 写状态：
```
evidence/resume_cr3_goal_state.json
```
- **不加载任何驱动，不执行驱动测试**。

### 4. 旧的高风险任务
```
DSH_Cr3_BootRunner
```
保持 **Disabled**，避免重启后自动运行物理扫描导致 BSOD。

## 文件
- `scripts/resume_cr3_goal.ps1`
- `scripts/DSH_Harness_Autostart.xml`
- `docs/autostart_goal.md`

## 验证结果
- 计划任务状态：`Ready`
- 手动启动任务：`LastTaskResult=0`
- 恢复脚本直接运行：`exit=0`
- 日志确认：
```
DSH already running
cmd target already running
resume end dsh=True cmd=True
```
- 启动快捷方式存在：`True`
- 旧 boot runner：`Disabled`

## 限制
- 该配置保证 **DSH 应用和 CR3 目标进程** 在登录后自动恢复；
- 实际的 goal 轮次是否自动继续，取决于 DSH 自身的会话/目标持久化机制；
- 脚本无法直接调用 DSH 内部的 goal API，但会保证会话载体已启动；
- 不自动加载驱动，避免重启后引入不可控的 BSOD。
