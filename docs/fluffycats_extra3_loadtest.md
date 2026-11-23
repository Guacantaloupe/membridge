# AMD SPI Flash 驱动静态/加载测试

## 静态特征
`amdembspiflash.sys`：
- `IoCreateDevice`
- `MmMapIoSpace`
- 无可见 `\Device\...` 字符串

## 加载
```
STATE: RUNNING
```

## 设备枚举
加载后未出现新的：
```
AMD*
SPI*
Flash*
```
设备对象。

## 结论
- 驱动可加载；
- 未创建用户态可直接访问的设备；
- 更像 PnP/平台 SPI 控制器驱动；
- 不能作为任意物理读入口。

## 其它新候选静态特征
| 驱动 | 关键导入 |
|---|---|
| `aehd.sys` | IoCreateDevice / MmGetPhysicalAddress / MmProbeAndLockPages |
| `alert(1).sys` | IoCreateDevice / MmMapIoSpace / MmProbeAndLockPages |
| `AmdCrashDefender.sys` | IoCreateDevice / MmMapIoSpace / MmGetPhysicalAddress |
| `amdkfd.sys` | 文件未导入（大小超限或版本差异） |

这些需要进一步静态分析，暂不加载。
