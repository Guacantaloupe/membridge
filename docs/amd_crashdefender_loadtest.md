# AmdCrashDefender 加载测试

## 结果
```
AmdCrashDefender.sys
STATE: RUNNING
```

## 设备枚举
没有出现：
```
\Device\AmdCrashDefender
\Device\CrashDefender
```
之类的新设备。

## 结论
- 驱动可加载；
- 没有创建用户态可直接访问的设备；
- 不能作为物理读入口；
- 无 BSOD。

## 当前状态
- 最新 dump：`112326-41453-01.dmp`
- 无新崩溃
