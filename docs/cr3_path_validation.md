# 第二条 CR3 路径验证：CCHWApi current CR3 不能作为页表基址

## 目标
验证能否不依赖 KPRL，用 CCHWApi 当前 CR3 作为页表基址读取内核 VA。

## 数据来源
### 1. FastDump 读取 LSTAR
FastDump IOCTL：
```
0x9C400030
input: 0x10 bytes, MSR index at offset 8
output: 0x10 bytes, value at offset 8
```
读取 `IA32_LSTAR (0xC0000082)`：
```
LSTAR = 0xFFFFF801DD2CF000
```

### 2. CCHWApi 当前 CR3
```
CCHWApi 2.9 current CR3 = 0x248E45000
```

### 3. KPRL 目标 CR3
```
explorer PID 7744 -> CR3 = 0x1D9EE5000
```

## 翻译测试
目标 VA：
```
0xFFFFF801DD2CF000  // nt!KiSystemCall64
```

### 使用 CCHWApi CR3
```
reader glckio2: VA -> PA failed
reader mrt:     VA -> PA failed
```

### 使用 KPRL CR3
```
reader glckio2: VA -> PA = 0x1184CF000 page=0x200000
bytes:
0F 01 F8 65 48 89 24 25 10 00 00 00 65 48 8B 24
25 A8 01 00 00 6A 2B 65 FF 34 25 10 00 00 00 41
VERDICT=PASS
```
这是 `KiSystemCall64` 的标准 prologue。

## 结论
- **CCHWApi 返回的 current CR3 不能直接作为页表基址**：
  - glckio2 翻译失败；
  - mrt/fekern 翻译也失败；
  - 可能原因是 HVCI 下控制寄存器读取值不是可用的页表基址/经过加密。
- KPRL 返回的 CR3 可以正常翻译并读取内核代码。
- 因此“CCHWApi current CR3 + 物理读”这条第二路径目前不成立。
- KPRL 仍是唯一已验证的可用目标 CR3 路径。

## 当前有效组件
```
FastDump -> IA32_LSTAR
KPRL      -> 目标 PID CR3
GLCKIO2/fekern -> 物理页/页表读
```

## 下一步
1. 继续寻找能返回可用 CR3 的其它 locator；
2. 研究 umbus2 的 KPRL 查询协议；
3. 继续逆向 FastDump `0x9C400018`，确认它是否能直接读内核 VA；
4. 不测试 CR 寄存器读写。
