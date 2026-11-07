# AWDL 实际状态研究报告（AWDL-CONTINUITY.md）

> 起因：观察到 iPhone ↔ Mac **通用剪贴板（文字/图片/视频）与网页接力（Handoff）正常**，
> 但驱动未实现 AWDL。本报告判定这些功能实际走哪条路径，并评估 AirDrop 可行性。
> 状态：**研究完成，未实施**（延续"AWDL 只分析不做"决策，除非后续另行决定）。

## 一、结论（一句话）

**AWDL 未激活。** 剪贴板/接力正常 = **iCloud Continuity 兜底路径**（macOS 12+/iOS 15+ 官方机制：
同一 Apple ID 且联网时，Handoff/剪贴板无需 AWDL 即可经 iCloud 转发）。
**AirDrop 是唯一无法被 iCloud 兜底的功能**（必须 AWDL 直连），当前不可用。

## 二、证据链（2026-11-07 本机采集）

| # | 检查 | 结果 | 含义 |
|---|---|---|---|
| 1 | `ifconfig -a` 有 awdl0？ | **无** | AWDL 网络栈未创建 |
| 2 | IO80211 接口树含 awdl 实体？ | **0 处** | 驱动侧无 AWDL 实体 |
| 3 | rapportd 近 30 天出现 "awdl"？ | **0 条** | Continuity 守护进程从不走 Wi-Fi AWDL 路径 |
| 4 | remoted / pboard 运行中？ | ✅ 都在 | 剪贴板同步服务正常（iCloud 路径载体）|
| 5 | iCloud 登录？ | ✅ linkstam@gmail.com | 兜底路径的前提满足 |
| 6 | sharingd AirDrop 配置 | ✅ Hash/Random UUID/DiscoverableMode 齐全 | AirDrop 发现框架在跑（但无 AWDL 载体）|
| 7 | 我们前端被拒 ioctl 直方图 | 1025 次拒绝（含 0x0161 高发 804 次）| 系统层曾尝试大量非白名单 ioctl（AWDL/WCL 家族），被拒后静默降级 |

## 三、路径判定

```
iPhone --(BLE 发现信标,可选)--> Mac
         \                    /
          \___ iCloud 转发 ___/   ← 实际路径（剪贴板/接力）
         AWDL 直连（未激活）      ← 仅 AirDrop 需要
```

- **为什么体验还那么好**：剪贴板/接力的 iCloud 路径延迟在 1-3 秒内，日常几乎无感；
  跨网络可用（不要求同一局域网）。
- **代价**：大文件直传（AirDrop）、离线直连场景不可用。

## 四、实测验证（2026-11-07，用户操作）

> **结果：能检测到 iPhone，但无法正常传输文件。** 这给出了 AirDrop 三阶段的精确判定：

| 阶段 | 状态 | 依据 |
|---|---|---|
| 发现（BLE 信标） | ✅ 工作 | 能看到 iPhone（AirDrop 发现走蓝牙 BLE 广播 + 密文 hash 交换，**不依赖 AWDL**）|
| 会话建立（链路本地 TCP）| ✅ 工作 | rapportd 捕获到与对端 `fe80:8::cdc:2b79:504d:3fb7` 的 **ESTABLISHED** 连接（49158 端口）——经普通 Wi-Fi 局域网的链路本地 IPv6 |
| 文件传输（数据通道）| ❌ 失败 | 数据传输绑定 **awdl0**（Apple 安全/性能设计，即使同网 Wi-Fi 也不走基础设施模式）；无 awdl0 → 传输无法进行 |

**结论：没有跳过 AWDL 的"局域网传输"开关。** 原生 BCM 卡可 AirDrop 正是因为 AppleBCMWLANCore 原生实现了 AWDL；我们的驱动没有 → 传输必然失败。

## 五、如果真要 AirDrop（AWDL 直连）：驱动侧需要什么

| 层 | 内容 | 难度 |
|---|---|---|
| 接口 | 注册 **awdl0** 虚拟接口（IO80211 多接口模型）| 中 |
| 频段 | 关联期间以 **5GHz ch149/周围 AWDL 保留信道** 时分收发 | 高（不能打断正常流量）|
| 帧格式 | Apple AWDL 专属 802.11 帧封装（私有 OUI/时序槽）| 高（需逆向）|
| 契约 | 系统 ioctl（APPLE80211_IOC_AWDL_*，被拒直方图 0x0161 等）与 WCL 通道 | 高（需逆向 IO80211Family）|
| 共存 | 正常 Wi-Fi 与 AWDL 分时、睡眠唤醒重建 | 高 |

