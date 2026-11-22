# 涓夊眰鐙珛鏋舵瀯锛氬畾浣嶅眰 / 缈昏瘧灞?/ 鍙栨暟灞?
`membridge/bin/layers.exe` 鎶婁换鎰忚繘绋嬭櫄鎷熷唴瀛樿闂媶鎴?**涓変釜浜掍笉渚濊禆鐨勫眰**锛?
```
瀹氫綅灞?(locator)      鐩爣 PID/鍚嶇О 鈹€鈹€鈻?CR3
缈昏瘧灞?(translator)   CR3 + VA 鈹€鈹€鈹€鈹€鈹€鈹€鈻?PA锛? 绾ч〉琛紝鏀寔 4KB/2MB/1GB 椤碉級
鍙栨暟灞?(fetcher)      PA + len 鈹€鈹€鈹€鈹€鈹€鈹€鈻?bytes锛堝彲鎻掓嫈鐗╃悊璇?provider锛?```

涓夊眰涔嬮棿鍙€氳繃鏄惧紡鏁版嵁缁撴瀯浜や簰锛屼换涓€灞傚彲鍗曠嫭鏇挎崲/娴嬭瘯锛?
## 1. 瀹氫綅灞傦紙鑾峰彇 CR3锛?`mb_layer_locate_cr3_*`锛?
| 绛栫暐 | 璇存槑 | 渚濊禆 |
|---|---|---|
| `--cr3 <hex>` | 鐩存帴鎻愪緵 CR3锛堟祴璇?宸茬煡鍊硷級 | 鏃?|
| `--locate --pid <pid> --scan-start/--scan-size` | **鐗╃悊鍐呭瓨鎵弿**锛氬湪鐗╃悊鑼冨洿涓寜 `_EPROCESS` 甯冨眬锛圥ID `+0x1D0`銆両mageFileName `+0x338`銆丏irectoryTableBase `+0x28`锛夊畾浣嶇洰鏍囪繘绋嬪苟鍙?CR3 | 浠呭彇鏁板眰 |
| `--locate --pid <pid> --auto-ranges` | 鍚屼笂锛岃寖鍥磋嚜鍔ㄤ粠娉ㄥ唽琛?`HARDWARE\RESOURCEMAP` 鑾峰彇 | 浠呭彇鏁板眰 |
| `--locate --kkyum --pid <pid>` | **鍐呮牳 VA 璇?+ 杩涚▼閾捐〃閬嶅巻**锛欿KYUM 璇?`PsInitialSystemProcess` 鈫?`ActiveProcessLinks` 鈫?鐩爣 EPROCESS 鈫?CR3锛坄--kkyum-device` 鍙寚瀹氳澶囷級 | 浠呭唴鏍歌 provider锛堜笉闇€瑕佸彇鏁板眰锛?|
| `--locate --kprl --pid <pid>` | **KPRL `l1malwarebits.sys` CR3-by-PID**锛欼OCTL `0x239584`锛堣緭鍏?PID锛屽亸绉?0x38 杩斿洖 CR3锛沗--kprl-device` 鎸囧畾璁惧锛?| KPRL 椹卞姩锛堜笉闇€瑕佸彇鏁板眰锛?|
| `--locate --invary --pid <pid>` | **Invary Sensor 鍐呮牳 VA 璇?+ 杩涚▼閾捐〃**锛欼OCTL `0x9C402408`锛坄MmCopyMemory` Virtual锛夎 `PsInitialSystemProcess` 鈫?`ActiveProcessLinks` 鈫?CR3锛坄--invary-device` 鎸囧畾璁惧锛?| Invary 椹卞姩锛堜笉闇€瑕佸彇鏁板眰锛?|

