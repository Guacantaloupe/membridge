# Ld9BoxSup（WHQL 签名）调查

## 文件
```
poc-package\drivers\Ld9BoxSup.sys
deliverables\...\Ld9BoxSup.sys
```
大小 356032，签名：
```
Valid
CN=Microsoft Windows Hardware Compatibility Publisher
```

## 特征
导入：
```
IoCreateDevice
MmGetPhysicalAddress
MmProbeAndLockPages
MmMapIoSpace
```

设备已存在：
```
\Device\Ld9BoxDrv
\Device\Ld9BoxDrvU
```

## 加载/访问
直接创建服务启动：
```
start=183 (ERROR_ALREADY_EXISTS)
```
说明驱动/设备已由 LDPlayer 相关组件加载。

用户态打开测试：
```
\\.\Ld9BoxDrv   err=2
\\.\Ld9BoxDrvU  err=2
\\.\Ld9BoxSup   err=2
```
没有 DOS 符号链接，用户态无法直接打开。

## 结论
- Ld9BoxSup 是 WHQL 签名、带物理内存 API 的真实驱动；
- 但设备只在内核对象管理器可见，没有用户态符号链接；
- 当前无法作为用户态物理读入口；
- 未加载/未卸载，无 BSOD。

## 后续
1. 不启动 LDPlayerSvr 以避免副作用；
2. 如需继续，先静态分析 `Ld9BoxDrv` IOCTL 和符号链接创建逻辑；
3. 不修改系统服务配置。
