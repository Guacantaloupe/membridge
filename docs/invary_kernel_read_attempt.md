# Invary Sensor 内核 VA 读取尝试

## 目标
Invary Sensor 提供 IOCTL：
```
0x9C402408 -> MmCopyMemory(..., flags=2=Virtual)
```
尝试用其作为安全内核 VA 读取路径。

## 直接 IOCTL 测试
设备：
```
\\.\invarysensor -> OPEN_OK
```
请求：
```
input 0x20:
  +0x00 u64 VA = 0xFFFFF801DD2CF000 (LSTAR)
  +0x08 u64 size = 0x20
  +0x10 u8 append = 0
output: 0x28 / 0x1000
```
结果：
```
outlen=0x28   -> ok=False err=487 (ERROR_INVALID_ADDRESS)
outlen=0x1000 -> ok=False err=0x80090005 (NTE_BAD_DATA?)
```
没有 BSOD。

## 结论
- Invary Sensor 设备可打开；
- `0x9C402408` 在没有初始化/append 的情况下拒绝读取；
- 可能是：
  - 需要先调用 0x9C402400/0x9C402404 初始化；
  - 或需要特定 session/append 参数；
- 暂不能作为内核 VA 读取路径；
- 不再做内核地址探测。

## 当前安全路径
- KPRL CR3-by-PID：唯一目标 CR3 查询；
- KKYUM 用户进程 VA 读：已验证；
- CCHWApi current CR3：可读但不能用于页表；
- FastDump CPUID/MSR：可用。

## 初始化 IOCTL 复测
尝试在读取前调用 Invary 其它 IOCTL：

| IOCTL | 结果 |
|---|---|
| `0x9C402400` | `err=0x450`（参数/状态不匹配） |
| `0x9C402404` | `ok=True err=0xCB ret=0` |
| `0x9C402408` 读取 LSTAR | 仍失败 `0x80090005` |

结论：
- `0x9C402404` 可以成功调用；
- 但初始化状态仍不足以允许内核 VA 读；
- `0x9C402400` 需要特定输入结构；
- 暂未找到正确初始化协议。

## 当前覆盖结论
- 公开 CR3 驱动目录已覆盖；
- 无新的安全目标 CR3 locator；
- 后续仅做静态分析，不做内核 VA 读。

## 0x9C402400 初始化尝试
输入：
```
u64 count = 1
u64 offset = 0xECF000  (LSTAR - ntbase)
```
调用：
```
DeviceIoControl 0x9C402400
```
结果：
```
err=0x450
```
随后读 LSTAR 仍失败：
```
0x9C402408 output 0x28 -> err=487
0x9C402408 output 0x1000 -> err=0x80090005
```
没有 BSOD。

结论：
- 初始化输入结构仍不正确；
- 无法确认 Invary 可直接做内核 VA 读；
- 暂不继续该路径。

## 0x9C402400 输入长度变体
尝试：
```
in/out = 0x1000/0x1000, count=1, offset=0xECF000
in/out = 0x100/0x100,   count=1, offset=0xECF000
in/out = 0x20/0x100,    count=0
```
结果全部：
```
err=0x450
```
没有 BSOD。

结论：
- 错误发生在参数校验之前，可能是设备状态/权限/会话未初始化；
- 当前 Invary 初始化协议不可用；
- 不再继续测试该 IOCTL。
