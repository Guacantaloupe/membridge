# KDU DBPACK 全量静态扫描

## 来源
下载 KDU 仓库：
```
https://github.com/hfiref0x/KDU/archive/refs/heads/master.zip
```
解压后 DBPACK 驱动目录：
```
KDU-master\Source\Utils\DBPACK\drv
```
驱动文件数量：50+ 个 `.bin`。

## 扫描
对每个 `.bin` 搜索：
```
0F 20 /r   // mov reg, CRn
0F 22 /r   // mov CRn, reg
```
结果：
```
只有 dbutildrv2.bin 命中 1 条 0F 22（写 CRx）
```
没有发现：
- 新的 CR3 读指令驱动；
- 新的 control-register 读取驱动；
- 新的 `PsGetProcessPcb` / `MmCopyMemory` locator。

## 已知相关 DBPACK 驱动
- `AppShopDrv103.bin`：已知 CR 寄存器访问，HVCI 高风险；
- `AsrDrv106/107/107n.bin`：ASRock CR 访问，已知高风险；
- `GLCKIO2.bin`：物理读写，无 CR3；
- `AsrDrv` 系列未出现直接 CR3 读指令。

## 结论
- KDU DBPACK 中没有新的安全 CR3 locator；
- 只有 `dbutildrv2.bin` 含写 CR 指令，不能用于安全读取 CR3；
- 不加载/测试这些高风险驱动；
- 无 BSOD。

## 覆盖更新
| 来源 | 状态 |
|---|---|
| 本仓库 691 sys | 已扫 |
| 工作区 2469 sys | 已扫 |
| DriverStore 525 sys | 已扫 |
| WinSxS 2003 sys | 已扫 |
| KDU DBPACK 50+ bin | 已扫 |
| LOLDrivers 713 记录 | 元数据已扫 |

剩余未做：逐个下载 LOLDrivers 全部 713 个二进制样本；静态成本高，但当前 CR3 关键词检索已完成。