瀹氫綅灞?*涓嶉渶瑕?*缈昏瘧灞傦紝涔熶笉闇€瑕侀〉琛紱鎵弿绛栫暐鍙渶瑕佸彇鏁板眰銆?
## 2. 缈昏瘧灞傦紙鑾峰彇 PA锛?`mb_layer_translate(reader, cr3, va, &pa, &page_size)`锛?- 4 绾ч〉琛ㄩ亶鍘嗭細PML4 鈫?PDPT 鈫?PD 鈫?PT锛?- 鏀寔 1GB锛圥DPT.PS锛夈€?MB锛圥D.PS锛夈€?KB 椤碉紱
- 鍙緷璧?*鍙栨暟灞?*鐨?`read(pa)`锛屼笉渚濊禆瀹氫綅灞傦紱
- 杈撳叆鏄惧紡 CR3 鍗冲彲鐙珛宸ヤ綔锛?- 绂荤嚎鑷祴宸茶鐩栵細4KB 鏄犲皠銆?MB 澶ч〉銆?GB 澶ч〉銆佽法椤电粍鍚堣銆?
- 鍙緷璧?*鍙栨暟灞?*鐨?`read(pa)`锛屼笉渚濊禆瀹氫綅灞傦紱
- 杈撳叆鏄惧紡 CR3 鍗冲彲鐙珛宸ヤ綔銆?
## 3. 鍙栨暟灞傦紙鑾峰彇瀛楄妭锛?`mb_phys_reader` 鎺ュ彛锛?
```c
typedef struct mb_phys_reader {
    int (*read)(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size);
    ...
} mb_phys_reader;
```

