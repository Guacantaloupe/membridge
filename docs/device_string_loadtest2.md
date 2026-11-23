# device_string_candidates 第二批：Pci9524 可加载，SDDisk2k 卡死整机

## 测试顺序
```
Pci9524(14).sys
SDDisk2k.sys
flash.sys
```

## 结果
### Pci9524(14)
```
STATE: RUNNING
```
加载成功。

### SDDisk2k.sys
执行 `sc start` 后命令长时间不返回，整机随后重启：
- 启动时间从 11:28:26 变为 11:32:07；
- 没有新增 minidump；
- 属于**驱动启动/挂起导致系统重启**，不是标准 BSOD。

### flash.sys
尚未测试。

## 结论
- `Pci9524` 可加载，可继续做设备枚举；
- **`SDDisk2k` 永久排除**：启动挂起/系统重启；
- `flash.sys` 暂缓。

## 安全规则更新
1. 有磁盘过滤/卷过滤特征的驱动先静态分析；
2. 每次只加载一个；
3. 启动超过数秒不返回立即视为危险；
4. 记录启动时间/重启时间，区分“挂起重启”与 BSOD。
