# device_candidates 加载测试

## 结果
| 驱动 | 状态 |
|---|---|
| `windrvr1251.sys` | RUNNING |
| `Pce9852x64(3).sys` | STOPPED（exit=193） |
| `EPIXXCW6(2).SYS` | RUNNING |
| `PSKMAD_64.sys` | RUNNING |
| `TcRteFilter.sys` | STOPPED（exit=31） |

3/5 RUNNING，无 BSOD。

## 设备枚举
加载后没有出现明显的新 DOS 设备：
```
WINDRVR1251
EPIX...
PSKMAD...
TcRteFilter...
```
均未匹配到。

说明：
- 这些更可能是 PnP/过滤/反作弊驱动；
- 或设备名不是简单 ASCII；
- 不能只靠“能加载”判断可物理读。

## 结论
本轮：
- 3 个可加载；
- 未新增可用物理读驱动；
- 无新 BSOD。

已验证的新增物理读驱动仍为：
1. GLCKIO2
2. fekern
3. biostoolcommondriver
4. JYWinRing0
