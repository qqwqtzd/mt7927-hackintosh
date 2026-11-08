# MT7922 AWDL 参考实现（第三方研究资料）

来源：GitHub 开发者分享的 `MT7922SkyWalk 2airdropdnowifi.zip`（2026-11-07，版本 1.2.158）。
本项目仅归档**关键实现与结论文档**用于研究参考；完整源码（42MB/355 文件）未纳入 git。

## 为什么这份资料重要

**它推翻了本项目此前对 AWDL 的"结构性不可修"判定。**

核心思路（非常有价值）：

```
不自己实现 Apple 私有 AWDL 协议，而是接管 IO80211Family 内置的 AWDL 引擎。
Apple 的 Family 里已经完整实现了：AWDLPeerManager / AWDLProtocol /
PeerSchedule / 主指示(MI) / TLV / 选举 / 可用窗口。

驱动只需：
 ① 创建 AWDL 虚拟接口（role6，awdl0 + fe80）
 ② 响应 MIF 命令（apple80211 selector）：
      116 SYNC_PARAMS / 117 SYNC_ENABLED / 129 CHAN_SEQUENCE
      164 PEER_TRAFFIC / 208 AF_TX_MODE
 ③ 把收到的 Action 帧交给 Family：
      IO80211AWDLPeerManager::actionFrameInput(buffer, channel, hdr, 0x48)
 ④ 上报 RF 窗口事件：
      IO80211AWDLPeerManager::reportDataPathEvents(0x29=窗口开始 / 0x2b=窗口结束)
 ⑤ 提供管理帧池：
      IO80211VirtualInterface::getMgmtFramePool()
      → IO80211MallocBufferPool::getBuffer() / IO80211MallocBuffer::writeBytes()
```

## 完成度（据其日志证据，1.2.156）

| 环节 | 状态 |
|---|---|
| awdl0 创建 + fe80 | ✅ 稳定（58/58）|
| 周期窗口 | ✅ SameChannelWindows 70→170 |
| 发现帧提交 | ✅ 3300→11840 |
| 外部 Action 接收 + handoff | ✅ serial 366→1046 |
| Peer 确认 | ✅ 48→136 |
| 主指示(MI) | ⚠️ 曾被 640B 上限拒绝（TLV 767B），已修复 |
| 数据面主机队列 | ⚠️ DataPathEnabled=Yes，入队/完成正常 |
| 数据面空中接收 | ❌ DataRxRouted=0 |
| Bonjour/sharingd 建连 | ❌ DNS Refused / Ask failed |
| **手机实际收到文件** | ❌ 未确认 |

**结论：驱动侧 AWDL 基础设施基本打通，端到端 AirDrop 尚未验证成功。**

## 归档文件

| 路径 | 内容 |
|---|---|
| `core/EngineRuntime25.hpp` | AWDL 引擎核心（96KB）：窗口调度、发现帧、MI、ROC 授权映射 |
| `core/DataPathRuntime25.hpp` | 数据面（18KB）：peer WTBL、队列、Skywalk 收发 |
| `core/WCLCoreRuntime.hpp` | WCL 请求入口（26KB）：MIF 命令处理 |
| `core/AWDLProtocol.hpp/.inc` | AWDL 协议结构/方法（Family 私有符号绑定）|
| `docs/*.zh-CN.md` | 实施与验证记录（11 篇）：启动重试、锁修复、独立模式、单向接入等 |
| `notes/*.txt` | 早期关键逆向笔记（RX handoff、系统创建、接口清单）|

## 对我们（MT7927）的可复用性

**可直接借鉴**：
- Family 私有符号调用契约（`__asm` 绑定名）—— 与我们在 REV/kernel 里看到的类一致
- MIF selector 字节布局（116/117/129/164/208）
- AWDL 接口创建条件与顺序
- Action 帧 handoff ABI（buffer 所有权语义：入队后归 Family）

**需要为 MT7927 重写**：
- 信道调度层：他们把 Family 序列映射为"有界 ROC 授权"（MT7922 的 ROC 机制）
  → MT7927 需要用我们驱动自己的 ROC/信道驻留实现
- 窗口时序（TSF/相位对齐）
- 数据面（Skywalk 队列）接入我们现有 MtkWlanNative 的数据路径

**风险**：大量依赖 Family 私有 ABI（系统大版本更新需重新核对）；数据面是公认难点（参考实现也卡在此处）。
