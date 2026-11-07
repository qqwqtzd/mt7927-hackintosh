# MT7927 驱动项目总览与最终路线（OPTIMIZE-ROADMAP.md）

> 项目状态：**功能完备、系统稳定、文档归档、GitHub 已同步**（截至 2026-11-07）。
> 本文件是项目**总进度与决策簿**：已完成 / 已判定 / 待触发 / 剩余可选，一目了然。

## 一、当前状态总览

| 维度 | 状态 |
|---|---|
| Wi-Fi | ✅ 2.4/5GHz + HE 160MHz（协商 2064Mbps）+ WPA2 + IPv6，`W6 pass` 稳定 |
| 蓝牙 | ✅ AirPods A2DP（无 HCI logging 依赖）、LE/经典设备，`ready` |
| 漫游 | ✅ 自研 RSSI 漫游上线并调参（多 AP 实测切换）；系统 profile 联动已接线（待系统触发）|
| 系统接口 | ✅ 25+ 个 apple80211 命令应答，**`MtkNativeRejected` = 0**（6200+ → 全放行闭环）|
| 恢复/诊断 | ✅ EFI 自动备份 + Windows 侧一键恢复；`tools/diag.sh` 一键诊断包 |
| 工具链 | ✅ REA + Ghidra 逆向栈（IO80211Family 全量映射 361 探针码已存档）|
| 发布 | ✅ 文档全套归档；GitHub 4 仓库已推送（本地=远端）|

## 二、已实现并验证（按域）

### 核心功能
- [x] 2.4/5GHz 关联，WPA2-Personal，HE(11ax) 80/160MHz（P-1：实测 2064Mbps 2SS 0 丢包）
- [x] IPv6（HDR_TRANS 组播修复）
- [x] TXS 速率遥测、WMM 多队列、RX 重排诊断（MtkW9* 属性）
- [x] 原生前端 MtkWlanNative（菜单/原生 Wi-Fi 栈接入）

### 稳定性（S）
- [x] S-1 睡眠/唤醒（wakeRc=0 自动回连）
- [x] S-2 固件卡死自恢复 / S-3 断线自动重连 / S-4 RX 压力（健康）
- [x] S-5 蓝牙 warm path（冷启 3.7s 一次过，无需优化）

### 漫游（F-2，重点交付）
- [x] 自研 **RSSI 选强漫游**（多 AP 同 SSID，默认开）
  - 调参：`MtkRoamDelta 12→8`（对齐 AC `roaming_dif=8`）、绝对阈值 `-80/-75`（镜像 AC）、
    3dB 窗口 HE 优先、`MtkW5RoamEvals` 诊断发布；实测切换正常（MtkW5Roam+2）
- [x] **系统漫游档案联动（#4）**：`setROAM_PROFILE` 解析（band_data trigger/delta）→ 推送
  `MtkRoamAdvise` → 核心 `applyRoamAdvise` 应用（一次性消费；待 airportd 触发后生效）

### 蓝牙（B）
- [x] B-1 A2DP 稳定；B-5 warm path
- [x] BTCOEX 系应答（get/set PROFILE/ACTIVE/2G_CHAIN/EXT accept + version）——共存接收端就位

### 系统接口 / 逆向（E）
- [x] E-5 通用命令应答：18 探针放行 + 实现 getNSS / getCHANNELS_INFO / setPM_MODE / setROAM_PROFILE /
      setWCL_ROAM_PROFILE_CONFIG / BTCOEX 系 / BEACON_INFO / BSS_BLACKLIST / TXRX_CHAIN_INFO /
      SLOW_WIFI / FW_HOT_CHANNELS（11/get：version 应答；set：accept）
- [x] E-6 IO80211Family 全 vtable 映射库：361 探针码 ↔ apple80211 命令名（REA/Ghidra）
- [x] 0xcc8 探针法 / 槽位定位法 / buffer 大小从 Family 调用点取（工程方法沉淀）

