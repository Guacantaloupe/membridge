# devhost.sys：系统 CR3 泄露 + 任意 CR3/VA 读候选

## 来源
LOLDrivers:
- https://www.loldrivers.io/drivers/d0cc79b2-a789-4cf3-9a88-25bdabb79416/
- 样本 SHA256：`ee8844ffd3879190fb389b0f613cb2dcdcd83375cf0a6994170a648c5ca8c479`
- WHQL attestation，HVCI TRUE

## 能力描述（LOLDrivers）
```
DriverEntry 时向用户态泄露 system CR3
IOCTL 0x28E017  硬编码 magic cookie 认证
IOCTL 0x28E01B  接收 caller 提供的 CR3 + VA
                 手动 4 级页表遍历
                 MmMapIoSpace 物理读
额外 IOCTL        NtBuildNumber / PsLoadedModuleList / KPCR CurrentThread
```
这意味着：
- 它能**获得/使用 CR3**（至少 system CR3，以及调用者提供的 CR3）；
- 属于“CR3 能力”驱动候选。

## 本机测试
下载：
```
drivers/devhost/devhost.sys
Length = 22104
```
Windows Defender 阻止读取/哈希：
```
Operation did not complete successfully because the file contains a virus or potentially unwanted software.
```
服务加载：
```
sc start -> 225 (ERROR_VIRUS_INFECTED)
```
结论：
- 文件被 Defender 拦截；
- 无法在本机签名验证/加载；
- 不通过关闭 Defender 或加排除来绕过；
- 记录为“CR3 候选，但当前环境不可用”。

## 当前 CR3 能力矩阵更新
| 驱动 | 目标 PID CR3 | 当前/系统 CR3 | 备注 |
|---|---|---|---|
| KPRL | ✅ | — | 唯一可用的目标 PID CR3 |
| CCHWApi | — | 当前 CPU | — |
| CCHWApiExt | — | 当前 CPU | — |
| devhost | — | 系统 CR3 | Defender 拦截，不可用 |
| KKYUM | ❌ | — | 进程 VA 直读可用 |
| umbus2 | ⚠️ 设备可开 | — | 协议不兼容 |
| FastDump | ⚠️ 候选 | — | 未还原 |
```
