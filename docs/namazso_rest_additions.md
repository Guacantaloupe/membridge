# namazso/physmem_drivers 剩余变体导入

## 来源
从 `namazso/physmem_drivers` 拉取 137 个 `.sys` 条目，去重/过滤后新增 **21 个 x64** 驱动：

```
drivers/namazso_rest/
```

## 新增
- `ALSysIO64_7196187F...sys`
- `ALSysIO64_7F375639...sys`
- `AsIO_2DA330A2...sys`
- `AsIO_436CCAB6...sys`
- `AsIO_B4D47EA7...sys`
- `AsIO_DDE6F28B...sys`
- 15 个 `CITMDRV_AMD64_*.sys` 变体

签名均 Valid。

## 过滤
下载时还包含 19 个 IA64 驱动，已删除，因为不是当前 x64 Windows 可用格式。

## 当前累计
```
corpus_samples             13
physmem_drivers            13
physmem_drivers_variants   29
import_candidates          12
namazso_extra               2
byov_input                 64
security_candidates         8
namazso_rest               21
新增合计                  162
```

## 下一步
1. 对 6 个 `AsIO` / `ALSysIO64` 变体做加载测试；
2. `CITMDRV` 变体按版本抽样测试；
3. 注意不要批量卸载；
4. 继续找更多驱动，不停止。