### 工程 / 文档 / 发布
- [x] E-2 `tools/diag.sh` 一键诊断包；`VERIFY-MATRIX.md` 验收矩阵
- [x] E-3 `UPSTREAM/` 可回馈补丁集（roam-* 4 + cmd-* 2，ISC；注明不可入 GPL-2.0 itlwm）
- [x] 文档全套：README（通用手册）、INSTALL、PORTING、AI-GUIDE、ROAM-AWDL-ANALYSIS、
      AWDL-CONTINUITY、MT7927-CAPABILITIES、IO80211-REV-NOTES、REVERSE-PATH-ANALYSIS
- [x] GitHub：qqwqtzd/{mt7927-hackintosh, WTLMtkWireless, WTLWifiMenu, WTLMtkBluetooth} main 全同步

## 三、已调研判定（不再投入）

| 项 | 判定 | 依据 |
|---|---|---|
| **AWDL / AirDrop 传输** | ❌ 封死 | 系统能力 gate 在驱动不可达路径；系统从不发起 AWDL 命令（0 次）；rapportd 走 AWDL 降级（实测）|
| **E-4 显示 802.11ax** | ❌ 系统残缺 | CoreWiFi/IO80211.framework 用户态**不存在**（精简构建），显示判定无人读；驱动能力字已尽力 |
| **BT 共存闭环** | ⚠️ 部分 | bluetoothd UCM 引擎愿推，但 WCM 服务本机缺失 → 内容推不到驱动；驱动 BTCOEX accept 已就位 |
| WPA3-SAE / 802.1X | ❌ 不做 | 家用 WPA2 够用；密码学数月 / 不适用（用户决策）|
| 6GHz / 6E | ❌ 不做 | 中国大陆未开放（监管）|
| 802.11r 无缝 FT | ❌ 不做 | 固件+macOS 无 FT 路径（分析过）|
| LE Audio | ❌ 低优先 | 全栈系统支持，驱动不可及 |

## 四、待触发 / 观察项（无动作）

- **#4 漫游档案联动**：系统（airportd）下发 ROAM_PROFILE 即自动应用（待系统触发场景：多 AP 网络管理/漫游模式配置/电源切换等）
- **BTCOEX 生效**：WCM 一旦存在（系统组件补全）既有接收端
- **漫游长期行为**：观察 MtkW5Roam/MtkW5RoamEvals 1-2 天（diag.sh 随时可采）

## 五、剩余可选（按性价比，供未来决定）

| # | 项 | 最终状态（2026-11-07 已逐一处理）|
|---|---|---|
| 1 | 0x100 放行 | ✅ **已实施**（加入 commandAllowed，装机 `68161aeb`，重启验证 Rejected=0）|
| 2 | 认证增强（PMKID/FT）| ✅ **评估完成**：4-way（secService）已实现并安装 PTK/GTK；PMKID 缓存收益低（回连在 10s 握手时限内），FT 无路径。**不做**（记录）|
| 3 | P-2 多 TX 队列 | ✅ **评估完成**：当前 TX 为单一数据 ring（txData.regs，无多 ring 分流）；多队列=数据面大改+风险；家庭带宽下收益待验证（需先基准）。**留待有吞吐痛点时再做** |
| 4 | P-3/P-4 checksum/ring | ✅ **评估完成**：无 HW checksum 路径（软件校验，正确）；ring 已按 Linux 尺寸配置并复核（TX_RING0_EXT_CTRL 管理在位）。**维持现状**（数据面不动）|
| 5 | F-6 弱信号/省电 | ✅ **评估完成**：驱动无 PS 空口省电（MtkPower 是 TX 功率非省电）；PS 实现=中等工程，台式持续供电收益低。**不做** |
| 6 | B-2 多设备并发 | ✅ **评估完成**：蓝牙多连接由系统栈（bluetoothd/BTLE）管理，我们桥提供固件多连接能力；无线驱动侧无可改点。**维持现状** |

## 六、方法沉淀（可复用）

- 逆向三件套：REA + Ghidra 12.1.4（11.3.2 补 mac decompiler）+ 本机 IO80211Family 全量映射
- 半自动化：`tools/kcabi/`（dump→genhdr→datas）+ diag.sh + VERIFY-MATRIX
- 原则：逆向只用于理解契约，能力机关在驱动/kext 侧满足；不修改系统服务二进制

---
*2026-11-07 最终整理。项目处于稳定驻留点：功能齐备、可日常使用、可继续开发、可分享。*
