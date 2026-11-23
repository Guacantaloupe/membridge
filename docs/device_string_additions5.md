# device_string 最终补充（19 个）

## 新增
### DAQ 剩余变体（18）
```
Daq2006(1/2/3)
Daq2005(2/3/7)
Daq2204(2/7) / Daq2204
Daq2205 / Daq2206(1) / Daq2206
Daq2208(2)
Daq2016(2)
Daq2502(1/2/4/5)
```

### 磁盘过滤（1）
```
dlpfde.sys  -> \Device\Harddisk
```

签名均 Valid。

## 安全
- `dlpfde.sys` 有磁盘过滤特征，**不加载**；
- DAQ 系列需要真实硬件，暂不加载。

## 当前 device_string 候选
```
drivers/device_string_candidates/  (69 files)
```

## 当前累计
```
原有新增                  466
本轮新增                   19
新增合计                  485
```
