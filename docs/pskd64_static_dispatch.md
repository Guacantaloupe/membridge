# PSKD64 IOCTL 0x220044 静态分发分析

## 已确认信息
LOLDrivers 记录：
```
IOCTL 0x220044
子操作 0x701E / 0x701F
能力: caller-selected virtual/physical memory read/write
```

## 静态分发结果
反汇编 `PSKD64.SYS` 后，`0x220044` 子操作分发如下：

### 0x701F
```
0x22775: xor r8d,r8d
0x22778: jmp 0x22705
0x22705: rdx = request
0x22708: call 0x228d4
```
`0x228d4` 检查：
```
[request+0x34] >= 0xC1
[request+0xA4] qword address
[request+0xAC] dword size
[request+0xB0] word flag
[request+0xC0] data buffer
```
之后调用 `0x2e458(address, size, 1)` 和 `0x4a4b0`。

### 0x701E
```
0x22717: r8d = 1
0x2271A: rdx = request
0x2271D: call 0x2374c
```
`0x2374c` 是更复杂的分支：
```
[request+0x34] >= 0xC1
[request+0x38] word
[request+0x3A] word
[request+0x3C] dword
[request+0x40] dword
[request+0x94] byte
```
其中包含 PCI/硬件访问逻辑，未确认可用于通用内核 VA 读写。

## 之前试读为什么失败
之前请求：
```
sub-op 0x701E / 0x701F
+0x34 = 0xC1
+0xA4 = LSTAR
+0xAC = 0x20
```
返回：
```
status=0
ret=0xC1
data 全零
```
说明：
- `+0x34` 不应设为 0xC1，可能是要求的最小输出长度；
- 或 `+0xB0` flag / `+0xC0` 偏移有误；
- 或 0x701E/0x701F 不是普通 VA 读；
- 盲试有写内核风险，已停止。

## 结论
- 已定位 0x701E/0x701F 的分发函数；
- 0x701F 的请求结构接近"地址/长度/数据"模型；
- 但字段含义仍未确认；
- 暂不继续动态试错；
- 无新 BSOD。
