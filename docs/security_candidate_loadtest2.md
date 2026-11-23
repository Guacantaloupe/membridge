# security_candidates 第二批加载测试

## 加载
| 驱动 | 状态 |
|---|---|
| `PSKD64.SYS` | RUNNING |
| `NxpCap64.sys` | STOPPED（exit=193） |
| `RDWM1179.SYS` | RUNNING |
| `RDWM1236.SYS` | RUNNING |
| `IntcOED.sys` | RUNNING |
| `stwrt64.sys` | RUNNING |

5/6 RUNNING，无 BSOD。

## 设备
仅发现：
```
\Device\PSKD64
```
其余为 PnP/音频接口，不提供直接 DOS 设备。

## 结论
- 这些驱动多为 PnP/音频/网络类，不能直接作为物理读；
- `PSKD64` 设备可枚举，但更像键盘/输入过滤驱动；
- 本轮无新任意物理读驱动；
- 已确认的任意物理读新增驱动仍为：
  - GLCKIO2
  - fekern
  - biostoolcommondriver

## 当前状态
- 最新 dump：`112326-44078-01.dmp`
- 无新 BSOD
