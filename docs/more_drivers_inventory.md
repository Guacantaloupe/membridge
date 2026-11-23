# membridge 可使用的更多驱动清单

## 结论
仓库里共有 **42 个 `.sys` 驱动文件**，目前项目可触达的驱动分三类：

1. `membridge --backend`：29 个物理/底层 backend（含 11 个主驱动）
2. `layers --reader`：21 个 provider（HVCI/内核访问）
3. `layers --locate`：KKYUM / KPRL / Invary 定位器

除了之前固定使用的 11 个，本次盘点出 **14 个尚未接入 11 驱动测试、但 `layers` 已支持的 provider**，以及若干已存在但不在 11 个里的 legacy backend。

---

## 一、新增可用 provider（14 个）

| provider | 设备 | 能力 | HVCI audit |
|---|---|---|---|
| `asrdrv` | `\\.\AsrDrv109` | read（AES cmd） | FAIL(W^X) |
| `cpuz` | `\\.\Global\CPUZ152` | read（dword，page guard） | FAIL(W^X) |
| `asupio64` | `\\.\AsUpdateio` | read（PEUPDATER.EXE gate） | FAIL(W^X) |
| `asio2` | `\\.\Asusgio2` | read（ASUSCERT gate） | WARN(vendor) |
| `aswarpot` | `\\.\aswSP_ArPot2` | read/write（<=4B） | **PASS(WHQL)** |
| `winio64` | `\\.\WinIo` | read/write（map） | FAIL(W^X) |
| `dumpit` | `\\.\C:` 卷过滤 | read（dynamic device） | WARN(vendor) |
| `asmmap` | `\\.\ASMMAP64` | read/write（map） | FAIL(W^X) |
| `atillk64` | `\\.\atillk64` | read/write | FAIL(W^X) |
| `asio64` | `\\.\Asusgio` | read/write（map） | FAIL(W^X) |
| `adv64drv` | `\\.\ADV64DRV` | read | FAIL(W^X) |
| `ntiolib` | `\\.\NTIOLib_1_0_C` | read/write | FAIL(W^X) |
| `amifldrv` | `\\.\genericdrv` | read/write（byte） | FAIL(W^X) |
| `citmdrv` | `\\.\CITMDRV` | read（map to caller VA） | FAIL(W^X) |

其中 **`aswarpot` 是 WHQL/MS 签名且 HVCI PASS**，最值得优先评估；但它的写能力限制在 <=4 字节。

---

## 二、已存在但不在 11 个里的 legacy backend

这些已经能被 `membridge --backend` 或 `layers --reader legacy` 使用：

- `sjtwinio`
- `rtkio`
- `rtkiow8x64`
- `eneio64`
- `winio64_elan`
- `winio64_1`
- `glckio2`
- `fastdumpx64`
- `fastdumpx64_v2`
- `directio`
- `icachex`
- `mrt`
- `winring0x64`

---

## 三、定位器类驱动

| 驱动 | 用途 |
|---|---|
| `KKYUM` | 内核 VA + EPROCESS/CR3 定位 |
| `kprl` | CR3 by PID IOCTL `0x239584` + 进程读 `0x234408` |
| `invarysensor` | 内核 VA locator |

它们不是物理读驱动，但属于项目可用的驱动程序。

---

## 四、文件与签名
新增 provider 对应文件均位于 `drivers\<name>\*.sys`，签名状态：

- 大多数：`Valid`
- `ntiolib`：`UnknownError`，优先排除
- `aswarpot`、`mrt`：WHQL，HVCI PASS

---

## 五、优先级建议

1. **aswarpot**：WHQL/HVCI PASS，先评估其读能力；
2. **mrt**：WHQL，物理读，但 device ACL SYSTEM-only；
3. **asrdrv / asupio64 / asio2**：有 gate，需要对应进程名/证书；
4. **winio64 / asmmap / asio64 / citmdrv**：map 型，W^X 风险；
5. **ntiolib / amifldrv / adv64drv / atillk64**：签名/兼容性风险较高，最后评估。

---

## 六、当前限制
当前 DSH 会话是 Medium 权限，无法加载驱动做 live probe。  
本清单基于：
- 文件清单；
- Authenticode 签名；
- `membridge --list`；
- `layers --list-legacy`；
- `layers --list-providers`。

下一步需要管理员权限逐个 `sc create/start` + `layers --probe-readers` 做 live 验证。
