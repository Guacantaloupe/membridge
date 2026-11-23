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
