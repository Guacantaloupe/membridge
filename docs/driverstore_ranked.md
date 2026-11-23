# DriverStore 物理 API 排名与 bsitf 复测

## 排名
对 `drivers/driverstore_thirdparty/` 100 个可解析驱动评分：

Top：
| 驱动 | 分数 |
|---|---:|
| `k57nd60a.sys` | 20 |
| `b57nd60a.sys` | 20 |
| `mlx5.sys` | 19 |
| `bsitf.sys` | 17 |
| `bcmpciedhd63.sys` | 15 |
| `qcwlan64.sys` | 15 |
| `athw8x/athwnx` | 15 |
| `BCMWL63AL/63a` | 15 |
| `amdacpksl.sys` | 14 |
| `cht4nx64.sys` | 14 |
| `bertreader.sys` | 5（仅 ZwOpenSection） |

## bsitf 复测
DriverStore 中的 `bsitf.sys`：
```
STATE: RUNNING
\\.\bsitf -> OPEN_FAIL err=5
```
结论：
- 驱动可加载；
- 设备 ACL 仍拒绝用户态打开；
- 不能作为用户态物理读入口。

## 判断
DriverStore 的物理 API 高分驱动主要是：
- 网卡/无线：k57/b57/mlx5/ath/BCM/rtl；
- 芯片组/音频：amdacpksl；
- bsitf：内核可加载但 ACL 锁定。

本轮没有新的用户态任意 PA 读驱动。
