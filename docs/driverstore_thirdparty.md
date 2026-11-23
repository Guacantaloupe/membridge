# DriverStore 第三方驱动导入（30 个）

## 新来源
```
C:\Windows\System32\DriverStore\FileRepository
```
筛选：
- 非 Microsoft；
- 项目中不存在；
- 签名 Valid。

共发现 **154 个**第三方驱动。

## 本轮导入 30 个
目录：
```
drivers/driverstore_thirdparty/
```

代表：
```
amdacpksl.sys
AsusConsumerDevFilter.sys / AsusDigiFilter / AsusKeyboardFilter
AsusVirtualDevice / AsusWmiAcpi
athw8x / athwnx
ax88179_178a / ax88772
b44amd64 / b57nd60a
bcmpciedhd63 / BCMWL63a
bertreader.sys
bsitf.sys
btfilter.sys
bxnd0a / bxnd60a
cht4nx64.sys
dptf_acpi / dptf_cpu
e1e6032e.sys
...
```
签名均 Valid。

## 说明
- DriverStore 中的驱动是**安装包缓存**，不一定可再次作为独立服务加载；
- 很多是 PnP/网络/ACPI 功能驱动；
- 需要按设备名/IOCTL 静态分析。

## 当前累计
```
原有新增                  349
driverstore_thirdparty       30
新增合计                  379
```

## 下一步
1. 对 DriverStore 剩余 124 个继续筛选；
2. 优先 Intel BERT Reader / ASUS ACPI / 网络类；
3. 新候选仍必须用 CR3 页交叉验证。
## 第二批导入（30 个候选，净值 29）
```
E1G6032E.sys
e1i68x64.sys
e1y60x64.sys
e2f68.sys
e2xw10x64.sys
eFE5b32e.sys
esif_lf.sys
ETDI2C.sys
fetn63a.sys
getn63a.sys
gna.sys
iaLPSS2_GPIO2_TGL.sys
iaLPSS2_I2C_TGL.sys
iaLPSS2_SPI_TGL.sys
iaLPSS2_UART2_TGL.sys
ibtusb.sys
IntcAudioBus.sys
IntcBTAu.sys
IntcDAud.sys
IntcDMic.sys
IntcSDW.sys
IntcSdwBus.sys
IntcSST.sys
IntcUSB.sys
ipoib6x.sys
ixe60x64.sys
k57nd60a.sys
l160x64.sys
L1C63x64.sys
```
签名 Valid。

## 大文件处理
`igdkmdn64.sys` 为 Intel 显卡驱动，大小约 52MB，仅作候选库价值低，已删除，不纳入仓库。

## 当前 DriverStore 目录
```
drivers/driverstore_thirdparty/  (56 files)
```

## 累计
```
原有新增                  349
driverstore 第一批            30
driverstore 第二批净值        29
新增合计                  408
```
## 第三批：30 个 ≤2MB DriverStore 驱动
为避免仓库被几百 MB 网卡/显卡驱动撑大，本轮只导入 ≤2MB 的候选：
```
GSCAuxDriverx64.sys
L1E62x64.sys / l260x64.sys
lan7400/7500/7800/9500-x64-n650*.sys
libusb0.sys
mlx4eth63.sys
mrvlpcie8897.sys
NETJME.sys
netr7364.sys
npcap.sys
nvm60x64.sys / nvm62x64.sys
ocnd65.sys
rt640x64.sys / rtcx21x64.sys
RTL8187B / RTL8187Se / rtl8192se / rtl819xp / RTL85n64
Rtnic64.sys
rtu53cx21x64 / rtu53cx22x64 / rtucx21x64 / rtump64x64 / rtux64w10
rtwlanu_81xx.sys
```
签名均 Valid。

## 累计
```
原有新增                  408
第三批 DriverStore            30
新增合计                  438
```
剩余大量 >2MB 的网卡/显卡驱动未导入，避免仓库膨胀。
## 第四批：剩余 ≤5MB DriverStore（净值 28）
```
mlx5.sys
netr28ux / netr28x
Netwbw02 / NETwew00 / NETwew01 / Netwtw10
Qcamain10x64 / qcwlan64
rtwlane_13 / rtwlanu_oldIC
SiSG664.sys
tap0901 / tap-quickfox
TeeDriverGSCW8x64 / TeeDriverW10x64
v1q63x64.sys
vmnet / vmnetadapter / vmnetbridge / vmxnet3
VSTAZL6 / VSTBS26 / VSTBS36 / VSTCNXT6 / VSTDPV6
yk63x64 / ykinx64
```
签名均 Valid。

剩余均为 >5MB 的大型网卡/显卡驱动，不导入。

## 累计
```
原有新增                  438
第四批 DriverStore 净值       28
新增合计                  466
```
