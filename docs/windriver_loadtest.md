# WinDriver 加载测试

## 驱动
```
drivers/import_candidates/windrvr1260.sys
```
Jungo WinDriver 12.60，导入：
```
MmMapIoSpace
MmGetPhysicalAddress
MmMapLockedPagesSpecifyCache
MmProbeAndLockPages
ZwOpenSection
```

## 加载结果
使用：
```
sc.exe create MbWinDrv1260 binPath= ...\windrvr1260.sys type= kernel start= demand
sc.exe start MbWinDrv1260
```

结果：
```
STATE: 4 RUNNING
WIN32_EXIT_CODE: 0
```
**驱动可成功加载并保持 RUNNING。**

按安全规则，本次没有 `sc stop/delete` 卸载，避免再次触发 0x18。

## 设备打开
尝试：
```
\\.\WINDRVR1260
\\.\windrvr1260
\\.\WinDriver
\\.\WINDRVR1281
```
均返回：
```
err=2 (ERROR_FILE_NOT_FOUND)
```

说明：
- 驱动已加载；
- 但当前没有暴露这些常见 DOS 设备符号链接；
- 设备名可能：
  - 由 WinDriver 用户态库动态创建；
  - 或需要 `\Device\...` 对象枚举；
  - 或该版本不默认创建符号链接。

## 下一步
1. 枚举 `\GLOBAL??` / `\Device` 对象，找到 WinDriver 设备；
2. 找 WinDriver 12.60 用户态库的 IOCTL 协议；
3. 如果设备可打开，再测试单次物理读；
4. 不要主动 `sc stop`，重启清场。
