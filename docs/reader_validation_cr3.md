# 新增读驱动 CR3 页交叉验证

## 方法
用 KPRL 得到 cmd.exe CR3：

```
CR3=0x10704B000
```

分别用以下设备读取 CR3 物理页：

- fekern：`\\.\fekern_00`
- biostool：`\\.\BiosToolCommonDriver`

## fekern：真实 PASS
```
READ_OK bytes=0x30
0000: 67 48 C6 0E 02 00 00 0A 00 00 00 00 00 00 00 00
0010: 00 00 00 00 00 00 00 00 67 E8 D8 46 02 00 00 0A
0020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

符合 PML4E 结构：
- 低 12 位 present/RW/US 位；
- 高 PFN 位；
- 非字符串。

结论：**fekern 是真物理读驱动，保留。**

## biostool：假阳性
```
READ_OK bytes=0x30
0000: 50 01 30 E3 8B 01 00 00 ...
0010: 44 00 61 00 74 00 61 00  ; "D a t a"
0020: 50 00 72 00 6F 00 67 ...
```

返回 UTF-16：
```
Data\ProgramFile...
```

不是 PML4E。

结论：
- `biostoolcommondriver` **不按请求 PA 返回物理内存**；
- 之前 `PA=0x1000` PASS 是假阳性；
- `biostool_common` backend 已从 membridge 移除；
- 不再计入可用物理读驱动。

## 当前真实可用新增读驱动
1. **GLCKIO2**：页表读 + 翻译，已验证；
2. **fekern**：CR3 页返回合法 PML4E，已验证；
3. **JYWinRing0**：低内存读，已知 WinRing0 语义；
4. `biostool` / `SMIBIOS64`：假阳性，已排除。
