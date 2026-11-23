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
