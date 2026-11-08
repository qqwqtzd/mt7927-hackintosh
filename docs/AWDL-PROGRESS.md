# AWDL 移植进度与 P2 实施计划

> 起点：`REVERSE-PATH-ANALYSIS` 曾判定 AWDL"结构性不可修"。
> 2026-11-08 获第三方 MT7922 参考实现证明可行，本项目开始移植。
> 状态：**接口层与硬件能力层已完整打通**（8+2 步），引擎层待实现。

## 已完成（2026-11-08）

| # | 内容 | 证据 |
|---|---|---|
| 1 | 分析参考实现：复用 IO80211Family 内置 AWDL 引擎 | `REV/reference/mt7922-awdl/` |
| 2 | 放行 selector 94 (VIRTUAL_IF_CREATE) | — |
| 3 | role-6 接口创建（init/attach/start），Family 接受 | `MtkAwdlCreate=published` |
| 4 | role 属性（`IO80211VirtualInterfaceRole=AWDL`、`IO80211InterfaceRole=AirLink`）| ioreg |
| 5 | 16 个 `_RESERVED` 槽本地空实现（OC vtable patch 必需）| OC 日志无报错 |
| 6 | Skywalk pool/queues | `MtkAwdlPool=ok` |
| 7 | 发布序列 `registerService(2)` + `setRunningState(false)` + `setLinkState(2)` | `MtkAwdlPublished=done` |
| 8 | `IO80211Controller::attachInterface` | `MtkAwdlAttachFamily=ok` |
| 9 | **`registerEthernetInterface`（真正创建 BSD 接口）** | **`en3` + fe80 出现** |
| 10 | **P1 硬件探测：ROC 驻留 AWDL ch6 + 发 Action 帧** | `MtkAwdlProbeRoc=0`, `ProbeTx=1`, `ProbeAbort=0` |
| 11 | 启动优化：AWDL 创建延迟 5s→60s（修复 3 分钟启动延迟）| 启动恢复正常 |
| 12 | AWDL 能力位宣告（capabilities[4] bit0）| — |

## 当前观测

```
Mac 能看到 iPhone   ✅（收方向：外部 AWDL Action 帧）
iPhone 看不到 Mac   ❌（发方向：需要真正的 AWDL 发现帧 + 窗口调度）
文件传输            ❌（需要数据面）
```

## P2 实施计划（参考实现对应位置）

### P2-A：AWDL selector 响应（Requests 层）
参考：`AngusCore/Native/Requests.cpp:989 awd1Request`
```
117 AWDL_SYNC_ENABLED   get → {version=1, ?, enabled=0}；set 记录 enabled
其余 selector (116,118-149) → ConfigurationState 存储/回显（不驱动无线电）
151 AWDL_SYNC_FRAME_TEMPLATE → 捕获 Family 下发的模板（nativeTemplateBytes）
```
注入点：我们的 `handleCardSpecific`（AWDL 命令目前返回 ENOTSUP）。

### P2-B：模板捕获（关键）
参考：`Requests.cpp:664-666`
```c
if (type==0x803069c8 && number==151 && nativeTemplateCaptureActive)
    captureNativeTemplate(data);
```
需要 `nativeTemplateCaptureActive` 由驱动在自己的 gate 内发起。

### P2-C：发现帧构建与发送
参考：`AngusCore/Native/DiscoveryRuntime.hpp`
```
buildDiscovery(ctx, timing, template, payload, ...) → frame
在 AWDL 窗口内 txMgmt 发送（P1 已验证可行）
```

### P2-D：窗口调度（周期）
参考：`AngusCore/Native/EngineRuntime25.hpp`（96KB）
```
周期 ROC（~100ms 窗口）→ 窗口内发/收 → 窗口外回到 AP 信道
关键：不打断 STA（参考实现用 ROC + 时间片）
```

### P2-E：数据面（AirDrop 文件传输）
参考：`AngusCore/Native/DataPathRuntime25.hpp`（18KB）
```
peer WTBL、IPv6 数据、Skywalk 收发
（参考实现也未完全打通，DataRxRouted=0）
```

## 工作量与风险

| 阶段 | 估计 | 风险 |
|---|---|---|
| P2-A/B | 1-2 周 | 低（纯协议/存储）|
| P2-C | 1 周 | 中（帧格式需对照参考）|
| P2-D | 1-2 周 | 中高（信道调度影响 STA）|
| P2-E | 2-4 周 | 高（双方都卡在此）|

## 已完成资产

- 驱动：`MtkWlanNative`（AWDL 接口）+ `MtkWlan`（ROC 探测）
- 工具：`~/rea-tools/scripts/install-kext.sh`（内容探测装机）、`install-native.sh`
- 参考实现：`REV/reference/mt7922-awdl/`（核心源码 + 11 篇实施记录）
- 文档：本文件 + `REVERSE-PATH-ANALYSIS` §17

## 下一步（下次会话）

1. 实现 P2-A（117/151 + ConfigurationState）
2. 捕获模板后实现 P2-C（用模板构建发现帧，在 P1 的窗口里发）
3. 验证 iPhone 能否看到 Mac（这是最直接的里程碑）
