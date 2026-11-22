# CR3 页不可读证据（VBS/HVCI 真机）

## 测试
目标：cmd.exe PID 11596，用户态 ImageBase=0x7FF74C950000（PEB=0x9E796FA000，MZ 基线 PASS）。

KPRL locator：
```
[OK] 定位层 kprl: PID=11596 CR3=0x1C670E000
```

用各物理驱动直接读 CR3 页 `0x1C670E000`：
| driver | 结果 |
|---|---|
| CCHWApi 2.9 | `[ERR] cchwapi ReadMemory err=8`, READ_FAILED |
| gibepext | `IOCTL_READ_PHYSMEM err=31`, READ_FAILED |
| tdeio | `IOCTL 0x91102440 err=31`, READ_FAILED |

用 CCHWApi 作为翻译 reader：
```
--translate-only --reader cchwapi --cr3 0x1C670E000 --va 0x7FF74C950000
=> translate failed
```

KKYUM locator 当前也失败：
```
[ERR] 定位层 kkyum failed: device/ntoskrnl/EPROCESS walk error
```

## 结论
- CR3 值在物理内存范围内，但所有测试的物理读驱动都无法读取 CR3 页；
- 同一个 CR3 无法用于翻译目标用户 VA；
- 当前机器 VBS/HVCI 处于启用状态：
  ```
  VirtualizationBasedSecurityStatus = 2
  SecurityServicesRunning = {2}  # HVCI
  ```
- 现象符合：**CR3/页表所在页被 VBS/HVCI 或 CR3 保护机制隔离**，不是普通页表内容。
- 因此“11 个物理驱动独立获取 CR3”在当前受保护真机上无法继续直接推进。

## 下一步选项
1. 一次性 VM，关闭 HVCI/VBS 后复测；
2. 实现 CR3 解密/PFN 恢复（需要额外的内核读取能力）；
3. 改用 KKYUM/Invary 这类 kernel-VA locator 直接读目标内存，不再依赖物理 CR3。