`layers.exe` 鍐呯疆 provider锛?| provider | 璁惧/鍗忚 |
|---|---|
| `cchwapi` | `\\.\CCHWAPI`锛孖OCTL `0x8024003C`锛坲64 PA锛?|
| `rtkio` | `\\.\rtkio`锛孖OCTL `0x80002000`锛圥A 鏄犲皠鍒扮敤鎴?VA锛?|
| `eneio64` | `\\.\GLCKIo`锛孖OCTL `0x80102040`锛圵inIO 鐗╃悊鏄犲皠锛?|
| `asrdrv` | `\\.\AsrDrv109`锛孋VE-2020-15368 瀹舵棌 AES 鍔犲瘑澶栧眰 IOCTL `0x22EC00` + 鐗╃悊璇?opcode `0x22E808`锛圧ound 27 宸查潤鎬佺‘璁ょ殑 AsrDrv(2).sys锛涘姩鎬侀獙璇佸緟鎻愭潈锛?|
| `mrt` | `\\.\fekern_00`锛孎ireEye/fekern MRT `MRT_IOCTL_READ_PHYSICALMEMORY 0x70F8800C`锛坄MmCopyMemory` 鐗╃悊璇伙紱璁惧 ACL SYSTEM-only锛涘姩鎬侀獙璇佸緟 SYSTEM锛?|
| `cpuz` | `\\.\Global\CPUZ152`锛坒allback `\\.\CPUZ152`锛夛紝CPU-Z 1.52 鐗╃悊 dword 璇?`0x9C402708`锛涢渶 `--expect-first <dword>` 鎻愪緵鐩爣椤甸 dword 閫氳繃椹卞姩鏍￠獙锛涘姩鎬侀獙璇佸緟鎻愭潈 |
| `asupio64` | `\\.\AsUpdateio`锛圓SUS EZ Update 3.00.03锛夛紝鐗╃悊鏄犲皠 IOCTL `0xA040A480`锛堥潤鎬佹帹瀵?24B 璇锋眰 `{interfaceType,busNumber,phys_start,addressSpace,size}` 鈫?杩斿洖 user VA锛夛紱**鍏煎鏃х増 AsUpIO 0x18 璇锋眰鍥為€€**骞惰嚜鍔?unmask锛涢棬鎺ц姹傚綋鍓嶈繘绋嬪悕鍚?`PEUPDATER.EXE`锛岄渶鎶?layers.exe 澶嶅埗/鏀瑰悕涓?`PEUPDATER.EXE`锛涘姩鎬侀獙璇佸緟鎻愭潈 |
| `asio2` | `\\.\Asusgio2`锛圓SUS AsIO2锛夛紝鐗╃悊鏄犲皠 IOCTL `0xA040A480`锛?4-bit 0x28 璇锋眰 `{u64 size; u64 phys_start; section_handle_out; user_va_out}`锛夛紱椹卞姩闂ㄦ帶璇诲彇褰撳墠杩涚▼ PE 鐨?**ASUSCERT 璧勬簮**锛圓ES+PE 鏃堕棿鎴虫牎楠岋級鈥斺€斿凡鏋勯€?`bin\layers_asio2.exe`锛圓SUSCERT=AES_dec(0)锛孴imeDateStamp=0锛夌绾块€氳繃闂ㄦ帶锛涘姩鎬侀獙璇佸緟鎻愭潈 |
| `aswarpot` | Avast anti-rootkit锛坄aswArPot.sys` 澶氱増鏈級锛岃澶?`\\.\aswSP_ArPot2`锛堝€欓€?`avastSP_ArPot2`锛夛紱浠绘剰鐗╃悊璇?IOCTL **`0x9988C408`**锛坕nput 12B `{u32 pa; u32 rsvd; u32 size}`锛宱utput=size bytes锛?2-bit PA锛孧mMapIoSpace锛夛紱鍙︽湁浠绘剰鐗╃悊鍐?IOCTL `0x9988C40C`锛圥A+鈮? 瀛楄妭锛夛紱闈欐€侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `winio64` | WinIo64锛堢粡鍏哥墿鐞嗗唴瀛樻槧灏勯┍鍔紝`\\.\WinIo`锛夛紱IOCTL **`0x80102040`** MAPPHYSTOLIN锛?x28 璇锋眰 `{u64 size; u64 phys_start; section_handle; user_va; object}`锛屾槧灏?`\Device\PhysicalMemory`锛夛紝`0x80102044` UNMAP锛涗换鎰忕墿鐞嗘槧灏?*璇?鍐?*锛堟槧灏勪负鍙啓 Section锛夛紱闈欐€侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `dumpit` | DumpIt 鍙栬瘉椹卞姩锛堝嵎杩囨护鍣紝璁惧鍚嶅姩鎬?GUID锛沗--device \\.\C:` 鎸囧畾鍗凤級锛汭OCTL **`0x22C01E`** 鐗╃悊璇伙細input `{u64 phys_addr}`锛堥暱搴?>7锛夛紝杈撳嚭缂撳啿鍖洪暱搴﹀嵆璇诲彇瀛楄妭鏁帮紱闈欐€侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `asmmap` | ASUS ASMMAP64锛坄\\.\ASMMAP64`锛夛紱IOCTL **`0x9C402580`** MAPMEM锛?x18 璇锋眰 `{u64 phys_start; u64 user_va_out; u64 length}`锛屾槧灏?`\Device\PhysicalMemory`锛夛紝`0x9C402584` UNMAP锛?*浠绘剰鐗╃悊鏄犲皠璇?鍐?*锛涘彟鏈?0x9C402588/58C 杩炵画鐗╃悊鍒嗛厤/閲婃斁銆?x9C402590 绔彛 I/O锛涢潤鎬侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `atillk64` | ATI/AMD atillk64锛坄\\.\atillk64`锛夛紱鐗╃悊璇?IOCTL **`0x9C402544`**锛坕nput 0x10 `{u64 phys; u64 size}`锛宍MmMapIoSpace` 鎷疯礉 size 瀛楄妭锛夛紱鐗╃悊鍐?IOCTL **`0x9C402548`**锛坕nput 0x18 `{u64 phys; u64 size; u64 src}`锛夛紱鍙︽湁 MSR/绔彛/PCI 璁块棶锛涢潤鎬侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `asio64` | ASUS Asusgio锛坄\\.\Asusgio`锛夛紱鐗╃悊鏄犲皠 IOCTL **`0xA040244C`**锛坕nput 0x18 `{u32 interfaceType; u32 busNumber; u64 phys; u32 addressSpace; u32 length}`锛屾槧灏?`\Device\PhysicalMemory`锛岃緭鍑?8 瀛楄妭 VA锛夛紝`0xA0402450` UNMAP锛涙槧灏勫彲鍐欙紝鏀寔璇?鍐欙紱闈欐€侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `adv64drv` | ADV64DRV锛坄\\.\ADV64DRV`锛夛紱鐗╃悊璇?IOCTL **`0x224004`**锛坕nput 0x10 `{u64 phys; u32 size; u32 reserved}`锛宍HalTranslateBusAddress` + `MmMapIoSpace` 鎷疯礉 size 瀛楄妭锛夛紱闈欐€侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `ntiolib` | NTIOLib锛坄\\.\NTIOLib_1_0_C`锛夛紱鐗╃悊璇?IOCTL **`0xC3506104`**锛坕nput 0x10 `{u64 phys; u32 width; u32 count}`锛岃緭鍑洪暱搴?鈮?width*count锛宍MmMapIoSpace` 鎷疯礉锛夛紱鐗╃悊鍐?IOCTL **`0xC3506108`**锛?x10 澶?+ 鏁版嵁锛夛紱鍙︽湁 MSR/绔彛/PCI 璁块棶锛涢潤鎬侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `winring0` | WinRing0锛坄\\.\WinRing0_1_0_1`锛屽€欓€?`WinRing0_1_2_0`锛夛紱鐗╃悊璇?IOCTL **`0x9C406104`**锛坕nput 0x10 `{u64 phys; u32 width; u32 count}`锛夛紱鐗╃悊鍐?IOCTL **`0x9C406108`**锛涘彟鏈?MSR/绔彛/PCI 璁块棶锛涢潤鎬侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `amifldrv` | AMI flash锛坄\\.\genericdrv`锛夛紱鐗╃悊璇?IOCTL **`0xFA002EEC`**锛坕nput 0x10 `{u64 phys; u32 addressSpace; u32 width}`锛寃idth 0/1/2 = 1/2/4 瀛楄妭锛岀粨鏋滃湪杈撳嚭 +0x0c锛沺rovider 閫愬瓧鑺傝锛夛紱鐗╃悊鍐?IOCTL **`0xFA002EF0`**锛堝悓 0x10 缁撴瀯锛?0x0c 涓哄啓鍏ュ€硷紱閫愬瓧鑺傚啓锛夛紱鍙︽湁 0xFA002EE0/EE4/EE8/EF8 杩炵画鐗╃悊鍒嗛厤/鏄犲皠锛涢潤鎬侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `citmdrv` | CITMDRV锛坄\\.\CITMDRV`锛夛紱IOCTL **`0x80002000`** MAP_MEM锛坕nput 0x18 `{u64 phys; u64 user_buffer_va; u32 size; u32 reserved}`锛屾槧灏?`\Device\PhysicalMemory` 骞舵妸 size 瀛楄妭鎷疯礉鍒拌皟鐢ㄦ柟 VA锛夛紱闈欐€侀€嗗悜纭锛屽姩鎬侀獙璇佸緟鎻愭潈 |
| `membridgephys` | 椤圭洰鑷爺 HVCI-compatible WDM 椹卞姩婧愮爜锛坄drivers/membridgephys/`锛宍\\.\MembridgePhys`锛夛紱IOCTL **`0x80002000`** 璇?/ **`0x80002004`** 鍐欙紙input 0x10 `{u64 phys; u32 size; u32 reserved}`锛夛紱**HVCI 涓嬮渶 Microsoft 绛惧悕鍚庢墠鑳藉姞杞?* |
| `mock` | 绂荤嚎鑷祴鐢ㄥ悎鎴愮墿鐞嗗唴瀛?|

