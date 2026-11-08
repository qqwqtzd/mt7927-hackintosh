# 单向隔空投送接入实施安排（2026-11-04）

用户确认1.2.92联网恢复。下一目标为Mac发现iPhone并发送文件，暂不修改第三方蓝牙。以原生Finder/sharingd作为AirDrop上层，驱动负责AWDL无线与系统网络接入，不另建自定义文件传输协议。

Apple分别描述了AirDrop的BLE/对等Wi-Fi及Instant Hotspot的BLE发现/热点连接信息交换。因此“双侧开启蓝牙和Wi-Fi后热点行为改变”不能作为AWDL已接入的证据。参考：[AirDrop](https://support.apple.com/en-ca/guide/security/sec2261183f4/web)、[Instant Hotspot](https://support.apple.com/en-euro/guide/security/seca4b33e8c9/web)。蓝牙不改不代表BLE步骤已成功；第一轮应从日志确认Mac侧是否能进入发送端发现流程。

## 当前源码能做什么

系统role6创建/enable/disable、AWDL配置槽、模板/序列、周期窗口、发现帧发送、Action交给Family、peer查询和AWDL数据面已有实现。功能开关见Compatibility/NetworkPolicy.hpp，原生周期引擎与被关闭的实验async scheduler是不同入口。老STATUS/AIRDROP_PLAN中的历史“尚无自动创建”等描述不能直接当作1.2.92现状。

当前不能从源码或离线测试宣布：iPhone已识别这些帧、peer是目标iPhone、IPv6可双向通讯、Bonjour/系统认证完成、文件能收取。这些是接入验收点。

## 分层推进及修改落点

| 阶段 | 所需证据 | 失败后修改位置 |
| --- | --- | --- |
| 发送端请求 | Finder/sharingd开始发现；BLE拒绝与AWDL请求分别记录 | 系统接入/WCL请求入口，暂保留蓝牙驱动 |
| 原生接口 | role6请求、真实awdl0注册/UP、实际地址 | WCLCoreRuntime.hpp与ModernCore.cpp，核对目标OS ABI |
| 同步和窗口 | 模板/序列就绪、grant、发现帧计数增长、退出/恢复 | EngineRuntime25.hpp与无线会话所有权 |
| 发现对端 | 外部Action接收增长、Family消费、目标peer身份 | Action元数据/缓冲所有权/选举及PeerSchedule |
| 数据链路 | 对端同信道时隙、peer WTBL、awdl0 fe80、ND/双向数据 | DataPathRuntime25.hpp、peer生命周期及Skywalk收发 |
| 原生传输 | Bonjour发现/解析、sharingd发起连接、iPhone接受并打开文件 | 先排查网络数据面，再定位服务/系统认证；不伪造成功 |

优先使用系统触发的自然调用，不依赖旧proximity探针强行写状态。若启动在基础网络优先期内被拒绝，必须记录是临时busy还是实际grant失败；之后实现有生命周期取消和超时的延迟启动，不能直接返回假成功。1.2.92握手独占和DHCP五秒优先保持，任何新实现都要回归网页/扫描/重连。

## 第一轮交付与验收

Source/Tools/testing/awdl_oneway提供一次120秒macOS接入采集、带P2P的Bonjour浏览、人工阶段标记与测试文件。使用已稳定的1.2.92，不为采集改kext/蓝牙。结果用本次计数增量区分历史残留，接口存在和TX排队均不冒充设备发现/文件接收。

收到第一轮采集后按最早缺失的阶段实现下一条路径：无role6先修系统创建；无窗口看模板/序列/拒绝；有TX无外部RX看调度和固件交付；有RX无peer看handoff；有peer无IPv6数据看数据面；到Bonjour后再看sharingd连接。可并行传输阶段先用小文件，再大文件与重复测试。

“手机收到文件”的用户标记保留为人工证据，不做自动验真；基准测试文件SHA256随测试包提供，可进一步核验文件内容。第一阶段未实机执行AWDL接入，也未宣称隔空投送完成。
