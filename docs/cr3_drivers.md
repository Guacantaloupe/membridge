# CR3 获取能力扫描与验证

## 目标
寻找“能获取 CR3”的驱动程序。

## 静态扫描
新增：
```
scripts/scan_cr3_drivers.py
```
扫描 691 个 `.sys`：
- `mov r64, cr3`（0F 20 /r）
- `mov cr3, r64`（0F 22 /r）
- `PsGetProcessPcb`
- `KeStackAttachProcess + PsLookupProcessByProcessId`
- `MmCopyVirtualMemory`

结果：
```
CR3 候选 89 个
```

Top：
| 驱动 | CR3 读指令 | CR3 写指令 | 关键导入 |
|---|---:|---:|---|
| `l1malwarebits_big.sys` | 5 | 4 | KeStackAttachProcess / MmCopyVirtualMemory |
| `Rtx_HalExt.sys` | 3 | 4 | — |
| `vmx86.sys` | 3 | 2 | — |
| `ctrl_pawnio.sys` | 2 | 2 | — |
| `dumpit64.sys` | 2 | 1 | KeStackAttachProcess |
| `DumpIt.sys` | 2 | 1 | KeStackAttachProcess |
| `CCHWApi.sys` | 1 | 1 | — |
| `AsrDrv2.sys` | 1 | 1 | — |
| `kprl/l1malwarebits.sys` | 1 | 0 | KeStackAttachProcess / MmCopyVirtualMemory |
| `fastdumpx64.sys` | 2 | 0 | — |

## 实测结果

### KPRL（可获取目标 PID 的 CR3）
```
layers --locate --kprl --pid 7592
=> CR3=0x1CF6C8000
```
状态：可用。

### CCHWApi（只能获取当前 CPU 的 CR3）
```
layers --cchwapi-cr3-current
=> current CR3=0x184472000
```
状态：可用，但只限当前 CPU，不能指定 PID。

### KKYUM
设备：
```
\\.\KKYUM -> OPEN_OK
```
CR3 定位：
```
layers --locate --kkyum --pid 7592
=> device/ntoskrnl/EPROCESS walk error
```
但直接进程读可用：
```
layers --read-process --pid 7592 --va 0x7FF6D3B80000 --size 0x40
=> 4D 5A 90 00 ...  > MZ
VERDICT=PASS
```
结论：
- KKYUM 是**新的 EXE 任意 VA 读驱动**；
- 它的 CR3-by-PID 定位不可用（EPROCESS 偏移/版本不匹配）。

### Invary Sensor
设备：
```
\\.\invarysensor -> OPEN_OK
```
CR3 定位：
```
layers --locate --invary --pid 7592
=> device/ntoskrnl/EPROCESS walk error
```
暂无 CR3 能力。

### l1malwarebits_big
```
sc start -> exit=2
```
不可启动；虽然静态有 5 次 CR3 读、4 次 CR3 写，但无法加载。

### Rtx_HalExt
```
sc start -> exit=2
```
不可启动。

## 当前结论
### 真正可获取 CR3 的驱动
1. **KPRL** — 按 PID 取目标 CR3；
2. **CCHWApi** — 当前 CPU CR3。

### 新增可用能力
- **KKYUM** — 直接读任意 EXE 用户态 VA（已读到 explorer 的 MZ）。

### 候选但当前不可用
- Invary Sensor：设备可开，locator 失败；
- l1malwarebits_big / Rtx_HalExt / dumpit / fastdumpx64：静态有 CR3 指令，尚未验证用户态可用。

## 下一步
1. 测试 `dumpit` / `fastdumpx64` 是否能加载并暴露 CR3；
2. 修正 KKYUM/Invary 的 EPROCESS walk（offset/构建匹配）；
3. 对 `l1malwarebits_big` 检查 DriverEntry 失败原因；
4. 继续扫描 CR3 读写指令 + 设备创建组合。
