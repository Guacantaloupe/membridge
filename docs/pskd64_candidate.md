# PSKD64 / PCIScope CR3 候选检查

## 来源
`driver-security` 语料中的 PCIScope 驱动：
```
PSKD64.SYS
PSKD.SYS (32-bit)
```
静态特征：
- 含 CR3 读指令；
- 主要功能为 PCI 配置空间枚举 / PCIScope 硬件访问；
- 日志字符串包括：
```
CKernPciBIOS
Access to the PCI Config. space
Enumerating PCI Buses ...
ReadPCIDevConfSpace
```

## 加载
```
sc create MbPskd64 ...
STATE: RUNNING
```

设备：
```
\\.\PSKD64 -> OPEN_OK
\\.\PSKD -> err=2
\\.\PCIScope -> err=2
\\.\Global\PSKD64 -> OPEN_OK
```

## 分析
- 驱动可加载、设备可打开；
- 其功能以 PCI 配置空间/总线枚举为主；
- 未发现明确的 CR3 查询 IOCTL；
- 需要进一步逆向 IOCTL 才能确认是否有控制寄存器访问；
- 暂不作为 CR3 路径。

## 其它 CR3 指令候选（driver-security）
```
RTKVAC.sys       (6 次 CR3 读)
ibtpci.sys       (5)
ibtusb.sys       (3)
PSKD.SYS         (2)
PSKD64.SYS       (1)
nvpcf.sys        (1)
RTKVAC64.sys     (1)
```
这些主要是音频/蓝牙/PCI 配置驱动，未发现用户态 CR3 查询接口。

## 结论
- 没有新增可用的目标 CR3 路径；
- PSKD64 是唯一可打开设备的新候选，但当前证据只到 PCI 配置空间；
- 无 BSOD。
