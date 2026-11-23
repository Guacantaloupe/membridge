# DSH 自定义物理读驱动（poc-package/build）

## 文件
```
poc-package/build/dsh_read_s3_cpp.sys
```
复制到：
```
drivers/dsh_custom/dsh_read_s3_cpp.sys
```

## 签名
```
Status: Valid
Signer: CN=DSH Test Driver
```

## 加载
```
sc create MbDshRead ...
sc start MbDshRead
=> exit=577 (0x241)
STATE: STOPPED
```

## 结论
- 该驱动由 **DSH Test Driver** 测试证书签名；
- 内核签名策略仍拒绝加载（不是 Microsoft 信任链/WHQL）；
- 不能直接作为 BYOVD 使用；
- 只有开启 `testsigning` 或禁用签名强制才可能加载，但这会改变系统安全配置，不执行。

## 当前状态
- 最新 dump：`112326-41453-01.dmp`
- 无新 BSOD
