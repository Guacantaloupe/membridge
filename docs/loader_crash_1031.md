# 变体驱动加载测试崩溃（2026-11-23 10:31）

## 崩溃
```
Dump: 112326-43546-01.dmp
BugCheck: 0x18 REFERENCE_BY_POINTER
P1: 0
P2: ffffa18de2b85e40
PROCESS: services.exe
STACK:
  nt!KeBugCheckEx
  nt!ObfDereferenceObject
  nt!IopUnloadDriver+0x412
  nt!NtUnloadDriver
FAILURE_BUCKET_ID: 0x18_OVER_DEREFERENCE_nt!IopUnloadDriver
```

## 触发点
在 `physmem_drivers_variants` 批量加载测试中：
- 前 11 个变体结果已写入 `evidence/driver_loadtest_variants.json`
- 第 12 行 `cpuz141.sys` 为 STOPPED（签名/吊销阻止）
- 崩溃发生在**下一轮驱动的 `sc stop/delete` 卸载路径**
- 具体是 `cpuz_x64.sys` 还是其后驱动，证据未写入，不能确认

## 结论
`0x18 IopUnloadDriver` 属于**驱动卸载引用计数错误**：
- 驱动在 `DriverUnload` / 设备删除时多解引用对象；
- `services.exe` 调用 `NtUnloadDriver` 时触发；
- 与物理内存读本身无关。

## 安全规则
1. 变体驱动不要“加载后立即 stop/delete”；
2. 要测试时：
   - 只 `sc create/start`；
   - 记录状态；
   - 不主动 `sc stop`；
   - 重启清场；
3. 或者每个变体单独一次启动，不做批量卸载；
4. 优先静态签名/字符串筛选，减少 live 加载。

## 已确认可加载的变体
- `AsrOmgDrv.sys`：RUNNING（证据第 7 行）

## 当前状态
- 最新 dump：`112326-43546-01.dmp`
- 之后未再启动变体加载测试
