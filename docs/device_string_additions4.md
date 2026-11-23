# device_string 候选继续扩充（第 4 批）

## 新增 20 个
### DAQ 系列（10）
```
Daq2214(3/4)
Daq2501(2/5/6)
Daq2205(3/5)
Daq2206(4)
Daq2006(5)
Daq2016(6)
```

### 非 DAQ（10）
```
DLFdeEd.sys / (1) / (2) / (3) / (4) / (5) / (6)   \Device\pded
C__Program Files (x86)_BenchMate 10_bin_bmrbt64.sys \Device\bmrbt-0.36
niserialDriver.sys                                 \DosDevices\Global
MOTUProAudio.sys                                   \DosDevices\Global
```

全部签名 Valid。

## 说明
- `DLFdeEd` 系列带 `\Device\pded`，属于磁盘/加密过滤类，**暂不加载**，避免重演 SDDisk2k 挂起；
- `niserialDriver` / `MOTUProAudio` 是串口/音频设备；
- 这些候选主要用于扩充库，不一定能成为物理读驱动。

## 当前累计
```
原有新增                  245
本轮新增                   20
新增合计                  265
```

device_string 候选总数：**50**。

## 已验证可用的新增读驱动
1. GLCKIO2
2. fekern_byov
3. JYWinRing0（低内存）

SMIBIOS64 / biostool 为假阳性，已排除。
