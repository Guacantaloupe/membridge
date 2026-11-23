# KPRL kernel-VA 读取导致 0x50 崩溃

## 目的
尝试用 `layers --kprl-read --pid 4 --va <kernel VA>` 读取内核内存，
以便通过 EPROCESS 链获取目标 CR3。

## 测试
FastDump 读到：
```
IA32_LSTAR = 0xFFFFF801DD2CF000
```
根据 dump 偏移推算 nt base：
```
0xFFFFF801DC400000
```
然后调用：
```
layers --kprl-read --pid 4 --va 0xFFFFF801DC400000 --size 0x40
```
并继续尝试 EPROCESS walk。

## 崩溃
```
Dump: 112426-42468-01.dmp
时间: 2026-11-24 07:05:17
BugCheck: 0x50
BUGCHECK_P1: 0xfffff801dc400000
PROCESS_NAME: System
```
`P1` 正是尝试读取的 nt base 地址。

结论：
- `KPRL --kprl-read --pid 4 --va <内核地址>` 对无效/未映射内核 VA 没有安全校验；
- 会直接触发 0x50 页错误；
- **KPRL 内核 VA 读取路径不安全，停止使用**；
- 该第二条 CR3 路径失败，不能继续用 KPRL 做 EPROCESS walk。

## 安全规则
1. 不再用 KPRL 读取未验证的内核 VA；
2. 不再做内核地址探测/扫描；
3. 只保留 KPRL 的 CR3-by-PID 查询；
4. PROCESS VA 读取只用已验证的目标用户 VA。
