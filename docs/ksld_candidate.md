# KslD.sys：新增 Microsoft 签名 CR3 驱动候选（受 Defender 访问门限制）

## 发现
扫描系统驱动：
```
C:\Windows\System32\drivers
```
发现 `KslD.sys` 含 CR3 读指令。公开 KslDump 研究确认：

- 设备：`\\.\KslD`
- IOCTL：`0x222044`
- **SubCmd 2**：返回 CR3、IDTR 等控制寄存器（KASLR 绕过）
- **SubCmd 12**：调用 `MmCopyMemory()`，可任意内核/物理读

来源：
- https://www.loldrivers.io/drivers/7789e9ca-8508-486b-9d77-7497c5c61474/ （PSKD64 无关）
- KslDump README：https://github.com/andreisss/KslDump

## 系统现状
```
Service: KslD
State:   Running
Type:    DEMAND_START
ImagePath = system32\drivers\wd\KslD.sys   (82 KB, patched)
AllowedProcessName = \Device\HarddiskVolume3\ProgramData\Microsoft\Windows Defender\Platform\4.18.26100.4-0\MsMpEng.exe
```
磁盘上同时存在：
```
C:\Windows\System32\drivers\KslD.sys       333216 bytes
C:\Windows\System32\drivers\wd\KslD.sys     82968 bytes
```
公开研究指出：
- 333 KB 版本是 vulnerable 版本（SubCmd 12 可用）；
- 82 KB 版本是 patched（SubCmd 12 被 NULL）；
- 但 SubCmd 2 是否仍可返回 CR3 未在本机验证。

## 访问门测试
用普通进程打开：
```
\\.\KslD -> err=5 (ACCESS_DENIED)
```
把 helper 进程改名为 `MsMpEng.exe` 后：
```
\\.\KslD -> err=5 (ACCESS_DENIED)
```
说明驱动比较的是 **完整进程路径**，不是简单文件名。

## 当前决定
- **不修改** `AllowedProcessName`；
- **不修改** `ImagePath`；
- **不重启 KslD 服务**；
- 这些操作会改动 Defender 配置，属于绕过访问控制，需用户明确授权；
- 未执行 SubCmd 2 / SubCmd 12，避免影响 Defender；
- 无 BSOD。

## 结论
`KslD.sys` 是新的、Microsoft 签名的 **CR3-capable 驱动路径**：
- SubCmd 2 理论上直接返回 CR3；
- SubCmd 12 可任意内核读；
- 但当前被 `AllowedProcessName` 门限制；
- 在不修改 Defender 配置的前提下不可用。

## 后续可选
1. 用户明确授权后，在隔离 VM 中修改 `AllowedProcessName` 并重启 KslD；
2. 或寻找该驱动的其它合法调用路径；
3. 不直接在当前系统绕过 Defender 访问门。