`membridge.exe` 宸查泦鎴?22 涓叏 64 浣嶇墿鐞嗚 backend锛坈chwapi/rtkio/eneio64/hp/gibepext/fastdump/directio/tdeio/icachex/mrt 绛夛級锛涙瘡涓?backend 閮藉彧鏄彇鏁板眰鐨勪竴涓疄鐜帮紝鏂板 backend 涓嶅奖鍝嶅畾浣嶅眰鍜岀炕璇戝眰銆?
## 4. 缁勫悎鎺ュ彛
`mb_layer_read_va(reader, cr3, va, buf, len)` 鍙仛缁勫悎锛?- 閫愰〉璋冪敤缈昏瘧灞?鈫?寰楀埌 PA锛?- 鐒跺悗璋冪敤鍙栨暟灞?鈫?寰楀埌瀛楄妭銆?- 涓嶅寘鍚畾浣嶉€昏緫锛圕R3 鐢辫皟鐢ㄨ€呮彁渚涳級锛屼繚璇佷笁灞傜嫭绔嬨€?
## 5. 绂荤嚎楠岃瘉锛堟棤闇€绠＄悊鍛?椹卞姩锛?`layers.exe --selftest-mock` 鐢?*鍚堟垚鐗╃悊鍐呭瓨**鏋勯€狅細
- 4 绾ч〉琛ㄦ妸 `VA 0x400000` 鏄犲皠鍒?`PA 0x5000`锛?- 浼€?`_EPROCESS`锛圥ID 4242 / MOCK.exe / DTB 0x1000锛夛紱
- 鍒嗗埆楠岃瘉锛氬彇鏁板眰銆佺炕璇戝眰銆佸畾浣嶅眰銆佷笁灞傜粍鍚堛€佺嫭绔嬫€с€?
缁撴灉锛?7/47 PASS锛夛細
```
[PASS] 鍙栨暟灞?fetcher): PA=0x5000 bytes match mock data
[PASS] 缈昏瘧灞?translator): VA=0x400000 -> PA=0x5000 (4KB page)
[PASS] 缈昏瘧灞?translator): VA=0x400010 -> PA=0x5010 (offset preserved)
[PASS] 缈昏瘧灞?translator): VA=0x200000 -> PA=0x200000 (2MB page)
[PASS] 涓夊眰缁勫悎(pipeline): read VA 0x200010 through 2MB page
[PASS] 缈昏瘧灞?translator): VA=0x40000000 -> PA=0x40000000 (1GB page)
[PASS] 涓夊眰缁勫悎(pipeline): read VA 0x80001000 through 1GB page
[PASS] 涓夊眰缁勫悎(pipeline): cross-page read VA 0x400FF0 (two 4KB pages)
[PASS] 缈昏瘧灞?translator): unmapped VA=0x800000 rejected
[PASS] 鍙栨暟灞?fetcher): asupio64 map request layout (page-aligned)
[PASS] 鍙栨暟灞?fetcher): asupio64 cross-page map request (map size 0x2000)
[PASS] 鍙栨暟灞?fetcher): asio2 map request layout (gate-bypass PE)
[PASS] 鍙栨暟灞?fetcher): cpuz request layout (addr/expected)
[PASS] 鍙栨暟灞?fetcher): mrt request layout (PA/size/flags)
[PASS] 鍙栨暟灞?fetcher): asrdrv read command layout (opcode/args)
[PASS] 鍙栨暟灞?fetcher): aswArPot physical read request layout
[PASS] 鍙栨暟灞?fetcher): aswArPot physical write request layout
[PASS] 鍙栨暟灞?fetcher): winio64 map request layout (size/phys)
[PASS] 鍙栨暟灞?fetcher): dumpit physical read request layout
[PASS] 鍙栨暟灞?fetcher): asmmap64 map request layout (phys/len)
[PASS] 鍙栨暟灞?fetcher): atillk64 read request layout (PA/size)
[PASS] 鍙栨暟灞?fetcher): atillk64 write request layout (PA/size/src)
[PASS] 鍙栨暟灞?fetcher): asio64 map request layout (phys/len)
[PASS] 鍙栨暟灞?fetcher): adv64drv physical read request layout
[PASS] 鍙栨暟灞?fetcher): ntiolib read request layout (PA/width/count)
[PASS] 鍙栨暟灞?fetcher): winring0 read request layout (PA/width/count)
[PASS] 鍙栨暟灞?fetcher): amifldrv read request layout (PA/width)
[PASS] 鍙栨暟灞?fetcher): amifldrv write request layout (PA/value)
[PASS] 鍙栨暟灞?fetcher): citmdrv map request layout (PA/dst/size)
[PASS] 鍙栨暟灞?fetcher): membridgephys read request layout (PA/size)
[PASS] 鍙栨暟灞?fetcher): kkyum direct process-read request layout (pid/src/dst/size)
[PASS] 鍙栨暟灞?fetcher): legacy backend registry (24 ids, find cchwapi_2_12, reject bad id)
[PASS] 瀹氫綅灞?locator): provider-scan registry has 20 physical providers
[PASS] 瀹氫綅灞?locator): PID 4242 / MOCK.exe -> CR3=0x1000 (physical scan)
[PASS] 瀹氫綅灞?locator): fake EPROCESS PA=0x8000
[PASS] 瀹氫綅灞?locator): kernel-reader list walk -> CR3=0x3000 (no physical reader)
[PASS] 瀹氫綅灞?locator): kprl CR3 query request layout (pid at offset 0)
[PASS] 鍙栨暟灞?fetcher): invarysensor kernel-VA read request layout
[PASS] 涓夊眰缁勫悎(pipeline): locate -> translate -> fetch returns mock data
[PASS] 鐙珛鎬?independence): 鍙栨暟灞?usable without locator/translator
[PASS] 鐙珛鎬?independence): 缈昏瘧灞?usable without locator
[PASS] 瀹氫綅灞?locator): registry range parser (Memory/MemoryLarge)
[PASS] 瀹氫綅灞?locator): truncated CM_RESOURCE_LIST rejected
[PASS] 瀹氫綅灞?locator): oversized resource Count rejected
[PASS] 缈昏瘧灞?translator): CR3=0 rejected
[PASS] 缈昏瘧灞?translator): VA=0x1010 -> PA=0x305010 (5-level/LA57)
[PASS] 涓夊眰缁勫悎(pipeline): read VA through 5-level page tables
VERDICT=PASS
```

