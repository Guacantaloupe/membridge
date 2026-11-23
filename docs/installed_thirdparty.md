# 系统已安装第三方驱动导入（10 个）

## 来源
从：
```
C:\Windows\System32\drivers
```
筛选非 Microsoft、签名 Valid、项目中不存在的第三方驱动。

共发现 **84 个**第三方驱动，先导入 10 个到：
```
drivers/installed_thirdparty/
```

## 导入清单
| 驱动 | 厂商 | 大小 |
|---|---|---:|
| `RzCommon.sys` | Razer Inc | 64880 |
| `RzDev_0099.sys` | Razer Inc | 64680 |
| `VBoxSup.sys` | Oracle | 1083808 |
| `vmci.sys` | VMware | 104888 |
| `vmusb.sys` | VMware | 70656 |
| `pvscsii.sys` | VMware | 45488 |
| `AsusPTPFilter.sys` | ASUSTek | 115176 |
| `RTKVHD64.sys` | Realtek | 6333912 |
| `CH341S64.SYS` | wch.cn | 84864 |
| `rteth.sys` | Realtek | 86016 |

签名均 Valid。

## 特点
- 这些是**本机已安装、正在使用的第三方驱动**；
- 不等于它们可被再次作为独立服务加载；
- 部分为 PnP/功能驱动，用户态设备可能受 ACL 限制；
- 需要按设备名/服务分别评估。

## 当前累计
```
原有新增                  265
本轮新增                   10
新增合计                  275
```

## 下一步
1. 对 Razer / VMware / VBox 驱动做静态 IOCTL 分析；
2. 优先找有独立 `\Device\` 且可打开的设备；
3. 新候选仍必须用 CR3 页交叉验证。
