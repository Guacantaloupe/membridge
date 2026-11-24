# WinSxS CR3 指令扫描

## 范围
```
C:\Windows\WinSxS
```
扫描 `.sys` 数量：**2003**

## 关键命中
| 文件 | CR3 指令数 | 说明 |
|---|---|---|
| `tcpip.sys` | 4 | 网络栈 |
| `ntfs.sys` | 3 | 文件系统 |
| `win32kfull.sys` | 2 | 内核图形 |
| `refs.sys` | 2 | ReFS |
| `dxgkrnl.sys` | 2 | 图形内核 |
| `cht4vx64.sys` | 2 | Chelsio 网卡 |
| **`KslD.sys`** | **2** | **Microsoft Defender 驱动，333216 bytes（vulnerable 版本）** |

其余命中包括 `win32kbase`、`storport`、`fltMgr`、`fvevol`、`http.sys`、`tcpip` 等系统驱动。

## 结论
- WinSxS 中确实存在 **333 KB 的 vulnerable KslD.sys**；
- 未发现新的用户态 CR3 locator；
- 没有新的安全目标 CR3 路径；
- 当前 KslD 服务仍指向 82 KB patched 版本，且访问门为 Defender 路径；
- 无 BSOD。

## 当前阻塞
KslD 的 SubCmd 2 理论上返回 CR3，但需要：
1. 修改 `AllowedProcessName`；
2. 或修改 `ImagePath` 回旧版；
3. 或重启 KslD 服务。

这些都会改动 Defender 配置，需要用户明确授权；因此暂不执行。
