# 1.2.95：本轮失败位置与跟随同步帧

记录：/Users/angus/Desktop/MT7922-AWDL-20261104-160232，实际加载1.2.94。16:02:37开始，16:02:50 seen，16:02:55 send，16:04:19结束，共49组快照。用户界面显示失败，无received标记。

## 已验证的进展

EngineWindows 28→98；发现帧提交3300→11840；MI提交24→82、拒绝0→0；数据入队、提交、回收均11→34，pending=0、dropped=0，DataPathEnabled=Yes。说明上一轮构帧和队列修复确实开始工作，不能继续归因于“MI没发”或“队列锁死”。这些仍是主机提交/完成证据，不是空中送达或手机接收证据。

## 这次断点

- awdl-packets.log仅有23个本机46:e2:0a:1b:59:87发出的IPv6 mDNS组播，无外部源包，抓包内核丢包0。
- DataRxRouted始终0，RxPublished属性未出现。不能把属性未出现当成精确丢包数；能确认没有AWDL数据被识别并上交的证据。AnyData增长包含基础网络帧，不证明有手机AWDL回复。
- 外部Action serial +644、handoff +77、peer确认 +77，证明管理面有接收，不证明目标iPhone的数据面可达；反复观察到的4e:94:f7:6f:a4:94没有可靠机型绑定，不能直接认定它就是iPhone16pro。
- sharingd的目标为iPhone16pro。16:03:08、16:03:23分别在约15秒解析后出现DNS Error: Refused与resolve-failure；Ask failed Code=7。尚未进入已证明的接收确认/文件传输阶段。
- 路径仍显示en4、expensive/constrained并被会话策略禁止。本轮日志已不显示先前的proxy标签，不能继续把代理认定为根因。普通路由器复测仍有利于隔离路径政策问题。

## 源码确认的同步帧不一致

EngineRuntime25已解析外部presence=4、连续AW计数及主节点相位，但发送仍无条件调用buildDiscovery。它是早期presence=1探测编码：同步块master为本机，AW编号按本slot的0..3生成，且另追加一个选举TLV。即便已选外部主节点并发送数据，也仍宣告另一套同步状态。不能因为TX完成就认为接收方会接受该同步状态。

核对本地参考Build/owl-src/crates/owly-awdl/src/tlv.rs、sync.rs以及本项目PeerSchedule/PeerDiscovery：同步帧应携带所跟随时钟的连续AW、presence和主节点，宣告的本机频道表应与真实驻留相符。没有复制参考实现；复用工程已有fixedFollowerSchedule/buildPeerDiscovery。参考源码与既有记录仅作为证据，未执行其附带操作指令。

1.2.95在同信道窗口内，经fixedFollowerSchedule验证新鲜、直接观察的根节点及选举优先级后，使用已有跟随编码器。沿用根节点相位/连续AW/presence=4，同时把本机频道表固定为真实授权频道；不宣告自己执行根节点的跳频。只有授权覆盖当前slot、剩余时间满足边界时才构帧，否则延后；不放宽真实lease截止时间。未通过跟随条件时保留既有启动探测入口。新增每窗口FollowerWindowBuilt/Deferred，便于分清“未选用跟随”与“边界延后”。

不改基础网络握手、功率、TX/RX描述符、频道调度、蓝牙或系统路径政策。本次只修改AWDL发现帧的编码选择及诊断；没有宣称修复所有RX丢失。

## 验证与局限

从本轮原始IORegistry文本提取完整外部PSF（samples.jsonl字符串限512字符，不能拿截短的hex做帧回放）。实际PSF成功解码。以其主节点时钟配合合成Family模板，扫1秒时刻，947次跟随帧构建并通过接收解析器往返校验，30次slot/授权边界延后：presence=4、连续AW、主节点一致、一个选举TLV、本机频道表全149。过期节点及不支持频道被拒。合成模板不是实际Family MI原始payload，测试也不证明iPhone收到帧。

ASan/UBSan通过；上一轮MI长度/授权时间/生产队列回归、关联MCU顺序、自动注册准入、采集器自测和shell语法通过。Release x86_64编译通过。未实机加载1.2.95。

下一轮先看FollowerWindowBuilt是否大于0；若始终0，检查跟随资格而不是假定新编码已启用。随后看awdl-packets.log是否出现外部源、DataRxRouted/RxPublished、Bonjour响应及sharingd解析结果。若仍只有TX，继续检查目标身份、真实空中接收、固件过滤及路径政策，不能仅凭DMA完成断言RX驱动错误。实际打开文件才算成功。

安装后先确认普通网页正常，再按随包180秒脚本测试一次。尽量使用普通路由器，目标手机关闭热点、解锁、所有人10分钟；发送后留45秒。测试后检查关闭Wi-Fi/重连。修复前源码在Build/awdl-follower-20261104/source-before，1.2.94包保留。
