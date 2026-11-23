# DSH 自定义驱动加载结果

## 测试
| 驱动 | start |
|---|---|
| `dsh_ntbase_diag.sys` | exit=577（0x241） |
| `dsh_probe_s0.sys` | exit=577（0x241） |

状态均为：
```
STOPPED
```

## 原因
- 两个驱动均未通过内核签名策略；
- 即使 Authenticode 显示 Valid，也可能缺少 Microsoft 内核信任链/目录签名；
- 不能直接作为 BYOVD 加载。

## 结论
- 项目自带的 DSH 小型驱动在当前系统不可加载；
- 不修改测试签名/Boot 配置；
- 继续依赖签名 Valid 且可被内核接受的驱动。

## 当前状态
- 最新 dump：`112326-41453-01.dmp`
- 无新 BSOD