## 6. 涓?N-day 鍊欓€夌殑鏄犲皠
| 灞?| 鍙敤鐨?N-day / 椹卞姩 |
|---|---|
| 鍙栨暟灞?| `membridge` 宸查泦鎴?22 backend锛涙柊鍊欓€夛細AsrDrv(2)锛堜换鎰忕墿鐞?R/W锛夈€丄sUpIO64锛堢墿鐞嗘槧灏?MSR锛夈€丆PU-Z锛堢墿鐞?dword R/W锛夈€丗ireEye MRT锛堢墿鐞嗚锛孲YSTEM-only锛?|
| 瀹氫綅灞?| 鐗╃悊鎵弿绛栫暐浣跨敤鍙栨暟灞傚嵆鍙紙鏃犻渶鍐呮牳璇婚┍鍔級锛涘唴鏍歌绛栫暐闇€ KKYUM 绛?|
| 缈昏瘧灞?| 绾畻娉曪紝浠讳綍鍙栨暟灞?provider 閮藉彲鐢?|

## 7. 瀹氫綅灞傝緟鍔╋細鑷姩鐗╃悊鍐呭瓨鑼冨洿锛堟棤闇€绠＄悊鍛?椹卞姩锛?鐗╃悊鎵弿 locator 闇€瑕佺煡閬撶墿鐞嗗唴瀛樿寖鍥淬€俙layers.exe --ranges` 鐩存帴璇诲彇娉ㄥ唽琛?`HKEY_LOCAL_MACHINE\HARDWARE\RESOURCEMAP\System Resources\Physical Memory`
鐨?`.Translated`锛圧EG_RESOURCE_LIST锛夛紝瑙ｆ瀽 `CM_PARTIAL_RESOURCE_DESCRIPTOR`
锛圡emory=3 / MemoryLarge=7锛屾敮鎸?0x200/0x400/0x800 澶ч〉闀垮害缂栫爜锛夛細

