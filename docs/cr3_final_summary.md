# CR3 能力驱动最终汇总

## 目标
找到更多可以获取 CR3 的驱动程序。

## 已完成扫描
```
仓库 .sys 总数        691
可解析 x64 驱动       656
CR3 指令/导入候选      89
工作区语料扫描         2469 个 .sys
```

## 实际验证

### 可获取目标 PID 的 CR3
| 驱动 | 结果 |
|---|---|
| **KPRL** (`l1malwarebits.sys`) | `CR3=0x1CF6C8000`，PID 7592，PASS |

### 可获取当前 CPU 的 CR3
| 驱动 | 结果 |
|---|---|
| **CCHWApi 2.9** | `CR3=0x184472000`，PASS |
| **CCHWApiExt** | `CR3=0x1D746F000`，PASS |

### 可获取系统 CR3（候选）
| 驱动 | 结果 |
|---|---|
| **devhost.sys** | LOLDrivers 记录 DriverEntry 泄露 system CR3；样本被 Defender 拦截，未加载 |

### CR3 不兼容/不安全
| 驱动 | 结果 |
|---|---|
| **AsrCDDrv** | CR 访问 IOCTL 触发 `0x20001` HVCI 崩溃，禁止 |
| **Rtx_HalExt** | HAL 扩展 DLL，不能独立加载 |
| **l1malwarebits_big** | start=2 |
| **umbus2** | `\\.\KPRL` 可开，但 `0x239584` 返回零 |
| **FastDump** | 设备可开，CR3 协议未还原 |
| **Invary** | CR3 locator 失败 |

### 进程 VA 直读（非 CR3）
| 驱动 | 结果 |
|---|---|
| **KKYUM** | explorer 镜像基址 MZ 读取 PASS |

## 结论
- 目标 **PID CR3** 目前只有 **KPRL** 是已验证可用的；
- **CCHWApi / CCHWApiExt** 能获取当前 CPU 的 CR3；
- **devhost** 是新的系统 CR3 候选，但当前环境被 Defender 拦截；
- **AsrCDDrv** 的 CR 访问在 HVCI 下不安全，已排除；
- 仍没有第二个能安全按 PID 取目标 CR3 的驱动。

## 安全规则
1. 不再测试任何 CR 寄存器写入；
2. CR 读取也只在隔离/可回滚环境进行；
3. KPRL/CCHWApi 是当前唯一安全路径；
4. 不做全量物理扫描。
