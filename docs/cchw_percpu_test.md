# CCHWApi per-CPU CR3 测试

## 目的
验证 CCHWApi 是否能通过“指定 CPU 的 CR3”获取目标进程 CR3。

## 方法
- 创建 helper PowerShell 进程；
- 绑定到 CPU0（`ProcessorAffinity=1`）；
- 高优先级忙循环；
- 用 KPRL 查询 helper 的 CR3；
- 用 CCHWApi `0x80240020` 查询各 CPU 的 CR3。

## 结果
```
helper PID   = 2632
KPRL CR3     = 0x177A13000
```

CCHWApi：
```
cpu=0 cr3=0x1A9A19000
cpu=1 cr3=0x1A9A19000
cpu=2 cr3=0x1A9A19000
cpu=3 cr3=0x1A9A19000
cpu=4 cr3=0x1A9A19000
cpu=5 cr3=0x1A9A19000
cpu=6 cr3=0x1A9A19000
cpu=7 cr3=0x1A9A19000
```

## 结论
- CCHWApi 对所有 CPU 返回同一 CR3；
- 该值与 helper 的 KPRL CR3 不同；
- 说明 CCHWApi 的 control-register 读取并不能按 CPU 获取目标进程 CR3；
- 不能作为“目标进程 CR3”新路径；
- 该方法无 BSOD。

## 当前结论
- 目标 CR3 仍只能由 KPRL CR3-by-PID 获取；
- CCHWApi current CR3 只适用于当前进程/CPU 上下文，不能指定目标；
- 继续静态寻找新 locator。
