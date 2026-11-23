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

## FastDump x64 候选
文件：
```
drivers/fastdumpx64/fastdumpx64.sys
```
加载：
```
STATE: RUNNING
```
设备：
```
\Device\FastDump
\??\FastDump
\\.\FastDump -> OPEN_OK
```
关键导入/字符串：
```
MmGetPhysicalAddress
PsInitialSystemProcess
\Device\PhysicalMemory
IoCreateDeviceSecure
IoValidateDeviceIoControlAccess
```
IOCTL 常量：
```
0x9C400000 0x9C400008 0x9C400010 0x9C400014
0x9C400018 0x9C40001C 0x9C40002C 0x9C400030
0x9C400034
0x9C40000C 0x9C400020 0x9C400024 0x9C400028
```
静态 CR3 读指令：2 次。

结论：
- 设备可打开；
- 具备物理内存/`PsInitialSystemProcess`/CR3 访问特征；
- 但 IOCTL 协议未还原；
- 暂未验证“用户态可获取目标 CR3”。

## 本轮 CR3 结论更新
### 可获取 CR3
1. KPRL — 目标 PID 的 CR3；
2. CCHWApi — 当前 CPU 的 CR3。

### 新增可用
- KKYUM — 直接读任意 EXE VA（MZ 验证 PASS）。

### 候选
- FastDump — 设备可开，有待逆向；
- Invary — 设备可开，locator 失败；
- l1malwarebits_big / Rtx_HalExt — start=2。

## 追加验证：CCHWApiExt
加载：
```
drivers/CCHWApiExt(1)_sys/CCHWApiExt(1).sys
STATE: RUNNING
```
设备：
```
\Device\CCHWAPIExt
\Device\CCHWAPI
\\.\CCHWAPI -> OPEN_OK
```
当前 CPU CR3：
```
layers --cchwapi-cr3-current --device \\.\CCHWAPI
=> CR3=0x1D746F000
VERDICT=PASS
```
`\\.\CCHWAPIExt` 作为 CR3 设备失败：
```
CCHWApi ReadControlReg(CR3) failed on all CPUs
```
结论：
- CCHWApiExt 是**新增的当前 CPU CR3 提供者**；
- 仍然不能指定 PID；
- 目标 PID CR3 目前只有 KPRL。

## 目标 CR3 提供者汇总
| 驱动 | 目标 PID CR3 | 当前 CPU CR3 | 进程 VA 直读 |
|---|---|---|---|
| KPRL | ✅ | — | — |
| CCHWApi | — | ✅ | — |
| CCHWApiExt | — | ✅ | — |
| KKYUM | ❌ locator失败 | — | ✅ |
| Invary | ❌ | — | — |

## umbus2 KPRL 变体协议测试
设备：
```
\Device\KPRL
\DosDevices\KPRL
\\.\KPRL -> OPEN_OK
```
静态：
```
PsLookupProcessByProcessId
MmCopyMemory
PsGetProcessSectionBaseAddress
ObReferenceObjectByName
ZwQuerySystemInformation
```
说明具备进程/物理内存访问代码路径。

### IOCTL 0x239584
使用 KPRL 查询结构：
```
input  0x48 bytes，PID at offset 0
output 0x48 bytes，expected CR3 at 0x38
```
实测：
```
DeviceIoControl -> True
ret=0
output 全零
```
尝试 PID offset 0/4/8/0x20/0x30：均返回成功但输出全零。

### WriteFile / ReadFile
```
WriteFile(8字节 PID) -> False err=1
ReadFile(16字节)       -> False err=1
```
设备不支持简单读写接口。

## 结论
- umbus2 确实创建 `\\.\KPRL`；
- IOCTL 0x239584 能进入驱动，但不返回 CR3；
- 它不是当前 KPRL 查询协议的兼容实现；
- 仍需单独逆向其真正 IOCTL/请求格式；
- 目标 PID CR3 仍只能由原版 KPRL 提供。

## CCHWApi 2.12 / CCHWApiExt 复测
### CCHWApi 2.12
尝试加载：
```
drivers/CCHWApi(1)_sys/CCHWApi(1).sys
sc start -> 183 (device/service duplicate)
```
当前设备已被 CCHWApiExt 占用，无法单独验证 2.12 版本。

### CCHWApiExt
```
STATE: RUNNING
\\.\CCHWAPI -> OPEN_OK
current CR3 = 0x1D746F000
```
仍只有当前 CPU CR3。

### CCHWApi-only 目标 CR3 尝试
```
layers --locate --cchwapi-only --pid 7592
=> ntoskrnl base not found
```
结论：
- CCHWApi 系驱动只能读取当前 CPU 控制寄存器 CR3；
- 不能直接按 PID 获取目标 CR3；
- 目标 CR3 仍需 KPRL。

## Rtx_HalExt 加载失败原因
PE 检查：
```
Subsystem = NT native
AddressOfEntryPoint = 0xff000
Export Directory = 0xfe000
Characteristics = 0x2022
```
结论：
- 它是带导出表的 **HAL 扩展/DLL 型镜像**；
- 不是独立 DriverEntry 驱动；
- 不能直接 `sc create/start`；
- 因此不作为 CR3 提供者。

## l1malwarebits_big 启动错误
```
sc start -> FAILED 2: The system cannot find the file specified.
```
- 路径存在，签名 Valid；
- 说明是 DriverEntry/加载阶段返回错误，不是路径不存在；
- 暂无法作为 CR3 提供者。

## 当前 CR3 能力再次确认
- CCHWApi / CCHWApiExt：只能当前 CPU CR3；
- umbus2：KPRL 设备可开，0x239584 返回零；
- FastDump：设备可开，协议未还原；
- Rtx_HalExt：HAL 扩展 DLL，不能独立加载；
- l1malwarebits_big：start=2；
- 目标 PID CR3 仍只有 KPRL。
