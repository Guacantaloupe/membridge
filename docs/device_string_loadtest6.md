# ExpressNet 加载与读测试

## 结果
```
ExpressNet.sys
STATE: RUNNING
\\.\expressnet2274 -> OPEN_OK
```

## 物理读尝试
```
ExpressNet + mrt -> err=1
```
失败，无 BSOD。

## 结论
- 驱动可加载；
- 设备可打开；
- 不是 MRT 兼容物理读；
- 需要单独逆向其 IOCTL；
- 本轮没有新增可用读驱动。
