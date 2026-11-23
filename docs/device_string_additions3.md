# device_string 候选继续补入（DAQ 系列）

## 新增 10 个
```
Daq2213(4).sys
Daq2213.sys
Daq2204(4).sys
Daq2204(5).sys
Daq2204(1).sys
Daq2010(4).sys
Daq2010(6).sys
Daq2010(5).sys
Daq2205(6).sys
Daq2206(5).sys
```
签名 Valid。

## 特征
```
\Device\DAQ2213
\Device\DAQ2204
\Device\DAQ2010
\Device\DAQ2205
\Device\DAQ2206
```

这些是数据采集卡驱动，通常需要实际 DAQ 硬件才会创建设备对象。

## 当前 device_string 候选
共 **30 个**。

## 备注
- 之前 `Daq2208(4)` 可加载但无设备，符合“缺硬件”判断；
- DAQ 系列不优先继续测；
- 主要价值是扩充候选库，而不是当前真机可读路径。

## 当前累计
```
原有新增                  209
device_string_candidates    30
新增合计                  239
```
