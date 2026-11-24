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

## 关键修正：0x701E 是读，0x701F 是写
对 `0x228d4` 的尾部静态分析：
```
0x2e458(address, size, 1)   // 地址/长度校验
0x4a4b0(dest, src, size)    // memcpy
```
`0x4a4b0` 汇编确认是 **memcpy：rcx=dest, rdx=src, r8=size**。

`0x228d4` 的调用参数：
```
r8d = 0 -> rcx = request+0xA4 (address), rdx = request+0xC0 (data)
           => memcpy(address, data, size)   // 写
r8d = 1 -> rcx = request+0xC0 (data), rdx = request+0xA4 (address)
           => memcpy(data, address, size)   // 读
```
分发关系：
```
0x701F -> r8d=0 -> 写
0x701E -> r8d=1 -> 读
```

## 安全事件记录
之前测试同时调用了：
```
0x701E
0x701F
```
其中 0x701F 实际是**写路径**，请求中 data 为零。理论上可能向 LSTAR 写入零。
本次没有观察到新 BSOD，系统仍可用，但这是高风险操作。

**立即规则：**
1. 禁止再调用 PSKD64 0x701F；
2. 禁止任何 PSKD64 写操作；
3. PSKD64 只允许在完整协议确认后做只读测试；
4. 当前不继续动态测试 PSKD64。

## 结论
- 读/写方向已确认；
- 0x701E 是读子操作；
- 0x701F 是写子操作；
- 之前的数据为零是读失败或请求结构问题；
- 后续只做静态分析或只读、已知有效地址测试。

## 0x701E 只读测试
按已确认方向，只调用读子操作：
```
IOCTL 0x220044
sub-op 0x701E   // read
request length   0xE0
+0x2C = 0x701E
+0x34 = 0xC1
+0xA4 = explorer ImageBase 0x7FF72CF20000
+0xAC = 0x20
+0xC0 = data
```
结果：
```
ok=True
ret=0xC1
status=0
data 全零
```
系统未崩溃。

解释：
- 0x701E 方向是读，但当前字段布局没有让驱动返回数据；
- 请求结构仍不正确；
- 没有调用 0x701F（写）；
- 不再动态试错。

## 后续
- 只做静态逆向；
- 若确认完整结构，先用只读、已知有效地址验证；
- 绝不调用 0x701F。

## 0x701E 只读复测
- 请求长度 0xE0；
- +0x28 状态在输入/输出均为 0；
- 数据区仍全零；
- 未调用 0x701F；
- 无 BSOD。

判断：
- 状态 0 但无数据，说明 `rdx` 很可能不是普通 IOCTL 系统缓冲，
  而是 IRP/自定义请求结构；
- 继续猜字段风险过高；
- 已停止 PSKD64 动态测试，并停止驱动服务。

## PCIScope 用户态安装包协议提取
下载：
```
https://www.tssc.de/site/download/prods/pciscope.exe
```
大小：
```
12,710,312 bytes
```
搜索：
```
220044
701E / 701F
PSKD64
DeviceIoControl
IOCTL
```
结果：
```
全部 0 命中
```
二进制原始常量搜索：
```
44 00 22 00
1E 70 00 00
1F 70 00 00
```
也全部 0 命中。
说明安装包被压缩/打包，无法直接提取协议；未执行安装，避免引入未知软件。
