# 24 个 legacy 内存读取驱动（membridge backends）与 HVCI 状态

in\membridge.exe --list 实测列出 **24 个 backend**；下表由 src/membridge.c 的 g_backends[] 与 evidence/hvci_driver_report.json 自动生成（静态审计）。

| # | backend id | driver | HVCI 审计 | 签名 |
|---|---|---|---|---|
| 1 | cchwapi_2_9 | CCHWApi_sys/CCHWApi.sys | PASS:MS/WHQL-signed | Valid |
| 2 | cchwapi_2_12 | CCHWApi(1)_sys/CCHWApi(1).sys | PASS:MS/WHQL-signed | Valid |
| 3 | cchwapi_ext_4_5 | CCHWApiExt(1)_sys/CCHWApiExt(1).sys | PASS:MS/WHQL-signed | Valid |
| 4 | sjtwinio | SjtWinIo_sys/SjtWinIo.sys | FAIL:W^X-section | Valid |
| 5 | hp_swtools | HP SW TOOLS DRIVER_sys/HP SW TOOLS DRIVER.sys | PASS:MS/WHQL-signed | Valid |
| 6 | swtoolsdriver | swtoolsdriver/swtoolsdriver.sys | PASS:MS/WHQL-signed | Valid |
| 7 | hp_swtools_v1 | hp_swtools_v1/HP_SWTOOLS_DRIVER_V1.sys | PASS:MS/WHQL-signed | Valid |
| 8 | isharepp | isharepp/iSharePp.sys | PASS:MS/WHQL-signed | Valid |
| 9 | rtkio | rtkio/rtkio.sys | FAIL:W^X-section | Valid |
| 10 | rtkiow8x64 | rtkiow8x64/rtkiow8x64.sys | WARN:vendor-signed(HVCI-may-require-MS) | Valid |
| 11 | eneio64 | eneio64/eneio64.sys | FAIL:W^X-section | Valid |
| 12 | winio64_elan | winio64_elan/WinIo64_ELAN.sys | FAIL:W^X-section | Valid |
| 13 | winio64_1 | winio64_1/WinIo64_1.sys | FAIL:W^X-section | Valid |
| 14 | glckio2 | glckio2/glckio2.sys | WARN:vendor-signed(HVCI-may-require-MS) | Valid |
| 15 | gibepext | gibepext/gibepext.sys | PASS:MS/WHQL-signed | Valid |
| 16 | gibepext2 | gibepext2/gibepext2.sys | PASS:MS/WHQL-signed | Valid |
| 17 | fastdumpx64 | fastdumpx64/fastdumpx64.sys | FAIL:W^X-section | Valid |
| 18 | fastdumpx64_v2 | fastdumpx64_v2/fastdumpx64_v2.sys | FAIL:W^X-section | Valid |
| 19 | directio | directio/DirectIo.sys | WARN:vendor-signed(HVCI-may-require-MS) | Valid |
| 20 | tdeio | tdeio/TdeIo64.sys | PASS:MS/WHQL-signed | Valid |
| 21 | icachex | icachex/iCacheX.sys | WARN:vendor-signed(HVCI-may-require-MS) | Valid |
| 22 | mrt | mrt/fekern_mrt.sys | PASS:MS/WHQL-signed | Valid |
| 23 | ufu_win10 | ufu/win10_ufudev64.sys | PASS:MS/WHQL-signed | Valid |
| 24 | winring0x64 | winring0/WinRing0x64.sys | PASS:MS/WHQL-signed | Valid |

## 计数
- 总 backends：**24**
- PASS 子集（MS/WHQL 签名 + 无 W^X）：**13**
- WARN（vendor 签名）：**4**
- FAIL（W^X/未签名等）：**7**

## 结论
- HVCI 下优先使用 PASS 子集；membridge.exe --backend <id> 会按 backend 自动加载驱动（需管理员）。
- ufu_win10 仅 32-bit PA/0x1024 字节；winring0x64 仅低内存 0xC0000-0xFFFFF，非任意 64-bit PA。
- 其余 WARN/FAIL 在 HVCI 下可能被签名/ W^X 拒绝；详见 docs/hvci.md。
- 另：layers.exe 当前有 **20 个真实 provider + mock**；repo drivers/ 共 **47 个 .sys**（含 variants 与新 provider 驱动）。
