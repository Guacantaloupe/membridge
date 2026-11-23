# fluffycats_extra 前 5 个物理候选加载测试

## 结果
| 驱动 | 状态 |
|---|---|
| `2.0.0.620_bedrock64.sys` | STOPPED（exit=2） |
| `503111e.sys` | STOPPED（exit=87） |
| `3933112b2.sys` | **RUNNING** |
| `$0_IntelFirmwareInterface_IntelFirmwareInterface.sys` | **RUNNING** |
| `1.sys` | STOPPED（exit=31） |

2/5 RUNNING，无 BSOD。

## 说明
- 两个成功加载的驱动没有明显 `\Device\...` 字符串；
- 更可能是 PnP/固件过滤驱动；
- 未发现可直接打开的 DOS 设备；
- 本轮没有新增可用物理读路径。

## 结论
fluffycats_extra 的候选需要：
1. 按服务/PnP 设备进一步定位；
2. 或先做字符串/导入表聚类；
3. 不能仅凭“能加载”就判断可做物理读。
