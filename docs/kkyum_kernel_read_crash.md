# KKYUM 内核 VA 读取导致 0x50 崩溃

## 测试
尝试使用 KKYUM 的隐藏内核读选项：
```
layers --kkyum-kread --va 0xFFFFF801DD2CF000 --size 0x20
```
目标 VA：
```
0xFFFFF801DD2CF000  // nt!KiSystemCall64 (LSTAR)
```

## 崩溃
```
Dump: 112426-42125-01.dmp
时间: 2026-11-24 07:18:57
BugCheck: 0x50
BUGCHECK_P1: 0xfffff801dd2cf000
```
`P1` 正是尝试读取的 LSTAR 地址。

## 结论
- KKYUM 的 `--kkyum-kread` 对内核 VA 没有安全校验；
- 无效/未映射内核 VA 会触发 0x50；
- **KKYUM 内核 VA 读取路径不安全，禁止使用**；
- 这与之前 KPRL 内核 VA 读取崩溃的结论一致。

## 安全规则更新
1. 禁止使用任何驱动读取未验证的内核 VA；
2. 禁止内核地址扫描/探测；
3. KKYUM 仅用于已验证的用户态进程 VA 直读；
4. 不再对 `--kkyum-kread`、`--kprl-read --pid 4` 做测试。
