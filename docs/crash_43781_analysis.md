# 崩溃分析：0x4E PFN_LIST_CORRUPT（MDL 双重解锁）

## 崩溃
- Dump：`C:\Windows\Minidump\112226-43781-01.dmp`
  本地副本（不提交 GitHub）：`msdriver-0day\evidence\crash_112226-43781-01.dmp`
- BugCheck：**0x4E PFN_LIST_CORRUPT**
  - Arg1：`0x7` = “A driver has unlocked a page more times than it locked it”
  - Arg2：PFN `0x19e384`
  - Arg3：`0x1108850000190885`
- 进程：**`membridge.exe`**
- 崩溃点：
```
nt!MiRemoveLockedPageChargeAndDecRef
nt!MiDoubleUnlockMdlPage
nt!MiDecommitFreePagesTail
nt!MiDecommitPagesTail
nt!MiDeleteVaDirect
nt!MiDeletePagablePteRange
nt!MiDeleteVirtualAddresses
nt!MiDeleteVad
nt!MiCleanVad
nt!MmCleanProcessAddressSpace
nt!PspRundownSingleProcess
nt!PspExitLastThread
nt!PspExitThread
nt!NtTerminateProcess
```

## 结论
- Windows 在 **membridge.exe 退出、清理用户地址空间**时发现 MDL 页被**多解锁了一次**；
- 这是驱动侧 `MmProbeAndLockPages` / `MmUnlockPages` / MDL 映射路径的 bug；
- 崩溃栈里没有第三方驱动帧，但当时已加载：
  - `CCHWApi.sys`
  - `TdeIo64.sys`
  - `gibepext.sys` / `gibepext2.sys`
  - `KKYUM.sys`
- 物理读驱动（CCHWApi/TdeIo/gibepext 家族）是重点怀疑对象；KKYUM 只做内核 VA 读，不在此崩溃栈中。

## 安全结论
- 这些遗留物理读驱动即使在“只读 8 字节”场景下，也可能在进程退出/驱动卸载时破坏内核 MDL 记账；
- 当前工作机已不适合继续做任何物理读/物理扫描驱动测试；
- KKYUM 的 CR3 locator 已实机 PASS，可继续作为安全 CR3 路径；
- 11 个物理读驱动测试只能放到一次性 VM/快照，每个驱动单独快照验证。

## 时间线
- 22:29 KKYUM locator PASS
- 22:36 CCHWApi-only 运行
- 22:38 safe_11 static 结果
- 22:38:49 重启（崩溃后）
- 22:39:25 生成 0x4E dump
