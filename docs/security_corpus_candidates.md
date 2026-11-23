# security/deliverables 语料新增候选：biostool 物理读 PASS

## 来源
从 `driver-security` 和 `deliverables` 中按物理内存 API 导入函数排名筛选，新增 8 个候选：

```
drivers/security_candidates/
```

| 驱动 | 大小 | 结果 |
|---|---:|---|
| `bsitf_unlocked.sys` | 36728 | load STOPPED（exit=577） |
| `biostoolcommondriver.sys` | 48352 | **RUNNING + 物理读 PASS** |
| `PSKD64.SYS` | 466344 | 未测 |
| `NxpCap64.sys` | 1728672 | 未测 |
| `RDWM1179.SYS` | 397336 | 未测 |
| `RDWM1236.SYS` | 398896 | 未测 |
| `IntcOED.sys` | 1563888 | 未测 |
| `stwrt64.sys` | 561680 | 未测 |

签名均 Valid。

## biostoolcommondriver 实测
加载：
```
STATE: RUNNING
```
设备：
```
\Device\BiosToolCommonDriver
\\.\BiosToolCommonDriver -> OPEN_OK
```
物理读：
```
membridge --device \\.\BiosToolCommonDriver --layout mrt \
  --pa 0x1000 --size 0x10 --dump
```
结果：
```
READ_OK bytes=0x10
VERDICT=PASS
```

已接入 backend：
```
membridge --backend biostool_common --no-load --pa 0x1000 ...
```
同样 `VERDICT=PASS`。

## 结论
- `biostoolcommondriver.sys` 是**新的可用任意 PA 单页读驱动**；
- 使用 `MRT_IOCTL_READ_PHYSICALMEMORY` 兼容路径；
- `bsitf_unlocked` 无法加载（exit=577）；
- 其余待测。

## 当前可用新增驱动
1. GLCKIO2（页表/单页）
2. fekern（任意 PA 单页）
3. biostoolcommondriver（任意 PA 单页）
4. JYWinRing0（低内存）

## 安全
- 只做单页读；
- 未批量扫描；
- 最新 dump 仍为 `112326-44078-01.dmp`。
