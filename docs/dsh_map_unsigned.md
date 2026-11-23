# dsh_map 未签名驱动加载测试

## 结果
```
dsh_map.sys
sc start -> exit=577 (0x241)
STATE: STOPPED
```

## 设备
没有出现新的 `dsh_map` / `map` 设备。

## 原因
- `dsh_map.sys` 未签名；
- Windows 内核签名策略拒绝加载；
- 与驱动本身无关。

## 结论
- 未签名驱动不可用；
- 不修改测试签名/Boot 配置；
- 继续使用签名 Valid 的驱动候选。

## 当前状态
- 最新 dump：`112326-41453-01.dmp`
- 无新 BSOD
