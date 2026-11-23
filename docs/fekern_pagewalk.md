# fekern 完整页表翻译到 cmd.exe MZ（已验证）

## 目标
```
PID=11232
CR3=0x10704B000
ImageBase=0x7FF797980000
```

## 页表遍历
使用 `fekern_byov` backend 逐级读物理页：

```
PML4E = 0xA0000024DD57867
PDPTE = 0xA0000024DD58867
PDE   = 0xA0000024DD59867
PTE   = 0x83000001FCA2F025
PA    = 0x1FCA2F000
```

结论：
- fekern 返回真实页表项；
- VA→PA 翻译成功；
- 目标 PA 可读。

## 目标数据
```
HEX=4D5A90000300000004000000FFFF0000
```

与 cmd.exe 用户态基线一致：
```
MZ 90 00 03 00 00 00 04 00 ...
```

**MZ_HEADER=PASS。**

## 结论
`fekern_byov` 已经是完整可用的物理页表读驱动：

- 可加载；
- 设备 `\\.\fekern_00` 可打开；
- 可读 CR3 页；
- 可读 PML4/PDPT/PD/PT；
- 可完成 VA→PA；
- 可读目标数据页；
- MZ 与基线一致。

## 当前新增读驱动
1. **GLCKIO2**：完整页表翻译 PASS；
2. **fekern_byov**：完整页表翻译 PASS；
3. **JYWinRing0**：低内存 PASS；
4. `SMIBIOS64` / `biostool`：假阳性，已排除。

## 当前状态
- 启动：11:49:24
- 最新 dump：`112326-41453-01.dmp`（11:29:00）
- 之后无新 BSOD
