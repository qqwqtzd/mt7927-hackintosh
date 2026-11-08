# 1.2.98 Wi-Fi恢复：AWDL接收mbuf链长度越界

## panic定位

用户panic中的1.2.97 UUID为F0A4B125-6B09-30E9-95E2-76F6F67D3E79，与已交付二进制完全一致。按加载基址0xffffff8019f17000还原：

- 返回地址0xffffff8019f6e10b → serviceProximityPackets+859（文件偏移0x5710b）。
- 上一层0xffffff8019f6c4ad → tick+57。
- 反汇编0x57106调用mbuf_copydata，0x5710b为其返回位置。并非猜测新授权回调就是崩溃函数。
- 系统GZAlloc报告mbuf对象512字节，读取高端越界；寄存器中的复制长度0x1a2即418字节。符合从被破坏的mbuf头段长度复制时跨越对象边界。

1.2.97使独立模式继续推进到接收，暴露了接收生产端的长度错误。之前宿主测试把mbuf模拟成单个连续数组，遗漏了系统分配多段链的情况。Wi-Fi开关反复关闭可能与panic/恢复有关，但单份panic不能证明系统保存开关状态的全部原因；普通Wi-Fi恢复仍须实机验证。

## 根因及修复

mbuf_allocpacket可以返回多个mbuf组成的包；mbuf_copyback按链写入、维护各段长度。原AWDL raw、translated及旧系统接收入口之后又调用mbuf_setlen(head,整包长度)，把总长度错误地写成第一段长度。包长超过首段可用空间时，随后mbuf_copydata按错误长度读出首段边界。

1.2.98删除这三处mbuf_setlen，仅保留包头总长度mbuf_pkthdr_setlen，尊重copyback生成的分段长度。不是把所有包强制压成单段，也不是丢掉大包。

另加可移植copyMbufChain，在进入mbuf_copydata之前验证将遍历的段：数据长度不超过实际数据指针剩余容量、链能覆盖所需包长、包头总长度相符，并限制遍历次数64以拒绝循环/异常链。普通基础RX、现代AWDL RX和旧系统AWDL RX均使用该防线。停机或数据面关闭时，AWDL队列只回收、不再复制上交。无法保证处理任意损坏指针；此修复针对有效所有权下的长度越界。

接口契约核对本机Kernel SDK sys/kpi_mbuf.h中mbuf_allocpacket、mbuf_copyback、mbuf_setlen和mbuf_pkthdr_setlen说明：分配对象可成链，单段长度不可越过可用空间，总包长属于packet header。

## 恢复版范围

版本1.2.98，profile=wifi-recovery。保留已实现的联网修复、授权前准备、跟随编码及其他功能，没有整包回档。为先验证普通Wi-Fi，Info.plist中MT7922AwdlAutoCreate暂设false；仅关闭自动注册入口，系统原有创建入口及AWDL代码仍在，因此本轮不要主动打开Finder隔空投送。后续确认普通Wi-Fi稳定后再安排AWDL测试，本轮不要求用户重新开启实验入口。

不改蓝牙、射频功率、握手带宽、基础TX或系统代理/路由。不安装驱动或改EFI。

## 验证

- 用418字节分段包（224+194）模拟旧生产端损坏，ASan稳定报告heap-buffer-overflow；修复后完整读取并核对所有字节。
- 验证损坏首段在调用copydata之前被拒绝，短链/循环拒绝；14..1518字节的分段包逐长度测试通过。
- 执行实际AWDL队列服务代码的宿主测试：启停、完成重试、停止回收和RX上交通过。该队列模型仍以连续数据模拟单个测试包；真实分段由独立链测试覆盖，不宣称宿主模型完全等价内核。
- 扫描/关联/关电保护的注册测试、关联MCU顺序检查通过；源码审计三处错误setter已移除、三个上交读取点接入链校验。
- Release x86_64编译通过。尚未在用户机器上加载；不能用离线检查保证没有其他panic或保证Wi-Fi开关状态已恢复。

## 用户本轮只验证普通Wi-Fi

替换1.2.98 kext并重启。先不要打开Finder隔空投送或运行AWDL采集。依次验证：Wi-Fi能开启并保持约30秒 → 连接熟悉网络并获取IP/打开网页 → 关闭再开启Wi-Fi → 重连/切换另一个网络。若仍会自动关闭，保留新的panic或相关系统日志，不反复触发。

修改前源码在Build/panic-mbuf-20261104/source-before；匹配的1.2.97 panic、符号定位及旧越界复现保留在该目录。旧版文件未覆盖。
