# byov_input 语料新增驱动（25 个）

## 来源
```
msdriver-0day\analysis\byov_input
```
本地已有 112 个驱动样本，其中 25 个与项目现有驱动 SHA256 不同，已复制到：
```
drivers/byov_input/
```

## 新增清单
| 驱动 | 大小 | 签名 |
|---|---:|---|
| 360rescon.sys | 29416 | Valid |
| aac-driver.sys | 431272 | Valid |
| asio3.sys | 59976 | Valid |
| AsIO3_1.sys | 59976 | Valid |
| AsrDrv1.sys | 68816 | Valid |
| astra64.sys | 21200 | Valid |
| asussaio.sys | 37048 | Valid |
| atszio64.sys | 43584 | Valid |
| cb_arkmon.sys | 399224 | Valid |
| cb_aswarpot.sys | 296544 | Valid |
| cb_fekern.sys | 267840 | Valid |
| cb_gsdriver.sys | 85736 | Valid |
| cb_khwinfo.sys | 80568 | Valid |
| cb_livekdd.sys | 39272 | Valid |
| cb_mark.sys | 265416 | Valid |
| cb_scansafe.sys | 95920 | Valid |
| cb_smr522.sys | 112088 | Valid |
| cb_tdcore.sys | 472568 | Valid |
| CTFarmMonitor.sys | 44984 | Valid |
| ctrl_asmio.sys | 40808 | Valid |
| ctrl_asus.sys | 25232 | Valid |
| ctrl_isstrtc.sys | 346728 | Valid |
| ctrl_jmkmd.sys | 618544 | Valid |
| ctrl_mst.sys | 87456 | Valid |
| ctrl_pawnio.sys | 85344 | Valid |

SHA256 清单：
```
evidence/byov_input_additions.json
```

## 优先级
- `asio3` / `AsIO3`：ASUS I/O 家族；
- `AsrDrv1/2`：ASRock I/O；
- `astra64`、`atszio64`：硬件工具；
- `cb_*`：分析过的内核/反作弊类驱动，需逐一验证；
- `ctrl_*`：Asus/MSI/PawnIO 等硬件访问。

## 下一步
1. 逐个只加载、不卸载测试；
2. 对能加载+设备可打开者做单页读；
3. 避免批量扫描；
4. 继续从 byov_input 其余样本和 namazso 仓库补驱动。
