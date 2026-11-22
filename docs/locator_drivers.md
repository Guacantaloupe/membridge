# 其他可用于获取 CR3 的驱动程序

除 CCHWApi 2.9 外，仓库里还有几类 **不需要物理内存扫描** 就能拿 CR3 的驱动。

## 按安全性/可用性排序

| 驱动 | 设备/IOCTL | 获取 CR3 的方式 | 签名 | HVCI | 风险 |
|---|---|---|---|---|---|
| **KKYUM.sys** | `\\.\KKYUM` | 内核 VA 读 + `ActiveProcessLinks` 遍历 | WHQL (Microsoft Hardware Compatibility Publisher) | PASS | 最低；已验证 HVCI 下可加载并读目标进程内存 |
| **invarysensor.sys** | `\\.\invarysensor` | IOCTL `0x9C402408` = `MmCopyMemory(..., Virtual)` + 进程链表 | 厂商签名 | 需实测 | 中 |
| **l1malwarebits.sys (KPRL)** | KPRL 设备 | IOCTL `0x239584` 直接返回目标 CR3 | 厂商签名 | **可能被拒绝** | 中；HVCI 下常不可用 |
| **CCHWApi.sys 2.9** | `\\.\CCHWAPI` | IOCTL `0x80240020` 读控制寄存器 CR3 + CCHWApi 物理读做页表/EPROCESS 遍历 | 厂商签名 | PASS（可加载） | 中；大范围物理扫描会 BSOD，已改为无扫描 |

### 对比
- **KKYUM**：最推荐。它直接按内核 VA 读，不经过物理扫描；WHQL 签名，HVCI 通过。
- **Invary**：同样是内核 VA reader，适合作为 KKYUM 的替代/交叉验证。
- **KPRL**：最直接（IOCTL 返回 CR3），但 HVCI 开启时其签名可能被拒绝；适合非 HVCI/VM。
- **CCHWApi 2.9**：可以做到纯 CCHWApi 拿 CR3，但必须避免全内存扫描；当前用控制寄存器 + 少量页表读。

## 测试脚本
[scripts/test_locators_cr3.ps1](test_locators_cr3.ps1)

```powershell
# 静态检查（不需要管理员）
pwsh -File scripts\test_locators_cr3.ps1 -StaticOnly

# 查看计划
pwsh -File scripts\test_locators_cr3.ps1 -DryRun

# 真实测试（管理员；会逐个加载/卸载 locator 驱动）
pwsh -File scripts\test_locators_cr3.ps1

# 只测 KKYUM
pwsh -File scripts\test_locators_cr3.ps1 -Locator kkyum
```

每个 locator：
1. 创建并启动内核服务；
2. 启动 cmd.exe 目标；
3. 调用：
   ```powershell
   layers.exe --locate --kkyum  --pid <pid>
   layers.exe --locate --invary --pid <pid>
   layers.exe --locate --kprl   --pid <pid>
   ```
4. 解析 `CR3=0x...`；
5. 停止/删除服务；
6. 结果写入：
   ```
   evidence\locators_cr3.json
   ```

## 实机结果（2026-11-22 22:29）
KKYUM 先测通过：
```
target PID=7456
load_status=STARTED (KERNEL_DRIVER RUNNING)
KR3=0x185BBD000
EPROCESS=0xFFFF880F27578080
verdict=PASS
new_dumps=""
```
- KKYUM 内核 VA + EPROCESS 链表路径成功拿到 cmd.exe CR3；
- 没有物理扫描、没有新 BSOD；
- 结果：`evidence\locators_cr3.json`。

Invary / KPRL 还未实跑；运行全部 locator：
```powershell
pwsh -File scripts\test_locators_cr3.ps1
```

## 安全说明
- 这三个 locator **都不扫描物理 RAM**；
- 每次只测试一个驱动，失败即记录；
- KPRL 在 HVCI 下可能加载失败，这是预期结果；
- 当前系统已发生 CCHWApi/HP 相关 BSOD，建议优先 KKYUM，并在 VM 中对比 Invary/KPRL。
