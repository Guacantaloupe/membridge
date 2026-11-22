# HVCI/VBS 下 11 个物理读驱动读取 CR3 页结果

## 结论
在 HVCI/VBS 开启的真实机器上，11 个物理读驱动**无法可靠读取实际 CR3 页/页表**：

- 多数驱动对 CR3 物理页返回 `err=31` 或 `err=8`；
- 部分驱动直接 BSOD（`0x4E` / `0x3B`）；
- 少数返回 PASS 的读数是**旧 CR3 页或非实际页表内容**（不是 PML4E）。

## 驱动矩阵

| 驱动 | 对当前 CR3 页结果 | 说明 |
|---|---|---|
| cchwapi_2_9 | FAIL `err=8` | 旧 CR3 页曾 PASS，但当前 CR3 页失败 |
| cchwapi_2_12 | 旧页 PASS | 未在当前 CR3 页复测 |
| cchwapi_ext_4_5 | 旧页 PASS | 未在当前 CR3 页复测 |
| hp_swtools | **BSOD 0x4E** | PFN_LIST_CORRUPT subtype 7 |
| swtoolsdriver | 未测 | 同 HP MapPhysicalMemory 族，高风险 |
| hp_swtools_v1 | 未测 | 同 HP 族，高风险 |
| isharepp | **BSOD 0x3B** | iSharePp+0x80DD 空指针 |
| gibepext | FAIL `err=31` | 当前 CR3 页不可读 |
| gibepext2 | 未测 | 同 gibepext 族 |
| tdeio | **BSOD 0x4E** | PFN_LIST_CORRUPT subtype 7 |
| ufu_win10 | 返回 PASS 但内容非页表 | 32-bit PA、数据像用户 VA，不是 PML4E |

## 关键 BSOD
- `112326-43265-01.dmp`：0x3B，iSharePp.sys
- `112326-43468-01.dmp`：0x4E subtype 7，HP swtools
- `112326-43656-01.dmp`：0x4E subtype 7，tdeio

## 原因
VBS/HVCI 保护页表物理页，普通物理读驱动无法读取；部分驱动自身在失败路径上存在 bug，直接 BSOD。

## 建议
1. 在 VM 中关闭 HVCI/VBS 后复测，才能区分“驱动能力”与“HVCI 保护”；
2. 在真机上不要继续让 HP/tdeio/iSharePp 读取 CR3 页；
3. 若必须在 HVCI 开启下工作，改用非物理 CR3 路径（内核 VA）或关闭 HVCI 后验证。
