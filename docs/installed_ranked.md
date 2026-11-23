# installed_thirdparty 物理 API 排名

## 方法
用：
```
scripts/rank_driver_imports.py
```
对 `drivers/installed_thirdparty/` 的 84 个驱动按以下导入评分：

```
MmMapIoSpace
MmGetPhysicalAddress
MmMapLockedPages
MmMapLockedPagesSpecifyCache
MmProbeAndLockPages
MmAllocateContiguousMemory
```

## 结果
```
scored=31
```

Top：
| 驱动 | 分数 | 说明 |
|---|---:|---|
| `VBoxSup.sys` | 20 | VirtualBox 内核支持 |
| `iaStorV.sys` | 19 | Intel 存储 |
| `VBoxUSB.sys` | 16 | VirtualBox USB |
| `iaLPSSi_I2C.sys` | 15 | Intel I2C |
| `nvraid.sys` | 14 | NVIDIA RAID |
| `mlx4_bus.sys` | 14 | Mellanox RDMA |
| `RTKVHD64.sys` | 14 | Realtek HD Audio |
| `VBoxNetAdp6/Lwf` | 12 | VirtualBox 网络 |
| `iaStorAVC/VD` | 10 | Intel 存储 |
| `wintun.sys` | 9 | WireGuard 隧道 |
| `vmci.sys` | 9 | VMware |
| `RtsUer.sys` | 9 | Realtek USB |

## 判断
这些驱动虽然使用物理内存 API，但多数是：
- 为 DMA/总线主控服务；
- 不向用户态提供“任意 PA 读”IOCTL；
- 设备对象多受 PnP/ACL 限制。

本轮没有发现新的任意物理读驱动。

## 下一步
1. 对 `VBoxSup` / `VBoxUSB` / `mlx4_bus` 做设备名/IOCTL 静态检查；
2. 不直接加载 VBox/存储类未知驱动；
3. 继续从其它语料找 BYOVD 型驱动。
