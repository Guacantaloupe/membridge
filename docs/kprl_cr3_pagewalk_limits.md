# KPRL 目标 CR3 页表遍历失败记录

## 现象
KPRL 查询 explorer PID 7640：
```
CR3 = 0x1B815C000
```
用 GLCKIO2 物理读该 CR3 页：
```
membridge --backend glckio2_new --no-load --pa 0x1B815C000 --size 0x20
=> READ_OK
0000: 67 28 28 B8 01 00 00 0A 00 00 00 00 ...
```
页面本身可读。

但按 `VA=0xFFFFF801DD2CF000` 计算 PML4 索引后：
```
PML4E 对应的页表项为 0
```
后续 pagewalk 失败：
```
translate VA -> PA failed
```

## 解释
- 这表示该 CR3 的 PML4 中，目标内核 VA 的索引项为 0；
- 可能原因：
  1. KPRL 返回的 CR3 不是当前可用页表基址；
  2. 进程在两次查询间切换/退出；
  3. HVCI 下 CR3 值被处理，KPRL 查询得到的是非明文值；
  4. 目标进程页表不包含该内核映射。
- 目前不能据此证明 KPRL 不可用，但 **本次查询结果无法直接用于页表翻译**。

## 对比
- 之前 PID 7744 的 CR3 `0x1D9EE5000` 曾成功翻译 LSTAR；
- PID 4 的系统 CR3 `0x1AE000` 无法翻译；
- PID 7640 的 CR3 `0x1B815C000` 页面可读但 PML4E 为 0。

## 安全
- 不再做 KPRL 内核 VA 读；
- 不再做 CR 寄存器访问；
- 只保留 KPRL CR3 查询作为实验性路径。

## 结论
第二条“KPRL CR3 + 物理读 pagewalk”路径目前不稳定，需要进一步确认 CR3 的有效性/版本兼容性。
