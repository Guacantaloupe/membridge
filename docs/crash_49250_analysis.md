# 崩溃分析：CCHWApi.sys 2.9 在物理扫描时触发 BSOD 0x3B

## 崩溃
- Dump：`C:\Windows\Minidump\112226-49250-01.dmp`
  本地副本（**不提交 GitHub**）：`msdriver-0day\evidence\crash_112226-49250-01.dmp`
- 时间：2026-11-22 10:52:43（debug session time）
- BugCheck：**0x0000003B** `SYSTEM_SERVICE_EXCEPTION`
  - Arg1：`0xc0000005` 访问违例
  - Arg2（出错指令）：`0xfffff80751dc9457`
  - Arg3：`0xffff808ad0de6400`
  - 进程：**`membridge.exe`**
- 崩溃前动作：`test_11_pass_cmd_cr3.ps1 -MaxBackends 1 -MaxRanges 3`
  - 目标 backend：`cchwapi_2_9`
  - 第一个物理范围成功，扩大到第 2/3 个范围时崩溃

## WinDbg 分析结果
```
SYSTEM_SERVICE_EXCEPTION (3b)
Arg2: fffff80751dc9457
...
CCHWApi+0x9457:
fffff807`51dc9457 f3a5    rep movs dword ptr [rdi],dword ptr [rsi]

MODULE_NAME: CCHWApi
IMAGE_NAME:  CCHWApi.sys
SYMBOL_NAME: CCHWApi+9457
FAILURE_BUCKET_ID: AV_CCHWApi!unknown_function
CCHWApi   T (no symbols)
    Loaded symbol image file: CCHWApi.sys
    Timestamp: Tue May  1 14:10:54 2018 (5AE804EE)
    CheckSum:  00019775
    ImageSize: 00015000
```

## 对应驱动文件
| 文件 | PE TimeDateStamp | CheckSum | 大小 |
|---|---|---|---|
| **`drivers\CCHWApi_sys\CCHWApi.sys`（cchwapi_2_9）** | **0x5AE804EE** | **0x19775** | 84,968 B |
| `drivers\CCHWApi(1)_sys\CCHWApi(1).sys`（cchwapi_2_12） | 0x5B6E79B3 | 0x1A00D | 92,136 B |
| `drivers\CCHWApiExt(1)_sys\CCHWApiExt(1).sys`（cchwapi_ext_4_5） | 0x5DD3877D | 0x1F92C | 85,736 B |

Dump 中的 `CCHWApi.sys` **Timestamp/CheckSum 与 `CCHWApi_sys\CCHWApi.sys`（2.9）完全匹配**。

## 结论
- 崩溃是 **`CCHWApi.sys` 2.9 驱动自身 bug**：在 `CCHWApi+0x9457` 执行 `rep movs dword ptr [rdi],dword ptr [rsi]` 时访问违例；
- 触发场景：`membridge` 用该驱动做**大范围物理扫描**（大量/较大物理读）；
- 此前单次小范围读取（`-MaxRanges 1`）未崩溃，说明是该驱动在特定物理页/读尺寸下不安全；
- **不是** mrt（mrt 的 FilterManager 警告是另一次加载失败，时间点接近但本次 dump 明确指向 CCHWApi）。

## 安全处理
- 11 驱动测试脚本默认**排除整个 CCHWApi 家族**：
  `cchwapi_2_9`、`cchwapi_2_12`、`cchwapi_ext_4_5`
- 需要测试时仅允许在一次性 VM 上使用：
  ```
  -AllowUnsafeCchwapi
  ```
- 不建议再对 CCHWApi 做全内存扫描；如必须，先用 4KB 小块读并严格限制范围，但仍可能 BSOD。
