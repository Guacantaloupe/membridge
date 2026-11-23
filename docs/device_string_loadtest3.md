# device_string_candidates 第三批：Pci9524 当前启动失败，再补 10 个候选

## Pci9524 复测
重启后重新加载：
```
sc start MbPci9524
=> start=2
=> STATE: STOPPED
```
说明该驱动在当前环境不再可启动（可能依赖硬件/前次状态），暂不作为可用驱动。

## 新增 10 个候选
从 device-string 候选列表继续补入：
```
Pci9524(11)/(4)/(9).sys
prot_2k.sys / (1) / (3) / (5)
TdkLib64.sys
Daq2208(4).sys
Daq2208(6).sys
```

特征：
```
Pci9524    -> \Device\PCIe9524
prot_2k    -> \??\Global
TdkLib64   -> \Device\TdkLib / \DosDevices\TdkLib
Daq2208    -> \Device\DAQ2208
```

签名均 Valid。

## 当前 device_string 候选
```
drivers/device_string_candidates/
```
共 20 个。

## 结论
- Pci9524 当前不可启动；
- TdkLib64 / DAQ 系列可能更依赖具体硬件；
- 下一步优先测试 `TdkLib64`，其次 `Daq2208`；
- 继续避免 Dcpp2k / SDDisk2k。
