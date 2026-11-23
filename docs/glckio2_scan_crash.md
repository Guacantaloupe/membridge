# GLCKIO2 批量扫描崩溃（0x4E_99）

## 崩溃
```
Dump: 112326-44078-01.dmp
BugCheck: 0x4E
P1: 0x99
P2: 0x119401
PROCESS: membridge.exe
FAILURE_BUCKET_ID: 0x4E_99_nt!MiBadShareCount
```

## 触发
使用新接入的 backend：
```
membridge --backend glckio2_new --no-load --locate \
  --pid <cmd> --name cmd.exe \
  --scan-start 0x100000000 --scan-size 0x80000000
```
即让 GLCKIO2 连续扫描 2GB 物理内存。

崩溃发生在扫描过程中。

## 解读
`MiBadShareCount` 属于内存管理器共享页计数错误，典型原因是：
- 驱动 `MapPhysicalMemory`/unmap 路径不正确；
- 大量映射/解除映射导致页共享计数错误；
- 驱动在进程退出时清理不完整。

GLCKIO2 与 HP/tdeio 类似：
- **单页读取安全**；
- **大范围连续映射/扫描不安全**。

## 安全规则
GLCKIO2 只用于：
- 单页读；
- 单次页表遍历（4–5 次读）；
- 已知 PA 的小读取。

禁止：
- 全 RAM 扫描；
- 2GB/512MB 连续扫描；
- 批量 `locate`。

## 仍然成立的结果
在崩溃前已验证：
- GLCKIO2 可读 CR3 页；
- 可完成 PML4/PDPT/PD/PT 翻译；
- 可读到 cmd.exe 的 MZ 头；
- 已接入 `membridge` backend `glckio2_new`。

## 当前状态
- 最新 dump：`112326-44078-01.dmp`（10:51:36）
- 启动：10:51:00
- 之后无新 BSOD
