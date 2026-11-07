# 逆向驱动的功能提升路径分析（REVERSE-PATH-ANALYSIS.md）

> 2026-11-07。基于本项目现状（Wi-Fi/BT 已稳定、漫游上线、通用命令应答完成、AWDL 判定封死）
> 与已建立的逆向工具链（REA + Ghidra 12.1.4 + IO80211Family 全量 vtable 映射），
> 评估"通过逆向 macOS 原生服务/驱动来改进本项目、补齐 Wi-Fi/BT 功能"的**可行性矩阵**。
> 本文为分析，未实施。

## 一、原则（重要）

逆向 **只用于"理解契约"**，最终在**驱动 / kext 侧配合实现**。**不建议修改系统服务二进制**：
- 用户态系统服务（airportd/CoreWiFi/BT 栈）改后需重签名 + 关 SIP，且每次 macOS 更新失效；
- 唯一长期安全路径 = 驱动侧提供更正确的能力/行为，让原生栈的既有逻辑走"正确分支"。

## 二、原生栈分层与我们的接触面

| 层 | 组件 | 我们能否接触 |
|---|---|---|
| Wi-Fi 用户态 | airportd、CoreWiFi.framework、apsd/WiFiAgent、networkd | 只读（理解判定逻辑）|
| Wi-Fi 内核 | IO80211Family + `MtkWlanNative`/`MtkWlan`（我们）| ✅ 已深度逆向 + 实现 |
| Continuity | rapportd、sharingd、apsd | 只读（AWDL gate 已判封死）|
| 蓝牙 | bluetoothd/BTLE、BlueTool 桥、IOBluetoothFamily + `MtkBt`（我们）| 部分（BlueTool/MTK 传输是大家做的桥）|
| 能力说明 | `CARD_CAPABILITIES` / IOReport | ✅ 我们已能自报（已验证改变了系统行为）|

## 三、可行性矩阵

| # | 目标功能 | 逆向对象 | 驱动侧落地方式 | 可行性 | 工作量 | 风险 |
|---|---|---|---|---|---|---|
| 1 | 系统信息完整（BEACON_INFO/CONNECTION_ID/PLATFORM_CONFIG 等 get 类）| IO80211Family（已全量）| 实现剩余 stub（方法已验）| **高** | 低 | 低 |
| 2 | **E-4 显示"802.11ax"**（当前显示 ac）| **CoreWiFi.framework + airportd**（PHY 判定路径）| 据判定逻辑在能力属性/IOReport 满足 | 中 | 中 | 低 |
| 3 | 更强认证（PMKID 会话/FT 相关）| IO80211Family 槽 | 驱动侧调试 | 中 | 中 | 中 |
| 4 | 漫游 11k/v/r 系统衔接（认领 WCL/ROAM 槽）| airportd + IO80211Family WCL | 驱动响应 WCL（已 hint）| 中高 | 高 | 中 |
| 5 | **蓝牙 Handoff / 邻近发现**恢复 | bluetoothd/BTLE + BlueTool 桥 | 检查 BT 私有特征/桥接是否可开 | 中 | 中 | 中高 |
| 6 | 蓝牙多设备并发策略 | bluetoothd | 驱动侧调参 | 中低 | 中 | 中 |
| 7 | 原生菜单/UI 细节补全 | MtkWlanNative（已有）| 增补 property | 高 | 低 | 低 |
| 8 | LE Audio | bluetoothd 全栈 | 系统不支持则无解 | 低 | 极高 | — |
| 9 | AWDL / AirDrop 传输 | — | 能力 gate 封死（实证）| **不可行** | — | — |
| 10 | 6GHz / WPA3 / 802.1X | — | 已排除（监管/复杂度）| 不做 | — | — |

## 四、关键机会逐条评估

### 2. E-4 显示 802.11ax（最值得推）
- 现象：`system_profiler` 显示当前网络 PHY=802.11ac，但 `MtkW8Mode`=HE 160MHz（实际 11ax）。
- 判定在 **CoreWiFi/airportd 用户态**：它们根据 `CARD_CAPABILITIES` 的 PHY-mode 字 + 链接时的 PHY 状态（IOReport/`CMD_...`）推导。
- 我们已自报 Wi-Fi 6E 能力字（getCARD_CAPABILITIES d[0x0e-0x10]），但显示仍 ac → 说明**另一处判定**（关联时传递的 PHY/IOReport）。逆向 CoreWiFi 的 `physicalLayerMode` 计算可定位所需属性。
- **可行性中、风险低**（只动驱动属性），是本阶段最贴合"逆向原生服务→驱动配合"的样例。

### 5. 蓝牙 Handoff/邻近
- 已实测：通用剪贴板/接力走 iCloud，AirDrop 发现（BLE）正常但传输（AWDL）失败。
- Handoff 的"发现"需要 BTLE 私有特征广播（Apple OOB）。我们的蓝牙桥（BlueTool+LegionBtFix）是否广播取决于 BT 传输层支持。逆向 bluetoothd/BTLE 可确认：**在无原生 Apple 蓝牙下的发现通路是否还能恢复**。可行中点但风险中高。

### 4. 漫游系统衔接
- 我们已有自研漫游（RSSI，工作正常）。系统 WCL 槽（11k/v/r）若认领，可让系统漫游逻辑（airportd 下发 roam profile）与我们的配合，但工作量大、收益待定（自研已够用）。

## 五、推荐路线（按性价比排序）

1. **【短期·高】补齐 IO80211 剩余 get 类**（#1）：BEACON_INFO、CONNECTION_ID、PLATFORM_CONFIG、BTCOEX_ACTIVE 等——方法已验证、低风险、无需新逆向。
2. **【中期·中】E-4 显示攻坚**（#2）：逆向 CoreWiFi 的 PHY 判定，驱动属性对齐。目标明确、风险低、收益是"显示正确性"。
3. **【中期·中】蓝牙 Handoff/邻近可行性确认**（#5）：先逆向确认通路，再决定投入。
4. **【后期·高成本】11k/v/r 系统衔接**（#4）：与自研漫游权衡后再定。
5. **不做**：LE Audio（全栈威…）、AWDL（封死）、6GHz/WPA3/802.1X（已排除）。

## 六、结论

**完全可行**，且我们已有 90% 的基础（工具链 + IO80211 全量映射 + 已被验证的系统行为驱动法）。
"通过逆向原生服务改进本项目"的正确打开方式 = **反向理解用户态判定（CoreWiFi/airportd/BTLE），再在驱动镜像侧满足条件**，
让原生栈走正确分支；这一策略已在"通用命令应答""BTCOEX"上被实证有效（Rejected 6200→0、系统推进探测）。

最大的现实收益与最小风险是 **#1 与 #2**；#5 做可行性侦查后再定。AWDL 保持"不可行"结论。

*（本文为分析，未实施。工具与依据见 docs/IO80211-REV-NOTES.md、AWDL-CONTINUITY.md。）*