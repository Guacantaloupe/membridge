# LOLDrivers 全量 CR3 能力调查

## 方法
下载 LOLDrivers 公开 JSON feed：
```
https://www.loldrivers.io/api/drivers.json
```
记录数：
```
713 个驱动
```
新增脚本：
```
scripts/search_loldrivers_cr3.py
```
在描述、命令、样本元数据中搜索：
```
CR3
DirectoryTableBase
control register
page-table walk
system CR3
```

## 结果：只有 3 个驱动命中
| 驱动 | 能力 | 样本 SHA256 |
|---|---|---|
| **AppShopDrv103.sys** | 控制寄存器 CR0/2/3/4/8 读写；IOCTL 0x22E86C 读 CR；物理内存/MSR/端口 IO | `cea231...8219e6` |
| **AsrCDDrv.sys** | 控制寄存器 CR0/2/3/4/8 读写；任意物理内存/MSR/端口 IO | `04cc7d...5324442f` |
| **devhost.sys** | DriverEntry 泄露 system CR3；IOCTL 0x28E01B 使用调用者 CR3 + VA 做页表遍历 | `ee8844...123f1126` |

## 结论
- 整个 LOLDrivers 已知目录中，只有这 3 个驱动涉及 CR3；
- AppShopDrv103 和 AsrCDDrv 都依赖**CR 寄存器访问**，在 HVCI 下风险极高；
- AsrCDDrv 已实测导致 `0x20001` HVCI 崩溃；
- devhost 的样本被 Windows Defender 拦截；
- 没有发现其它已知的、可安全提供 CR3 的 LOLDrivers 驱动。

## 安全判断
1. 不测试 AppShopDrv103 的 CR 访问；
2. 不再测试 AsrCDDrv；
3. devhost 如需验证，只能在隔离 VM，且不关闭 Defender；
4. 目标 PID CR3 的已知安全路径仍只有 KPRL。

## 产物
- `evidence/loldrivers_cr3_hits.json`
- `scripts/search_loldrivers_cr3.py`
