# DriverStore CR3 指令扫描

## 范围
```
C:\Windows\System32\DriverStore\FileRepository
```
扫描 `.sys` 数量：**525**

## 命中含 CR3 读指令的驱动
| 文件 | CR3 指令数 | 大小 | 类型 |
|---|---|---|---|
| `igdkmdn64.sys` (iigd_dch) | 8 | 52.5 MB | Intel 核显驱动 |
| `RTKVHD64.sys` (hdxasus) | 2 | 6.5 MB | Realtek 音频 |
| `cht4vx64.sys` | 2 | 1.85 MB | Chelsio 网卡 |
| `Netwtw10.sys` | 1 | 5.5 MB | Intel Wi-Fi |
| `igdkmdn64.sys` (另一版本) | 1 | 51.6 MB | Intel 核显 |

## 结论
- 这些 CR3 指令主要来自 GPU/音频/网卡驱动的页表/DMA 上下文；
- 没有发现新的用户态设备接口可返回 CR3；
- 没有发现新的 CR3 locator；
- 无 BSOD。

## 当前已确认的新候选
1. `KslD.sys`：Microsoft 签名，SubCmd 2 理论上 CR3；被 `AllowedProcessName` 限制；
2. `PSKD64.SYS`：已知任意内核 R/W，读协议未解开；禁止写路径；
3. 其它公开目录未发现新 locator。
