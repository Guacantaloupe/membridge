# GLCKIO2 完整页表翻译成功（新驱动实测 PASS）

## 结果
用 KPRL 取目标 cmd.exe CR3：

```
CR3=0x1548FD000
ImageBase=0x7FF6FBD00000
```

用 **GLCKIO2** 逐级读页表：

```
PML4E = 0xA00000155B09867
PDPTE = 0xA00000109A0A867
PDE   = 0xA0000002E70B867
PTE   = 0x81000001080E8005
PA    = 0x1080E8000
```

读取该物理页：

```
HEX=4D5A90000300000004000000FFFF0000
```

即：

```
MZ 90 00 03 00 00 00 04 00 00 00 FF FF 00 00
```

**与 cmd.exe ImageBase 的 MZ 头完全一致。**

## 结论
GLCKIO2 已经验证为完整的物理页表读取驱动：

- 能读当前/目标 CR3 页；
- 能读 PML4E / PDPTE / PDE / PTE；
- 能完成 VA→PA 翻译；
- 能读目标数据页；
- 全过程没有 BSOD。

这是本轮找到的**第一个可端到端工作的新增物理读驱动**。

## 可复现脚本
```
scripts/pagewalk_glckio2.ps1
```

用法：
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\pagewalk_glckio2.ps1 `
  -Cr3 0x1548FD000 -Va 0x7FF6FBD00000 -Size 0x40
```

## 限制
- 当前 CR3 仍由 KPRL 获取；
- 下一步需要改成物理 EPROCESS 扫描或其它 11 驱动路径；
- GLCKIO2 驱动是新增样本，需要接入 `membridge` 正式 backend。

## 下一步
1. 把 GLCKIO2 增加为 `membridge` backend（如 `glckio2_new`）；
2. 用 `membridge --locate` 物理扫描获取 CR3，摆脱 KPRL；
3. 实现完整的“只用物理驱动读任意 EXE 内存”；
4. 继续找更多驱动。
