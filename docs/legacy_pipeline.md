# 24 涓?legacy backend 鎺ュ叆銆孋R3 瀹氫綅 + 椤佃〃缈昏瘧 + 鐗╃悊鍙栨暟銆?
鐩爣锛氫笉閲嶅啓 24 涓?legacy 椹卞姩鍗忚锛屽厛璁?**`membridge.exe` 鐨?24 涓?backend 缁熶竴鍏呭綋鍙栨暟灞?*锛屽啀閰嶅悎 `layers.exe` 鐨勫畾浣嶅眰/缈昏瘧灞傝鍙栦换鎰忚繘绋嬶紙濡?cmd.exe锛夌殑鐢ㄦ埛 VA銆?
## 1. 鏋舵瀯

```
                  鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?cmd.exe PID 鈹€鈹€鈹€鈹€鈹€鈻衡攤 瀹氫綅灞? KPRL(0x239584) / KKYUM / Invary  鈹傗攢鈹€鈻?CR3
                  鈹?鎴栫墿鐞嗘壂鎻?EPROCESS (闇€瑕佸彇鏁板眰)          鈹?                  鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                      鈹?CR3
                                      鈻?                  鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?cmd.exe VA 鈹€鈹€鈹€鈹€鈹€鈹€鈻衡攤 缈昏瘧灞? 4-level / 5-level page walk       鈹傗攢鈹€鈻?PA
                  鈹?姣忔璇婚〉琛ㄩ」閮介€氳繃鍙栨暟灞傝鍙?8 瀛楄妭       鈹?                  鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                      鈹?PA
                                      鈻?                  鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                  鈹?鍙栨暟灞? legacy bridge                     鈹?                  鈹?layers --reader legacy --legacy-backend X 鈹?                  鈹?  鈫?spawn membridge.exe --backend X       鈹?                  鈹?    --pa PA --size N --out-file temp      鈹?                  鈹?  鈫?璇诲洖 temp 瀛楄妭                        鈹?                  鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

`layers.exe --reader legacy` 鏄竴涓?*鍏?provider**锛氬畠鏈韩涓嶆墦寮€璁惧锛岃€屾槸鎶婃瘡娆＄墿鐞嗚杞彂缁?`membridge.exe --backend <id>`銆傚洜姝?24 涓?legacy backend 绔嬪埢鍏峰涓夊眰鑳藉姏锛屾棤闇€鎶?24 濂楀崗璁Щ妞嶅埌 `layers.c`銆?
**璇诲彇鍒嗗潡**锛歚membridge.exe --size` 涓婇檺鏄?`0x10000`锛岃€岀墿鐞嗘壂鎻?locator 姣忔璇?1MB锛沚ridge 鑷姩鎶婂ぇ璇绘媶鎴?64KB 鍧楋紝淇濊瘉瀹氫綅灞傜殑 1MB 鎵弿涔熻兘璧?legacy backend銆?
## 2. 24 涓?backend

`layers.exe --list-legacy` 鎵撳嵃鍏ㄩ儴 24 涓?backend銆侀┍鍔ㄨ矾寰勫拰 HVCI 瀹¤锛?
- `cchwapi_2_9` / `cchwapi_2_12` / `cchwapi_ext_4_5` / `hp_swtools` / `swtoolsdriver` / `hp_swtools_v1` / `isharepp` / `gibepext` / `gibepext2` / `tdeio` / `mrt` / `ufu_win10` / `winring0x64`锛欻VCI PASS 瀛愰泦锛?- 鍏朵綑涓?FAIL/WARN锛堣瑙?`docs/drivers_inventory.md` 涓?`evidence/hvci_driver_report.json`锛夛紱
- `ufu_win10` 浠?32-bit PA锛沗winring0x64` 浠呬綆鍐呭瓨銆?
## 3. 鐢ㄦ硶

### 3.0 20 涓?provider 鑷姩鎵弿鎵?CR3
鏂板 `--locate --scan-providers`锛?```powershell
.\bin\layers.exe --locate --scan-providers --pid <cmd_pid> --name cmd.exe --auto-ranges
# 鎵惧埌 CR3 鍚庡彲鐩存帴缁х画璇?VA锛?.\bin\layers.exe --locate --scan-providers --pid <cmd_pid> --name cmd.exe --auto-ranges --va <va> --size 0x40
```
渚濇灏濊瘯 20 涓彇鏁?provider锛涙瘡涓?provider 鐨勭墿鐞嗚浼氭寜 64KB/4KB 鑷姩闄嶇骇锛屾渶鍚庡彲鐢?`--legacy-backend` 鎸囧畾鐨?legacy backend 浣滀负 fallback銆?
甯﹁€楁椂缁熻锛?```powershell
.\bin\layers.exe --locate --scan-bench --pid <cmd_pid> --name cmd.exe --scan-start 0x100000 --scan-size 0x100000
pwsh -File scripts\bench_provider_scan.ps1 -ProcessId <cmd_pid> -AutoRanges
```
姣忎釜 provider 鎵撳嵃 `open=... ms scan=... ms result=...`銆?
### 3.1 鐗╃悊璇伙紙鍙栨暟灞傚崟鐙獙璇侊級
```powershell
.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 `
    --read-phys --pa 0x10000000 --size 0x40 --dump
```

