# LOLDrivers 全部二进制样本静态扫描

## 方法
- 从 LOLDrivers JSON feed 提取全部唯一 MD5；
- 通过 GitHub LFS media 下载真实样本：
```
https://media.githubusercontent.com/media/magicsword-io/LOLDrivers/main/drivers/<md5>.bin
```
- 样本数：
```
2222 个唯一二进制
```
- 纯静态扫描，不加载、不执行。

## 结果
- 下载完成：**2222 / 2222**
- 含 CR3/CRx 指令或相关字符串的候选：**589 个**
- 结果文件：
```
evidence/loldrivers_binary_scan.json
```

## 重点候选
| MD5 | 文件名 | CR3 读指令 | 备注 |
|---|---|---:|---|
| `7296e9a1...` | neacsafe | 13 | 网易 NeacSafe64 反作弊；有 KeStackAttachProcess/PsLookupProcessByProcessId/MmCopyVirtualMemory |
| `e8f17754...` | ACE-BASE.sys | 10 | Anti-Cheat Expert |
| `0bee791c...` | pmxdrv.sys | 7 | Intel PMxDrv / 物理内存 |
| `a5f637d6...` | aswVmm.sys | 6 | Avast VM monitor |
| `3e62bffd...` | vsapint.sys | 6 | 可禁用 Code Integrity |
| `0b5c4cd7...` | 360Hvm64.sys | 4 | 360 安全卫士 hypervisor 驱动；记录提到 `\\.\360Hvm` IOCTL `0x22240c` |
| `ff3351f6...` | KSophon_x64.sys | 3 | 反作弊驱动，含 CR3 字符串 |
| `eb59b404...` | Kinkajou.sys | 3 | vulnerable-drivers 驱动 |
| `b316425d...` | KSLD.sys | 2 | Microsoft Defender KslD |
| `d788954c...` | AppVkgr.sys | 2 | 进程 attach / MmCopyVirtualMemory |
| `016c1d3c...` | fastdumpx64.sys | 2 | 已有 FastDump |
| `7475bfea...` | BEDAISY.SYS | 2 | BattlEye 反作弊 |

## 重要结论
- 之前按元数据搜索只发现 3 个 CR3 驱动，**并不完整**；
- 二进制扫描发现 **589 个候选**，其中很多是反作弊/安全/硬件驱动；
- CR3 指令存在不等于暴露 CR3 查询接口，需要继续静态确认设备名/IOCTL；
- 这些驱动大多高风险（反作弊、hypervisor、安全产品），不能在当前系统随意加载；
- 本轮没有加载/执行任何新候选，无 BSOD。

## 下一步
1. 对重点候选做静态 IOCTL/设备名分析；
2. 只选择无冲突、可安全加载的候选；
3. 反作弊/hypervisor 驱动需要隔离 VM；
4. 不修改 Defender、不写内核内存。
