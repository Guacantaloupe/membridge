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

## LOLDrivers 确认：PSKD64 是任意内核内存读写驱动
LOLDrivers 条目：
```
UUID: 7789e9ca-8508-486b-9d77-7497c5c61474
IOCTL: 0x220044
子操作: 0x701E / 0x701F
能力: caller-selected virtual/physical memory read/write
风险: 任意内核读/写；可移除进程保护、终止安全进程
```
公开样本 SHA256：
```
34cdbf48f5a51ca9553c14f007aaa1b67c24e1334d8cb29e80f07c80c5dc07e7
```

## 本地样本
```
drivers\security_candidates\PSKD64.SYS
```
SHA256 与 LOLDrivers 样本比对结果见 evidence。

## 协议试读（已停止）
尝试请求：
```
IOCTL 0x220044
子操作 0x701E / 0x701F
请求长度 0xE0
address 字段 0xA4 = LSTAR
size    字段 0xAC = 0x20
```
返回：
```
ok=True ret=0xC1 status=0
data 全零
```
系统未崩溃，但没有读到 LSTAR 数据；该请求也可能被驱动当作写操作处理，
因此**停止继续测试 PSKD64 的读写子操作**，避免损坏内核。

## 结论
- PSKD64 是**新的、已知可用的任意内核内存读写驱动**；
- 理论上可通过读取 EPROCESS/AE 获取目标 CR3；
- 但需要正确的请求结构，且存在内核损坏风险；
- 当前不继续盲试；需要静态逆向完整协议或公开 PoC。
