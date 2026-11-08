# 独立 MT7922 Core/Bus 请求边界（13.7 x86_64 研究样本）

本模块不继承 AppleBCMWLANCore/Bus，不伪装 BCM 设备；没有捏造完整 Apple 私有类声明。

`CoreBridge` 是 MediaTek 自有请求边界，`BusOperations` 是同步后端契约。Xcode target `MT7922AngusSWCore` 构建静态库；`MT7922AngusSWTests` 链接该生产静态库进行无硬件测试。

已接入实际驱动的入口为 `Mt7922zOWDriver::anguscoreRequest(Request const&)`：使用原驱动控制锁和 powerBlocked/firmwareReady/firmwareRunning/wfdmaReady 状态。它是独立的显式长度入口，不是旧 `apple80211Request` 的重解释。当前后端 AWDL 操作返回 Unsupported，未发送 MCU/RF 命令。此方法已编入开发副本 Native kext，但旧 Native target 仍依赖 Legacy，不能称为完成 modern family 迁移。

## 输入约束

| selector | 输入字节数 | 本层 SET 约束 |
|---|---:|---|
| 116 SYNC_PARAMS | 24 | 保留全部字段，不臆定单位/未知范围；sentinel 留给对应上方 manager/后端 |
| 117 SYNC_ENABLED | 12 | enabled 位于 +8；不与 BTLE wrapper 混用 |
| 129 CHAN_SEQUENCE | 400 | count@5 为 1..32，encoding@6 为 0..1；合法射频信道仍由后端核验 |
| 164 PEER_TRAFFIC | 184 | version=1、标识长度 1..128；MAC 内联于 +4，标识@0x10，末字节置 NUL |
| 208 AF_TX_MODE | 8 | mode 为 +4 的 u32；未臆造 enum 范围 |

GET 不套用 SET 的 payload 内容约束。固定容器长度在调用后端前检查；未支持 selector 不默认成功。局部对齐缓冲避免未对齐结构访问，后端失败不回写用户输出；成功允许 SET/GET 回写。Bus 回调只能同步借用缓冲，不能保留指针。调用者必须串行控制，CoreBridge 本身不宣称线程安全。beginShutdown 后拒绝后续请求。

`receiveHciEvent` 解析完整 HCI event（不含 H4 type 字节），先核对全部长度再调用 credit/completed sink。支持 0x0e、0x0f、0x13，无法替代命令发送队列、断连回收、USB/WMT/固件初始化或 LE/ISO 全状态机。

## ABI 与后端边界

布局来自研究目录 interface-contracts 的 KC/userland 两端访问证据；仅覆盖五个请求，不是全部 171 个 handler 的实现。Result 是自有状态 enum，不能直接当成 IOReturn/errno；未来 Apple SPI 适配器必须显式翻译。这里没有建立 IO80211SkywalkInterface、awdl0、事件映射、peer/调度或数据通路。

下一阶段仍需恢复/核验 modern family 的必要 C++ 类 ABI，实现真正 controller/interface/queue 适配，再把 MediaTek 第二 BSS、peer 和时钟/信道命令接入 BusOperations。未闭环部分不能通过换 plist 依赖名解决。
