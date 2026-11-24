# 新候选：AppVkgr（KPRL 设备）与 Kinkajou

## AppVkgr.sys
来源：LOLDrivers / vulnerable-drivers
- 文件名：`AppVkgr.sys`
- 本地 SHA256：`70b47bd596e76297ded3b096839082bc31f79ef58cdffeaac7308e8b9565a6d5`
- 签名：**Microsoft Windows Hardware Compatibility Publisher / WHQL 有效**
- 导入：
```
PsLookupProcessByProcessId
KeStackAttachProcess
KeUnstackDetachProcess
MmCopyVirtualMemory
MmCopyMemory
IoCreateDevice
IoCreateSymbolicLink
PsGetProcessPeb
```
- 字符串：
```
[KPRL] IoCreateDevice failed with status ...
[KPRL] IoCreateSymbolicLink failed with status ...
\Device\KPRL
\DosDevices\KPRL
C:\Users\Blluettw\Downloads\kprlRW(1)\kprlRW\x64\Release\kprl.pdb
```

### 加载测试
```
sc create MbAppVkgr ... type=kernel
sc start -> 31 (ERROR_GEN_FAILURE)
```
事件：
```
MbAppVkgr service failed to start: A device attached to the system is not functioning.
```
- 这是 **WDF/KMDF 驱动**（导入 WDFLDR.SYS / WdfVersionBind）；
- 旧 KPRL 服务已停止/删除，`\\.\KPRL` 打开 err=2；
- 仍无法启动，可能与 WDF 绑定/驱动初始化有关；
- 未继续强行加载，避免 BSOD。

## Kinkajou.sys
来源：LOLDrivers
- SHA256：`f5dd6db447868c2964c1d109e7d2cc31ebdace198b3d0a02cfba5b7ef7ae6964`
- 签名：WHQL 有效
- 设备：`\Device\Kinkajou` / `\DosDevices\Kinkajou`
- 加载：
```
sc start -> 0
\\.\Kinkajou -> OPEN_OK
```
- 导入：`IoCreateDevice`, `MmCopyMemory`, `KeStackAttachProcess`, `PsGetProcessPeb`
- IOCTL 分发地址：
```
0x221A4C, 0x221A50, 0x221A54, 0x221A58,
0x221A5C, 0x221A60, 0x221A64, 0x221C00
```
- 反汇编发现：
```
KeAttachProcess
mov rbx, cr3
```
说明它内部读取目标 CR3 并做页表/内存操作，但**未确认用户态 IOCTL 是否直接返回 CR3**；
- 不盲发 IOCTL，避免未知崩溃。

## 当前结论
- `AppVkgr` 是新的 **WHQL 签名 KPRL 设备驱动**，理论上可能提供目标 CR3/进程内存接口；
- 但当前加载失败（error 31），暂不可用；
- `Kinkajou` 可加载/打开，内部读 CR3，但接口未验证；
- 两者都不修改 Defender 配置，不写内核内存；
- 无 BSOD。

## 下一步
1. 静态逆向 AppVkgr 的 WDF 初始化/依赖，解决 error31；
2. 静态逆向 Kinkajou 的 IOCTL 处理函数，确认是否返回 CR3；
3. 只做安全 IOCTL 测试，不盲试；
4. 其它候选：ACE-BASE、aswVmm、vsapint、360Hvm64、KSophon_x64、pmxdrv。

## AppVkgr INF 安装尝试
创建 KMDF INF：
```
AppVkgr.inf
ServiceType=1
StartType=3
ServiceBinary=%12%\AppVkgr.sys
KmdfService=AppVkgr,AppVkgr.KMDF
[AppVkgr.KMDF]
KmdfLibraryVersion=1.15
```
安装：
```
rundll32 setupapi.dll,InstallHinfSection DefaultInstall 132 AppVkgr.inf
```
结果：
```
AppVkgr service 已创建
sc start -> 31 (ERROR_GEN_FAILURE)
```
仍然无法启动，说明不是简单的服务/WDF 注册问题；可能是驱动内部初始化失败或 KMDF 版本/class bind 不匹配。
- 未继续修改驱动二进制或 WDF 配置；
- 无 BSOD。
