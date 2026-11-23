# GLCKIO2：新增可用物理读驱动（实测 PASS）

## 驱动
```
drivers/namazso_extra/GLCKIO2_3a5ec83fe670e5e23aef3afa0a7241053f5b6be5e6ca01766d6b5f9177183c25.sys
```
签名 Valid。

## 加载
```
sc create MbGLCKIO2 binPath= ... type= kernel start= demand
sc start MbGLCKIO2
```
结果：
```
STATE: RUNNING
WIN32_EXIT_CODE: 0
```
未主动 stop/unload。

## 设备
对象枚举：
```
\Device\GLCKIo2
```
用户态打开：
```
\\.\GLCKIo2 -> OPEN_OK
```

## 物理读实测
命令：
```
membridge --device \\.\GLCKIo2 --layout glckio2 --pa 0x1000 --size 0x10 --dump
```
结果：
```
READ_OK bytes=0x10
0000: E9 4D 06 00 01 00 00 00 01 00 00 00 3F 00 18 10
VERDICT=PASS
```

这是本轮**第一个新增的、真实可用的物理读驱动**：
- 可加载；
- 设备可打开；
- `MapPhysicalMemory` IOCTL 可读物理页；
- 无 BSOD。

## 下一步
1. 读当前 CR3 页验证；
2. 接入 `membridge` backend id（如 `glckio2_new`）；
3. 测试更大范围/单页扫描；
4. 继续测试 AsUpIO。