鏈満瀹炴祴锛圡edium 鐢ㄦ埛锛屾棤闇€绠＄悊鍛?椹卞姩锛夛細
```
[OK] 5 physical memory ranges (registry HARDWARE\RESOURCEMAP):
  [ 0] 0x1000 - 0x9F000  len=0x9E000
  [ 1] 0x100000 - 0xA00000  len=0x900000
  [ 2] 0xA73000 - 0x6D6EB000  len=0x6CC78000
  [ 3] 0x727FF000 - 0x72800000  len=0x1000
  [ 4] 0x100000000 - 0x27F800000  len=0x17F800000
VERDICT=PASS
```
`--locate --pid <pid> --auto-ranges` 浼氶亶鍘嗚繖浜涜寖鍥存壘 `_EPROCESS`锛屾棤闇€鎵嬪伐缁?`--scan-start/--scan-size`锛堜粛闇€鍙栨暟灞?driver 鎻愪緵鐗╃悊璇伙級銆?
## 8. 鐢ㄦ硶
```powershell
# 绂荤嚎涓夊眰楠岃瘉
.\bin\layers.exe --selftest-mock

# 20 涓?provider 鑷姩鐗╃悊鎵弿鎵?CR3锛堝惈 legacy fallback锛?.\bin\layers.exe --locate --scan-providers --pid <pid> --name cmd.exe --auto-ranges --va <va> --size 0x40
# 閫愪釜 provider 鎵撳嵃 open/scan 鑰楁椂
.\bin\layers.exe --locate --scan-bench --pid <pid> --name cmd.exe --scan-start 0x100000 --scan-size 0x100000
pwsh -File scripts\bench_provider_scan.ps1 -ProcessId <pid> -AutoRanges

# 24 legacy backend 鎺ュ叆鍙栨暟灞傦紙CR3+缈昏瘧+鐗╃悊璇伙級
.\bin\layers.exe --list-legacy
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 --read-phys --pa 0x10000000 --size 0x40 --dump
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 --cr3 <CR3> --va <VA> --size 0x40 --dump
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 --locate --pid <pid> --auto-ranges

# 鐙珛杩涚▼鍐呭瓨璇诲彇锛圞KYUM 鐩存帴 PID+VA锛孒VCI 鍏煎椹卞姩锛?.\bin\layers.exe --read-process --pid <cmd_pid> --va 0x7FF600000000 --size 0x40

# 鍙栨暟灞傝澶囧瓨鍦ㄦ€ф帰娴嬶紙鏃犻渶椹卞姩鍗冲彲杩愯锛?.\bin\layers.exe --probe-readers
.\bin\layers.exe --probe-all          # readers + locators 涓€娆℃帰娴?.\bin\layers.exe --list-providers     # provider/device/capability/HVCI 琛?.\bin\layers.exe --hvci               # HVCI 鐘舵€?+ 鎺ㄨ崘 provider + 閿欒鐮佹彁绀?
# 椤圭洰鑷爺 HVCI-compatible 椹卞姩锛堥渶 Microsoft 绛惧悕鍚庡姞杞斤級
#   婧愮爜/INF: drivers/membridgephys/
.\bin\layers.exe --reader membridgephys --read-phys --pa 0x10000000 --size 16

# 绂荤嚎 mock锛氱炕璇?+ 鍙栨暟锛堝唴寤烘暟鎹?"MOCKCLI-DATA-..."锛?.\bin\layers.exe --reader mock --cr3 0x1000 --va 0x400000 --size 8

# 鍔ㄦ€侀獙璇?harness锛堢鐞嗗憳 + 宸茬鍚嶉┍鍔ㄦ椂锛汥ryRun 瀹夊叏棰勬紨锛?.\scripts\dynamic_verify.ps1 -DryRun -Reader winio64 -DevicePath '\\.\WinIo' -Locate
.\scripts\dynamic_verify.ps1 -Reader winio64 -DevicePath '\\.\WinIo' -DriverPath C:\path\WinIo64.sys -ServiceName WinIo64

# 瀹氫綅灞傝澶囧瓨鍦ㄦ€ф帰娴嬶紙KKYUM / Invary / KPRL锛?.\bin\layers.exe --probe-locators

# 瀹氫綅灞傝緟鍔╋細璇诲彇鐗╃悊鍐呭瓨鑼冨洿锛堟棤闇€椹卞姩锛?.\bin\layers.exe --ranges

# 鍙栨暟灞傦細鐗╃悊鍐欙紙aswArPot 1-4 瀛楄妭锛沇inIo64 浠绘剰闀垮害鏄犲皠鍐欙級
.\bin\layers.exe --reader aswarpot --write-phys --pa 0x10000000 --data DEADBEEF
.\bin\layers.exe --reader winio64 --write-phys --pa 0x10000000 --data 0011223344556677

# 鍙栨暟灞傦細鐗╃悊璇伙紙HVCI 鎰熺煡鑷姩閫夋嫨锛歛swarpot -> mrt -> cchwapi锛?.\bin\layers.exe --reader hvci --read-phys --pa 0x10000000 --size 0x40

# 鍙栨暟灞傦細鐗╃悊璇?.\bin\layers.exe --reader rtkio --read-phys --pa 0x10000000 --size 0x40

# 鍙栨暟灞傦細CPU-Z 1.52 鐗╃悊 dword 璇伙紙闇€鎻愪緵鐩爣椤甸 dword锛?.\bin\layers.exe --reader cpuz --expect-first 0x42594F56 --read-phys --pa 0x10000000 --size 0x40

# 鍙栨暟灞傦細AsUpIO64锛堜娇鐢?bin\PEUPDATER.EXE锛涘悓涓€浜岃繘鍒跺悓鏃舵弧瓒?AsUpIO64 鏂囦欢鍚嶉棬鎺т笌 AsIO2 ASUSCERT 闂ㄦ帶锛?.\bin\PEUPDATER.EXE --reader asupio64 --read-phys --pa 0x10000000 --size 0x40

# 鍙栨暟灞傦細AsIO2锛圥EUPDATER.EXE / layers_asio2.exe 鍧囧惈 ASUSCERT 璧勬簮 + TimeDateStamp=0锛?.\bin\layers_asio2.exe --reader asio2 --read-phys --pa 0x10000000 --size 0x40

# 鍙栨暟灞傦細Round 27 鐨?AsrDrv(2) N-day provider锛堥渶鍏堟湁绠＄悊鍛樺姞杞介┍鍔級
.\bin\layers.exe --reader asrdrv --read-phys --pa 0x10000000 --size 0x40

# 瀹氫綅灞傦紙娉ㄥ唽琛ㄨ嚜鍔ㄨ寖鍥寸墿鐞嗘壂鎻?CR3锛? 缈昏瘧灞?+ 鍙栨暟灞?.\bin\layers.exe --reader rtkio --locate --pid 1234 --auto-ranges --va 0x7FF600000000 --size 0x40

# 瀹氫綅灞傦紙鍐呮牳 VA 璇?+ 杩涚▼閾捐〃锛孠KYUM锛涗笉闇€瑕佸彇鏁板眰锛?.\bin\layers.exe --locate --kkyum --pid 1234

# 瀹氫綅灞傦紙KPRL l1malwarebits CR3-by-PID锛涗笉闇€瑕佸彇鏁板眰锛?.\bin\layers.exe --locate --kprl --pid 1234

# 瀹氫綅灞傦紙Invary Sensor 鍐呮牳 VA 璇?+ 杩涚▼閾捐〃锛涗笉闇€瑕佸彇鏁板眰锛?.\bin\layers.exe --locate --invary --pid 1234

# 瀹氫綅灞傝嚜鍔ㄥ洖閫€锛欿PRL -> Invary -> KKYUM -> 鐗╃悊鎵弿
.\bin\layers.exe --locate --auto --pid 1234 --reader cchwapi --auto-ranges

# Invary 鍐呮牳 VA 璇昏瘖鏂紙闇€瑕?invarysensor.sys 椹卞姩锛?.\bin\layers.exe --invary-read --va 0xFFFF800000000000 --size 16

# 鏄惧紡 CR3锛氱炕璇戝眰 + 鍙栨暟灞傦紙榛樿 4-level锛?-la57 浣跨敤 5-level/LA57 椤佃〃锛?.\bin\layers.exe --reader cchwapi --cr3 0x1A0000 --va 0x7FF600000000 --size 0x40
.\bin\layers.exe --reader mock --cr3 0x300000 --va 0x1010 --size 4 --la57

# 绂荤嚎缈昏瘧灞俹nly锛坢ock 椤佃〃锛屼笉闇€瑕侀┍鍔級
.\bin\layers.exe --reader mock --cr3 0x1000 --va 0x400000 --translate-only
.\bin\layers.exe --reader mock --cr3 0x300000 --va 0x1010 --translate-only --la57
```

