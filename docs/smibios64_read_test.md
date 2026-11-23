# SMIBIOS64_W10：新的任意物理页读驱动（实测 PASS）

## 驱动
```
drivers/device_string_candidates/SMIBIOS64_W10.sys
```
来源：fluffycats device-string 候选。
签名 Valid。

## 设备
```
\Device\BIOS_SMI_DRIVER
\DosDevices\BIOS_SMI_DRIVER
\\.\BIOS_SMI_DRIVER -> OPEN_OK
```

## 物理读实测
加载：
```
sc create MbSmi binPath= ...\SMIBIOS64_W10.sys type= kernel start= demand
sc start MbSmi
=> RUNNING
```

使用现有 `MRT` 兼容 layout 单页读：
```
membridge --device \\.\BIOS_SMI_DRIVER --layout mrt \
  --pa 0x1000 --size 0x10 --dump
```
结果：
```
READ_OK bytes=0x10
0000: 78 56 34 12 ... 
VERDICT=PASS
```

## 已接入 backend
```
SMIBIOS64_W10.sys
  -> backend id: smibios
  -> layout: MRT
  -> device: \\.\BIOS_SMI_DRIVER
```

验证：
```
membridge --backend smibios --no-load --pa 0x1000 ...
VERDICT=PASS
```

## 结论
- 这是**第 5 个**新增的任意物理页读驱动；
- 与 `mrt` / `fekern` / `biostoolcommondriver` 使用兼容的物理读 IOCTL；
- 单页读通过，未触发 BSOD；
- 仍禁止批量扫描。

## 当前已验证新增物理读驱动
1. GLCKIO2
2. fekern
3. biostoolcommondriver
4. JYWinRing0（低内存）
5. **SMIBIOS64_W10**

## 当前状态
- 启动：11:49:24
- 最新 dump：`112326-41453-01.dmp`（11:29:00）
- 之后无新 BSOD

## 重要更正：SMIBIOS64 不是物理读驱动
后续用 KPRL 得到目标 CR3：

```
CR3=0x10704B000
```

用 SMIBIOS64 读取该 CR3 页：

```
READ_OK
0000: 78 56 34 12 50 01 00 00 ...
0010: 50 00 3D 00 43 00 3A 00 5C 00 55 00 73 00 65 00  ; P = C : \ U s e r
0020: 72 00 73 00 5C 00 6D 00 34 00 61 00 31 00 5F 00
```

返回内容是 **UTF-16 用户路径字符串**，不是 PML4E 页表数据。

结论：
- SMIBIOS64 的 IOCTL 没有按请求 PA 返回物理内存；
- 它返回的是驱动内部固定/系统信息；
- 之前的 PA=0x1000 “PASS” 是**假阳性**；
- smibios backend 已从 membridge 移除；
- SMIBIOS64 不再计入已验证物理读驱动。

## 正确计数
已验证的新增任意 PA 读驱动仍为 4 个：
1. GLCKIO2
2. fekern
3. biostoolcommondriver
4. JYWinRing0（低内存）

下一步必须用 CR3 页数据交叉验证 fekern / biostool，避免同类假阳性。
