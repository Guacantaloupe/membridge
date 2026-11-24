# 候选驱动加载测试记录

## pmxdrv.sys
- 大小：31152
- SHA256：`82b30461dbf40ac15fce6a83b9bad2ebd05b27dea1b784eaa096422fe8927b7b`
- 签名：`CN=PAIPTAC Driver`，有效
- 导入：`IoCreateDevice`、`IoCreateSymbolicLink`、`ZwOpenSection`、`ZwMapViewOfSection`
- 设备字符串：
```
\Device\Pmxdrv
\DosDevices\PMXDRV
\Device\PhysicalMemory
```
- 加载：
```
sc start -> 0x800B010C (证书链不受信任)
```
- 不可用。

## vsapint.sys（Trend Micro VSAPI）
- 大小：7,270,224
- SHA256：`0e674d7e96e2e2e33ca96a5537cd4dd9db247249131ac91c44ebe9764efbfffe`
- 签名：WHQL 有效
- 加载：
```
sc start -> 0
STATE: RUNNING
```
- 设备：
```
\\.\VsapiNt -> ACCESS_DENIED (err=1)
\\.\TmComm  -> err=2
```
- 未继续尝试 SYSTEM 权限/IOCTL；
- 该驱动是 VSAPI 安全产品驱动，不适合盲目调用。

## Kinkajou.sys
- 加载成功、设备可打开；
- IOCTL 为进程 VA/PTE 操作；
- 无 CR3 返回接口。

## AppVkgr.sys
- WHQL KPRL 设备；
- 直接加载和 INF/KMDF 安装都返回 error31；
- 暂不可用。

## 结论
- 本轮没有新增已验证的目标 CR3 接口；
- 新候选：
  - `vsapint` 可加载但设备 ACL 限制；
  - `pmxdrv` 签名链失败；
  - `AppVkgr` KMDF 加载失败；
  - `Kinkajou` 无 CR3 输出。
- 未修改 Defender、未写内核内存、无 BSOD。
