# ALSysIO64 探测结果：可加载、设备可打开

## 驱动
```
drivers/corpus_samples/ALSysIO64.sys
```
来源：fluffycats / LOLDrivers 语料，签名 Valid。

## 加载
```
sc.exe create MbALSysIO64 binPath= ...\ALSysIO64.sys type= kernel start= demand
sc.exe start MbALSysIO64
```
结果：
```
STATE: RUNNING
start=0
```
按安全规则未执行 `sc stop/delete`。

## 设备对象
通过 `NtOpenDirectoryObject` / `NtQueryDirectoryObject` 枚举：

```
\Device\ALSysIO          Type=Device
\GLOBAL??\ALSysIO        Type=SymbolicLink
```

用户态打开：
```
\\.\ALSysIO
```
结果：
```
OPEN_OK
```

## 驱动字符串特征
```
ReadMemorySpace
ReadPhysicalPCI
WritePhysicalPCI
ReadPhysicalPCIDeviceRegisters
MmMapIoSpace
MmUnmapIoSpace
```

说明它可能提供：
- 物理内存读；
- PCI 物理读；
- 物理内存映射。

## 下一步
1. 反汇编 `DriverEntry` / dispatch，找 IOCTL 常量；
2. 先发一个低风险查询/版本 IOCTL（如果存在）；
3. 再对 `PA=0x1000` 做单次 8 字节读；
4. 禁止批量扫描。

## 当前状态
- 驱动保持 RUNNING；
- `\\.\ALSysIO` 可打开；
- 未发任何 DeviceIoControl；
- 无新 BSOD。

## 已发现 IOCTL
从 dispatch (0x140003bf0) 提取到：
```
0x9C40262C
0x9C402648
0x9C402650
0x9C402654
0x9C402658
0x9C402660
0x9C402664
0x9C402668
0x9C40266C
0x9C402674
0x9C402678
```
对应 handler 在 0x140004xxx 区间，尚未映射到具体功能。
