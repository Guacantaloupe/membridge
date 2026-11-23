# security_corpus2 新增 25 个驱动

## 来源
```
driver-security/0day-web-hunt/corpus
```
筛选非重复、x64、≤2MB，导入 25 个到：
```
drivers/security_corpus2/
```

代表：
```
aldwa.sys
amdacpafd / amdacpbus2 / amdsdws / amdsfhkmdf / amdusbhubfilter
AppRemover-Killer__ardrv
aqnic650
AsrNfDrv000
AVerPola / AVKsCIR / AVPolCIR / AVPolDIR
bcmnfcusb
bthpan
CcProtect-Killer__CcProtect
ch64usb / cvusbdrv
DC624e
devmap.sys
DisplayLinkUsbIo_x64
dsh_map.sys
dsh_ntbase_diag.sys
dsh_probe_s0.sys
```

注意：
- `dsh_map.sys` **NotSigned**；
- 其余签名 Valid。

## devmap 测试
`devmap.sys`：
```
STATE: RUNNING
```
设备枚举未见新的 `devmap` 设备对象。

## DSH 自定义驱动
`dsh_map` / `dsh_ntbase_diag` / `dsh_probe_s0`：
- 是项目自带的小型测试驱动；
- 没有可见的 `MmMapIoSpace`/`\Device\` 字符串；
- `dsh_map` 未签名，直接加载可能被签名策略阻止；
- 暂不加载。

## 当前累计
```
原有新增                  600
security_corpus2            25
新增合计                  625
```