**参考现状**：itlwm 社区不支持 AWDL；Linux 至今无生产级 AWDL（NAN 是不同协议但可做思路参考）。
**预估**：月级工程 + 高回归风险（可能影响现有 Wi-Fi 稳定性），**维持不做**的结论。

## 五、外部项目评估（2026-11-07）

| 项目 | 内容 | 对我们的意义 |
|---|---|---|
| **Guizzan/mt7927-airdrop** | Linux 上 MT7927(`mt7925e`) 的 AirDrop：实测 iPhone→PC ~7MB/s、Mac→PC ~10MB/s | **最重要的硬件可行性证据** |
| **seemoo-lab/opendrop** | AirDrop 应用层（Python，AWDL 之上）| AWDL/AirDrop 协议层开源参考 |
| **jedbillyb/owl**（seemoo-lab/owl fork）| AWDL 协议核心 + 用户态虚拟接口 + 信道序列策略 | AWDL 协议实现参考 + 信道调度经验（如 `pin` 到 peer 信道会破坏 iOS26 TX 的反例）|

**mt7927-airdrop 揭示的芯片能力（关键）**：让 MT7927 收发 AWDL 只需三条底层手段，且全部**与 macOS 驱动已有原语重合**：

| Linux 侧手段 | 解释 | macOS 驱动对应能力 |
|---|---|---|
| 为 AWDL peer 启动**固件 RX Block Ack**（`mt7925_mcu_uni_rx_ba`）| A-MPDU 收发必需 | ✅ 已有 BA 会话（MtkW9Ba / rxBaStart/rxBaStop / BA 窗口）|
| 把 peer 作为**关联 STA 加入隐藏 AP vif**（`awdl-sta`）| 固件要有该 peer 的 station 记录 | ✅ 已有 STA_REC 增删（joinStart/staRemove）|
| 多虚拟接口同信道 + sniffer 信道配置 | 主 STA 与 AWDL 共存 | ⚠️ 有 ROC（临时占道，MtkRoc）；多 vif 需扩展 |

> **这条信息推翻了此前"芯片不支持/不确定"的保留**：**MT7927 硬件/固件完全具备 AWDL 收发能力**，
> 且具体手段与我们已经实现的驱动原语高度重合。

**为何仍不能直接移植**：Linux 方案的 AWDL 协议跑在**用户态 OWL**（自研栈 + netlink 虚拟接口），与 Linux 卡驱动解耦；
macOS 的 AWDL 由 **IO80211Family 系统栈垄断**，AirDrop 走系统 UI/分享框架，第三方 kext 无法"自建协议栈"接入。
要在 macOS 上用系统 AirDrop，只能**配合系统栈**（awdl0 + AWDL ioctl 契约 + 主链路分时），这是唯一的、未变的工作量/风险点。

**结论更新**：AWDL 缺口从"协议未知 + 芯片未知"收敛为**单一且明确的一步——逆向 IO80211Family 的 AWDL 驱动契约**（0x0161 家族 ioctl、awdl0 注册、信道分时调度）。协议行为有 OWL/opendrop 参考、芯片原语已有。可行性评估从"极低"上调为"**有明确路径但仍是月级逆向工程**"。

### 五、修正说明

上一版"一票测试"预判"发现也会不可用"，实测修正：**发现（BLE）与握手（局域网链路本地）在无 AWDL 时依然可用**，
只有**数据传输**被 AWDL 卡住。对 AirDrop 的判定从"全链路不可用"更新为"仅传输不可用"。

## 六、替代方案（供日常使用）

- 跨设备传文件：微信文件传输助手 / iCloud Drive / 局域网 SMB（Mac 与 iPhone 均可挂）
- 接力/剪贴板：现状 iCloud 路径已可用
- 若未来评估做 AWDL：先攻 0x0161 家族 ioctl 契约 + awdl0 最小链路，再做分时（分阶段）

---

*最后更新：2026-11-07。本文仅为研究与判定，未实施。*

---

## 七、IO80211Family AWDL 契约逆向（第一阶段，2026-11-07）

使用 **rea (Reverse Engineer Anything) + Ghidra 12.1.4** 对 IO80211Family(1200.13.1)
（从 BootKernelExtensions.kc 提取）完成全量导入分析：12,341 函数、176,463 字符串。

### 关键发现