### 3.2 CR3 + 椤佃〃缈昏瘧 + 鍙栨暟锛堣 cmd.exe VA锛?```powershell
# 1) 閫氳繃 KPRL/KKYUM/Invary 瀹氫綅 cmd.exe 鐨?CR3锛堜笉闇€瑕?legacy 鍙栨暟灞傦級
.\bin\layers.exe --locate --kprl --pid <cmd_pid>
# 鎴?.\bin\layers.exe --locate --kkyum --pid <cmd_pid>
# 鎴?.\bin\layers.exe --locate --invary --pid <cmd_pid>

# 2) 鐢ㄦ煇涓?legacy backend 鍋氱墿鐞嗗彇鏁帮紝缈昏瘧骞惰鍙?.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 `
    --cr3 <CR3> --va <cmd_va> --size 0x40 --dump

# 3) 涔熷彲鐢?physical scan 瀹氫綅锛堣繖鏃跺彇鏁板眰鐢?legacy backend锛?.\bin\layers.exe --reader legacy --legacy-backend cchwapi_2_12 `
    --locate --pid <cmd_pid> --auto-ranges
```

### 3.3 甯哥敤閫夐」
- `--legacy-bin <path>`锛氭寚瀹?`membridge.exe`锛堥粯璁ゅ悓鐩綍 `membridge.exe`锛夛紱
- `--legacy-no-load`锛氫笉缁欏瓙杩涚▼浼?`--no-load` 鏃朵笉鍔犺鍙傛暟锛涢┍鍔ㄥ凡鍔犺浇鏃跺缓璁姞锛岄伩鍏嶆瘡娆″皾璇曞姞杞斤紱
- `MB_LEGACY_DEBUG=1`锛氳緭鍑哄瓙杩涚▼鍛戒护/閫€鍑虹爜锛屼究浜庤皟璇曘€?
## 4. 楠岃瘉锛堢绾匡紝涓嶉渶瑕佺湡瀹為┍鍔級

浠撳簱鑷甫鍙鐜版祴璇曪細
```powershell
pwsh -File scripts\test_legacy_pipeline.ps1
```
`test/fake_legacy_mem.c` 鏋勯€犱竴涓悎鎴愮墿鐞嗗唴瀛橈紙EPROCESS + 4 绾ч〉琛?+ 鏁版嵁锛夛紝娴嬭瘯锛?
1. `--reader legacy --cr3 0x200000 --va 0x400000 --size 8` 鈫?`PA=0x300000`銆乣4C 45 47 41...`銆丳ASS锛?2. `--reader legacy --locate --pid 4242 --name cmd.exe --scan-start 0x100000 --scan-size 0x100000 --va 0x400000 --size 8` 鈫?**瀹氫綅灞傛壂鎻忓埌 EPROCESS_PA=0x100000銆丆R3=0x200000**锛岀炕璇?鍙栨暟 PASS锛?3. 1MB 鎵弿鍒嗗潡娴嬭瘯锛坆ridge 鎶?1MB 鎷嗘垚 64KB 鍧楋級锛孭ASS锛?4. `--locate --scan-providers` + legacy fallback锛氬皾璇?20 涓?provider 鍚庣敱 `legacy:cchwapi_2_12` 瀹氫綅鍒?CR3 骞惰 VA锛孭ASS銆?
瀹炴祴杈撳嚭锛?```
[OK] 瀹氫綅灞?scan: PID=4242 EPROCESS_PA=0x100000 CR3=0x200000
[OK] 缈昏瘧+鍙栨暟 VA=0x400000 -> 8 bytes
4C 45 47 41 43 59 2D 43
VERDICT=PASS
LEGACY_PIPELINE_TEST=PASS
```
璇佹槑 **24 legacy backend 鈫?瀹氫綅灞傦紙鐗╃悊鎵弿锛夆啋 缈昏瘧灞?鈫?鍙栨暟灞?* 鐨勫畬鏁撮摼璺凡鎵撻€氾紱鐪熷疄椹卞姩鍔犺浇/璇诲彇浠嶉渶绠＄悊鍛樼幆澧冦€?
`layers.exe --selftest-mock`锛?*46/46 PASS**锛堟柊澧?legacy registry 妫€鏌ワ級銆?
## 5. 闄愬埗涓庝笅涓€姝?- **鎬ц兘**锛氬綋鍓嶆槸姣忎釜鐗╃悊璇讳竴娆″瓙杩涚▼锛岃 4KB 椤电害闇€涓€娆¤繘绋嬪惎鍔紱閫傚悎楠岃瘉锛屼笉閫傚悎澶ф壒閲忔壂鎻忋€備笅涓€姝ュ彲浠ュ湪 `membridge.exe` 澧炲姞闀胯繛鎺?鎵归噺杈撳嚭锛屾垨鎶?24 濂楀崗璁Щ妞嶄负 in-process provider銆?- **绠＄悊鍛?*锛氱湡瀹炶鍙栦粛闇€瑕佺鐞嗗憳鍔犺浇椹卞姩锛汬VCI 涓嬩粎 PASS backend 鍙姞杞姐€?- **blocklist**锛欻VCI 鏈哄櫒鐨?blocklist 榛樿绛栫暐鍙兘鎷︽埅宸茬煡 BYOD hash锛岄渶鎸?`--list-legacy` 鐨?PASS/瀹為檯鍔犺浇缁撴灉閫夋嫨銆?- **涓嬩竴姝?*锛氫负 `membridge.exe` 澧炲姞鎸佷箙 server 妯″紡锛堜竴娆℃墦寮€璁惧銆佸娆¤锛変互鍑忓皯杩涚▼鍚姩寮€閿€锛涘啀瀹炵幇 `legacy` provider 鐨勬寔涔呰繛鎺ャ€?
