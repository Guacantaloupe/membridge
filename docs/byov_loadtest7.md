# byov_input 第七批（剩余驱动）

## 加载结果
| 驱动 | 状态 |
|---|---|
| `out_asuss33.sys` | STOPPED（exit=183） |
| `out_itkvar.sys` | STOPPED（exit=31） |
| `out_mst.sys` | RUNNING |
| `out_mst10.sys` | RUNNING |
| `out_nvflash1.sys` | STOPPED（exit=183） |
| `vmwprotect.sys` | STOPPED（exit=2） |

2/6 RUNNING，无 BSOD。

## 已覆盖
至此，`byov_input` 的 64 个唯一驱动均已完成至少一次 `sc create/start` 测试。

## 汇总
- 总共导入：64 个
- 测试覆盖：64 个
- 成功 RUNNING 的驱动包括：
  - asio3 / AsrDrv1 / astra64 / atszio64 / ctrl_* / iomap / umbus2 / sivx64 / system_monitor / JYWinRing0 / mtxmem / out_* / cb_* / loc_* / fekern 等
- 已验证可读：
  - **GLCKIO2**（页表读/翻译）
  - **fekern**（任意 PA 单页读）
  - **JYWinRing0**（低内存读）

## 当前状态
- 最新 dump：`112326-44078-01.dmp`
- 无新 BSOD