1. **AWDL 协议栈完全在系统 IO80211Family 内部**：`IO80211AWDLPeerManager`（选举/信道时序/
   duty cycle/流量登记）、`IO80211AWDLPeer`、`IO80211NANPeer`、`IO80211AWDLProtocol`,
   以及 AirDrop/SideCar/FaceTime/CarPlay 的 `handle*TrafficReg` 全是系统栈实现。
   `apple80211get/setAWDL_*` handler 只是**薄包装**：经接口 vtable（槽 0xcc8 门 / 0x10d0 派发）
   把`apple80211_awdl_* `结构转给核心。
2. **驱动契约 = 35 get + 35 set 共 70 个 `apple80211get/setAWDL_*` skywalk ioctl**（完整清单
   记于对应 REA 会话 notes）。这是第三方驱动要响应才能被系统视为"支持 AWDL"的接口面，
   **也是 MtkWlanNative 被拒直方图（0x0161 家族）的根本来源**。
3. **信道分时由系统栈调度**：`awdlDutyCycle` / `updateAwdlChanSeq` /
   `setAwdlChannelSequence` / `dynSdbStateMachine` 均在 PeerManager 内完成；
   驱动只需执行（切信道、设 sync frame 模板、收发、给 peer 建 BA/sta）。
4. **awdl0 虚拟接口**通过 `IO80211VirtualInterface`（postAwdlStatistics / setInterfaceCCA）
   家族创建与维护；第三方 skywalk 客户端需在 IO80211Family 认可的控制器路径上注册。

### 工作量重估（相对此前"自研协议"降档）

| 项 | 需要 | 难度 |
|---|---|---|
| 实现 70 个 AWDL ioctl handler | 把 apple80211_awdl_* 缓冲转发给驱动核心（多为薄转发）| 中（量大但机械）|
| 驱动核心芯片能力 | 多接口/虚 BSSID、按序列切信道、收发 sync frame、给 AWDL peer 建硬件 BA/STA 记录（呼应 mt7927-airdrop 的 awdl_rx_ba）| 中高 |
| AWDL 协议状态机 | **无需实现**（系统栈全包）| — |

> 与 mt7927-airdrop(Linux) 的关键差异确认：**macOS 上协议在系统栈**，不做自研 OWL；
> 但芯片级手段（固件 RX BA / STA 记录 / 隐藏 AP 承载）两边一致，MT7927 硬件可行已由
> 该 Linux 项目证实。逆向材料（70 ioctl + PeerManager) 使我方"实现驱动接口面"路线明确。

## 八、实现蓝图 v1（基于逆向，2026-11-07）

### 已确认的事实链

```
用户态(rapportd/apsd/airportd) --(ioctl)--> IO80211Family
  IO80211AWDLPeerManager(协议/选举/时序，系统内置)
        |  via IO80211VirtualInterface (awdl0)   (IO80211Controller::setAwdlInterface 登记宿主)
        v
  底层驱动 = IO80211Controller 子类 (AppleBCMWLANCore / 我们的 MtkWlanNative)
        |  响应 70 个 apple80211get/setAWDL_* skywalk ioctl + controller 槽
        v
  芯片: 多接口/虚BSSID/切信道(按 channel sequence)/sync frame 收发/给 peer 建硬件 BA·STA
```

- 我们的 `MtkWlanNative : public IO80211Controller` → **有资格实现 setAwdlInterface 等控制器槽**
- 但 ABI 里 vtable > 462 的槽我们未 dump → **IO80211Family 需要的 AWDL 控制器槽（如 0x10d0=538）我们的 vtable 空着** → 系统调用落空 → 只见"被拒 ioctl"（0x161 家族 1025 次）

### 最小验证实验（下一步，可回滚）
1. 在 MtkWlanNative 实现 2~3 个薄 AWDL ioctl handler（如 set/getAWDL_BSSID、CONFIG、SYNC_ENABLED）：
   接受结构体、记录属性/日志、返回成功——先"让系统认为我们接住了"
2. 实现 `IO80211Controller::setAwdlInterface` 槽（记录被调用的接口指针）
3. 装 EFI 重启，观测：awdl0 是否出现 / PeerManager 是否开始推进 / 系统是否请求更多 AWDL ioctl
⇒ 一次重启就能判定"系统是否接受第三方驱动走 AWDL 路径"。若推进，再按 PeerManager 需求逐项补齐。

### 工作量分档（最终）
- 实验/验证阶段：1 次重启，风险可控（新代码不影响现有 STA）
- 完整 AWDL（若实验为真）：70 ioctl 薄转发 + controller 槽 + 芯片能力（多接口/切信道/BA/STA）+ wifi 侧多接口分时 → **数周级**，但系统栈承担协议全部心智，路线确定
- 仍保留的不可知因素：IO80211Family 是否在 runtime 校验驱动"原生性"（若强制校验则第三方无解——实验可揭晓）
