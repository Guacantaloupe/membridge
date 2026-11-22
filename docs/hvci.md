# HVCI（Memory Integrity）下的 Provider 使用指南

本机实测（`layers.exe --hvci` / `scripts/hvci_check.py`）：

```
HVCI(memory integrity) enabled=1
VulnerableDriverBlocklistEnable=0
VBS SecurityServicesRunning={2} (HVCI)
```

## 1. HVCI 对 provider 的硬性要求
HVCI 开启时，内核驱动必须同时满足：

1. **Microsoft 签名**：WHQL 或 attestation 签名；**test signing / 未签名驱动不会加载**；
2. **HVCI 兼容**：PE 不得有可写+可执行（W^X）节，不得动态生成/修改代码，不得把可执行页映射为可写；
3. **不在 Microsoft vulnerable driver blocklist 中**（该策略在本机为 0/未启用；其他机器可能启用，已知 BYOVD hash 仍可能被拦）。

> HVCI 本身不是可以“绕过”的东西：它由 Hyper-V 在 VTL1 执行代码完整性检查。可行路径是**选用已签名且 HVCI 兼容的驱动**，或让自研驱动取得 Microsoft 签名。

## 2. 本仓库驱动的静态审计结果
`scripts/hvci_check.py` 对 `drivers/` 全部 `.sys` 做：
- Authenticode 状态 / signer；
- PE 节 W^X 检查；
- 已知 BYOVD 家族提示。

> 这只是**静态审计**；签名有效 + 无 W^X ≠ 保证 HVCI/blocklist 一定加载，仍需管理员实机验证。

### 2.1 PASS（MS/WHQL 签名，无 W^X；HVCI 优先候选）
`cchwapi/*`、`HP SW TOOLS DRIVER`、`KKYUM`、`aswarpot/aswArPot.sys`、`gibepext*`、`hp_swtools_v1`、`isharepp`、`mrt/fekern_mrt.sys`、`swtoolsdriver`、`tdeio`、`ufu`、`winring0/WinRing0x64.sys`。

对 `layers.exe` 的 provider：
| provider | 驱动 | 结论 |
|---|---|---|
| **`aswarpot`** | `aswArPot.sys`（MS/WHQL，无 W^X） | **HVCI 下首选**：任意物理读 + ≤4 字节写 |
| **`mrt`** | `fekern_mrt.sys`（MS/WHQL，无 W^X） | HVCI 下可用：物理读（SYSTEM） |
| **`cchwapi`** | MS/WHQL，无 W^X | HVCI 下可用 |
| `winring0` | repo 的 `WinRing0x64.sys` PASS；但默认 `WinRing0_1_0_1` 样本 FAIL W^X | 仅使用 PASS 变体/设备名时再试 |

### 2.2 WARN（vendor 签名；HVCI 可能需要 Microsoft 签名/再签名）
`asio2`（AsIO2.sys）、`dumpit`（DumpIt.sys）、`invarysensor`、`kprl`（l1malwarebits）、`rtkiow8x64`、`glckio2`、`icachex`、`directio`。这些可能在某些 HVCI 策略下加载，也可能因签名链被拒，需实测。

### 2.3 FAIL（W^X 节；HVCI 会拒绝）
`asmmap64`、`atillk64`、`asio64`、`adv64drv`、`ntiolib`、`amifldrv64`、`citmdrv`、`cpuz1`、`asrdrv`、`asupio64`、`winio64`、`sjtwinio`、`rtkio`、`eneio64`、`fastdumpx64` 等。
这些驱动**在 HVCI 开启时不能依赖**，即使它们在其他机器上可用。

## 3. 让 provider 在 HVCI 下正常使用的操作路径
### 3.1 使用已签名的 HVCI 兼容驱动（推荐）
1. 选择 PASS 列表里的驱动（`aswarpot` / `mrt` / `cchwapi` 等）；
2. 管理员安装/启动驱动：
   ```
   sc.exe create <Svc> type= kernel start= demand binPath= C:\path\driver.sys
   sc.exe start <Svc>
   ```
3. 检查 `--hvci`、`--probe-readers`，再执行：
   ```
   layers.exe --reader aswarpot --read-phys --pa <pa> --size 16
   ```
4. 或直接用动态验证 harness：
   ```
   pwsh -File scripts\dynamic_verify.ps1 -Reader aswarpot -DevicePath "\\.\aswSP_ArPot2" -DriverPath C:\path\aswArPot.sys -ServiceName aswArPot -Locate
   ```

**驱动加载失败时的错误判断：**
- `577` / `0xC0000365`：签名/代码完整性被拒（未签名、证书吊销、HVCI 不兼容）；
- `1275` / `0xC0000428`：驱动被阻止/完整性策略拦截；
- `0xC0000035`、`2`：设备/符号链接未创建；
- `1275` 在 blocklist 启用时也可能是 vulnerable-driver blocklist。

### 3.2 自研驱动 `membridgephys`（需要 Microsoft 签名）
`drivers/membridgephys/` 提供了**按 HVCI 兼容原则编写**的 WDM 驱动源码：
- 无 W^X 节、无动态代码、只用 `MmMapIoSpace` + `RtlCopyMemory`；
- 提供 `\\.\MembridgePhys` 物理读 `0x80002000` / 写 `0x80002004`；
- 源码/INF/build.ps1 已提交 GitHub。

**但 HVCI 开启时未经 Microsoft 签名的 .sys 不能加载**。要让 `--reader membridgephys` 可用：
1. 用 WDK 10 构建 `MembridgePhys.sys`；
2. 通过 Microsoft Partner Center 做 **attestation signing** 或 WHQL；
3. 安装签名后的驱动，再运行 provider。
（test signing 在 HVCI 开启时无效。）

### 3.3 不要做的事
- 不要试图用 test signing/关闭 Secure Boot 绕过 HVCI（需要管理员+重启，且不属于 HVCI 开启场景）；
- 不要把 W^X FAIL 的驱动用于 HVCI 机器；
- 不要在生产机器加载这些研究样本。

## 4. 驱动上传 GitHub
所有 provider 驱动样本已提交到 `membridge/drivers/`：
- 已签名第三方驱动：asmmap64、atillk64、AsIO2、AsIO64、ADV64DRV、NTIOLib、WinRing0、Amifldrv64、CITMDRV、aswArPot、DumpIt、invarysensor、KPRL、cpuz、AsUpIO64、AsrDrv 等；
- 自研 HVCI-compatible 驱动源码：`drivers/membridgephys/`；
- 驱动映射/说明：`drivers/README.md`；
- HVCI 审计：`scripts/hvci_check.py` / `evidence/hvci_driver_report.json`。

> 这些样本仅用于授权测试环境；在 HVCI 机器上请优先使用第 2.1 节的 PASS 驱动。
