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

### 2. E-4 显示 802.11ax —— 已判为"框架缺失"，驱动侧无解（修正）
- 实测（2026-11-07 全对象侦查）：`CoreWiFi.framework` 与 `IO80211.framework` 主二进制**不存在**（symlink 悬空、
  airportd weak 链接容忍、全盘无 dyld shared cache）。用户态 Wi-Fi 判定层在本机（Hackintosh 精简构建）**先天残缺**。
- 因此"显示 802.11ax"等依赖 CoreWiFi 的功能**无法由驱动修复**（对象不存在），只能"驱动尽力上报 + 接受显示局限"。
- 已尝试的能力属性（CARD_CAPABILITIES Wi-Fi 6E 字）无效即源于此——系统判定根本没人读。

### 5. 蓝牙 Handoff/邻近
- 已实测：通用剪贴板/接力走 iCloud，AirDrop 发现（BLE）正常但传输（AWDL）失败。
- Handoff 的"发现"需要 BTLE 私有特征广播（Apple OOB）。我们的蓝牙桥（BlueTool+LegionBtFix）是否广播取决于 BT 传输层支持。逆向 bluetoothd/BTLE 可确认：**在无原生 Apple 蓝牙下的发现通路是否还能恢复**。可行中点但风险中高。

### 4. 漫游系统衔接
- 我们已有自研漫游（RSSI，工作正常）。系统 WCL 槽（11k/v/r）若认领，可让系统漫游逻辑（airportd 下发 roam profile）与我们的配合，但工作量大、收益待定（自研已够用）。

## 五、推荐路线（修正后，基于"全对象侦查"）

**全对象侦查结论**：完整且可逆向 = `IO80211Family`（内核，全量）、`airportd`、`rapportd`；
`CoreWiFi/IO80211/BT` 用户态框架在本机**缺失**（空壳+弱链接）→ 相关"系统判定型"功能（如显示层）驱动侧无解。

1. **【短期·高】补齐 IO80211 剩余 get 类**（#1）：BEACON_INFO、CONNECTION_ID、PLATFORM_CONFIG、BTCOEX_ACTIVE 等——方法已验证、低风险。
2. **【中期·中】认证增强**（PMKID/FT 辅助，#3）：IO80211 槽，无用户态依赖。
3. **蓝牙共存联动 —— 逆向确认存在链路，触点=IO80211 BTCOEX**（2026-11-07）：
   airportd `setBluetoothCoexProperty` 校验 entitlement + key 白名单后织 `CWRequest(setType:0x13)` → scheduler
   → 下发给 WiFi 驱动（BTCOEX 系 ioctl，我们已 accept）。剩余未知：bluetoothd 侧是否主动上报（待逆向 bluetoothd）。
4. **【中期·中】漫游配置联动（#4 平价化）**：系统漫游=档案驱动（CWRoamingProfileConfiguration → ROAM_PROFILE），
   链路已到我们 accept 的 setROAM_PROFILE；可做=解析 profile 联动自研漫游参数（RoamDelta/AbsRssi），无协议复杂度。
5. **不做**：E-4 显示（框架缺失）、LE Audio、AWDL（封死）、6GHz/WPA3/802.1X。

## 六、结论

**完全可行**，且我们已有 90% 的基础（工具链 + IO80211 全量映射 + 已被验证的系统行为驱动法）。
"通过逆向原生服务改进本项目"的正确打开方式 = **反向理解用户态判定（CoreWiFi/airportd/BTLE），再在驱动镜像侧满足条件**，
让原生栈走正确分支；这一策略已在"通用命令应答""BTCOEX"上被实证有效（Rejected 6200→0、系统推进探测）。

最大的现实收益与最小风险是 **#1 与 #2**；#5 做可行性侦查后再定。AWDL 保持"不可行"结论。

*（本文为分析，未实施。工具与依据见 docs/IO80211-REV-NOTES.md、AWDL-CONTINUITY.md。）*

## 七、BT 共存链路最终闭环判定（2026-11-07 bluetoothd 逆向）

```
bluetoothd UCM 引擎（BT音频/2.4G/HRB/AFH 地图/t3-t4 时序, UpdateWifiState/coexCase…）
  → XPC 推送 com.apple.WirelessCoexManager (WCM)
  →（设计上）airportd UCM / CWRequest(0x13) → IO80211 BTCOEX → 驱动
```
逆向结论：
- 蓝牙侧引擎完整（主动推）；airportd 接收端在（com.apple.wifi-xpc 等已注册，entitlement com.apple.wifi.ucm）。
- **WCM 用户态服务在本机缺失**（launchd 无注册；bluetoothd 自身日志 "WirelessCoexManager not available"）。
  同 CoreWiFi/IO80211.framework 命运：本机（Hackintosh 精简构建）用户态组件残缺。
- 因此共存内容**推不到驱动**；我们驱动 BTCOEX 系列 accept 已就位（第三方能做的最大程度）。
- 与此前的 E-4 显示、CoreWiFi 缺失同族：**系统用户态残缺是这类问题的共同根源**，不是驱动可补。


## 八、E-4 收尾：Channel 已修、PHY 标签判定路径定位（2026-11-07，含 M5 真机对照）

- **Channel 带宽显示（真 bug）已修复**：核心把 160MHz 上报为 `bw==3`，前端 `linkChannelFlags`
  缺 3 分支 → 误报 20MHz；补 `bw==3 → 0x800`（itlwm `APPLE80211_C_FLAG_160MHZ`）后显示 160MHz ✅。
- **PHY Mode 标签（ac→ax）判定路径**（M5 cache 方法名表逆向确认）：CoreWiFi 有
  `physicalLayerMode / supportedPhysicalLayerModes / supportsPHYMode`；
  当前 PHY 判定最可能经 `getPHY_MODE`（active_phy_mode）的 `IO80211NoneProtocol` 服务——**前端无该服务接入点**，
  系统按不支持回退 ac。与 AWDL/WCM 同为"系统判定路径对第三方驱动不可达"，驱动侧无解；标注为接受项。
- M5（macOS 27.2 arm64）对照：原生显示 ax + 160MHz，驱动为 DriverKit `AppleCentauriAlpha`。
- 结论更新：**E-4 修正**——初期"CoreWiFi 缺失"判错（实现在 dyld cache）；实际为第三方驱动无法接入其判定服务。

## 九、剩余需求 × 逆向方案重估（2026-11-07 晚，含 M5 逆向）

| # | 需求 | 原判定 | 本机/M5 逆向新证据 | 重估 |
|---|---|---|---|---|
| 1 | **E-4 PHY Mode 显示 ax** | 不可达 | 本机内核 dispatch 证实 ioctl 0x0E 唯一路径 = `apple80211getPHY_MODE` → NoneProtocol 服务 → slot 61（**1194 槽几乎全 pure_virtual，子类需数百实现**）；M5 报告确认显示规则（PHYMODE_ACTIVE 单位 0x100）| **架构不可行**（子类成本过巨）；接受 ac 标签（功能不受影响，Channel 160 已修复）→ 详见 `docs/M5-PHYMODE-REPORT.md` |
| 2 | **WCM 共存闭环** | 服务缺失 | **M5 有 `WirelessCoexManager.framework` 实体**（含主二进制）；本机仅空壳（缺主二进制）→ 架构（arm64→x86_64）不可移植 | ⚠️ **条件可行**：需同 build 的 x86_64 主二进制来源（黑苹果镜像/同版本完整系统）|
| 3 | **AWDL/AirDrop** | 能力 gate 封死 | 无新推翻；补充**观测实验**（override 虚拟接口槽 419/445/446 打日志，系统若曾尝试创建 awdl 接口即可见）| 判定维持；观测可一锤定音 |
| 4 | 漫游档案触发 | 已接线待触发 | — | 维持 |
| 5 | PMKID/FT 认证 | 收益低（不做）| — | 维持 |
| 6 | F-6 省电 / P-2 多 TX | 评估维持 | — | 维持 |

**新三选项**：① E-4 PHY：实现 NoneProtocol 子类（本机逆向可解）；② WCM：找同版 x86_64 框架源（资源型）；③ AWDL：观测探针（一次重启）。


## 十、AWDL 探针实测结论（2026-11-07 晚，重启验证）

- 在 `MtkWlanNative`（IO80211Controller）override `enableVirtualInterface / disableVirtualInterface /
  createVirtualInterface / attachVirtualInterface / detachVirtualInterface`（仅日志，行为走基类）。
- 重启后整段运行：**MtkNativeTrace 无任何 `vif:*` 记录、awdl0 未出现** → 系统对第三方内核 Controller
  **从不请求创建 Wi-Fi 虚拟接口** → AWDL/awdl0 在第三方驱动上通关失败（能力 gate 实证）。
- 与 WCM 同族更新：26.x86_64 生态无官方 Intel 构建 → WirelessCoexManager 用户态在该生态大概率本身缺失；
  E-4 显示标签受 NoneProtocol(~1156 纯虚) 限制。三个"系统侧功能"统一为**系统组件/接入点限制，驱动不可逆**。
- 项目最终稳定驻留点：全部Functional/Cosmetics 项闭环；见面可交付 = 160MHz 显示修复（真实 bug）。

## 十一、通用剪贴板单向（Mac→iPhone 失败）根因：AKS 安全硬件依赖（2026-11-07）

现象：iPhone→Mac 剪贴板（文字/图片）正常；Mac→iPhone 全部失败。
- pboard 日志：`AppleKeyStore: failed to open connection (-536870174)`、`AKS: Error: Locked (4294967295)`。
- 排除：AWDL（无 AWDL 但拉取侧同网可用）、FileVault 卷锁（卷状态 Unlocked）、applekeystored 服务（running）。
- 根因：pboard 远程剪贴板"加密创建/上传"调用 **AppleKeyStore 设备状态**，在无 Secure Enclave 的
  Hackintosh 上该 API 结构性失败 → Mac 无法上传剪贴板项（iPhone→Mac 拉取不受 AKS 写入依赖，故单向可用）。
- 判定（最终证据链，2026-11-07 终版）：**结构性不可修**。
  - 已排除：FileVault（关闭后仍失败）、applekeystored kickstart、接力开关（开）、useractivityd 重启。
  - **useractivityd 日志决定性证据**：能**发现**附近设备（`SFContinuityScanManager Received advertisement`、
    `foundDevice`），但对端广告载荷 **`flagCopyPasteKey = 0`** → 本机广播**不宣告剪贴板能力** →
    useractivityd 上传静默不触发 → M5/iPhone 永远收不到（拉取侧正常 = iCloud 路径，不受影响）。
  - 根因：Apple 私域蓝牙广播的剪贴板能力标志需蓝牙栈专门支持（固件/HCI 私域广告特性）；
    桥能"发现"但无法"宣告"→ Bluetooth 栈能力层结构性缺失（同族：无 AWDL/无 SE 键）。
- 替代：微信/文件/网盘；iPhone→Mac 单向可用。

## 十二、剪贴板广播层实锤 + 第三方兜底（2026-11-07 终）

- bluetoothd 日志：本机**确在发送 Apple 私域广告**（sharingd 发起：
  `Server.LE.Advertise ... manufacturer data [0B FF 4C 00 10 06 40 1D CB 9C A9 28]`），
  但载荷 flags `0x40` 仅为发现标志，**无 Copy/Paste 能力位** → 与对端见到的
  `flagCopyPasteKey=0` 一致。该位由系统（sharingd 组装、依据 Apple 芯片能力）决定，
  **非桥/驱动可设** → 结构性不可修（BT 广播能力在 Apple 芯片侧判定）。
- **第三方兜底（可用替代）**：LocalSend（localsend.org，macOS+iOS 免费/开源/局域网直连，
  双向文字/图片剪贴板+文件）；备选 CrossPaste、柠檬 Push。均为局域网 TCP，不依赖
  Apple Continuity 链路，与本机 AWDL/AKS 缺位无关。

### 原生路径已全部走尽（2026-11-07 终判）
- 已试（均无效）：接力开关重开、`ClipboardSharingEnabled=1`+重启 useractivityd、applekeystored
  kickstart、关闭 FileVault、退出/重登 iCloud（`handoff-own-encryption-key` 始终不生成）。
- 终判：系统初始化 Handoff 剪贴板身份依赖硬件安全条件 → 黑苹果不生成身份密钥 →
  sharingd 广播不自宣剪贴板能力（flagCopyPasteKey=0）→ 上传永不触发。
- **不装第三方前提下 Mac→iPhone 剪贴板不可修**；iPhone/M5→Mac 正常。
- 原生替代：备忘录 / iCloud 云盘 / 共享相簿（零第三方）。

## 十三、InstallAssistant-26.7.2.pkg 评估与 WCM 修正（2026-11-07）

- 安装包（17GB）为**软件更新型 asset 仓库**（SharedSupport.dmg + 16MB Payload；zip 分块为
  ramdisk/固件类），**不含完整系统组件实体**（非 IESD），对"实体逆向"无直接贡献 → 记录评估结论。
- 但引出修正：本机 dyld cache 含 `WirelessCoexManager` 镜像（主文件+分片），磁盘 stub 为正常结构；
  `bluetoothd Server.MacCoex` 持续上报 `isCoexDenied=0` → **BT/WiFi 共存通告在工作**，
  "WCM 缺失"不再成立；该项从"不可修"改为"无需修"。

## 十四、U 盘恢复镜像评估：首个可解析 x86_64 独立实体（2026-11-07）

- U 盘（disk5/1401，FAT32 引导盘）结构：EFI + NullMoth + com.apple.recovery.boot
  （BaseSystem.dmg 847MB）+ 13.6GB 未用空间；**无 InstallESD、无第二分区**（网络安装型引导盘）。
- BaseSystem = **macOS 15.6.1 (24G90) Sequoia x86_64**（非 26.7.2；跨版本契约参考级价值）。
- 提取到 `~/rea-tools/pkg/usb-extract/`：**CoreWiFi.framework（4.4MB，独立 x86_64）、
  IO80211.framework（606KB）、WiFiPolicy、BluetoothManager** —— 首个不受 dyld cache
  解析失败阻隔、可直接进 Ghidra 的实体。
- 快速三检产出：
  1) `CWFComparePHYMode` 反汇编：PHY 序数比较含 0x100/0x80 哨兵（15.6.1 已有该比较器）；
     E-4 卡点在内核 NoneProtocol 纯虚，用户态 PHY 逻辑正常 → 与判定不冲突。
  2) IO80211.framework 导出仅 `Apple80211GetRangingPhyErrorString`（无 PHY getter）→
     佐证 E-4：PHY 模式源在内核 IO80211Family，用户态本无该服务。
  3) WiFiPolicy 含 `BTCoex/AWDLCoex/RC1Coex` 内核采样类 → 独立佐证共存路径在内核侧工作
     （与十三节 WCM 修正一致）。
- 缺口：WirelessCoexManager 主二进制仍无（恢复镜像不含；bluetoothd 侧也未见引用字符串）。
  若要 WCM x86_64 实体：全量 IESD 镜像 或 本机 26.7.2 cache 提取（Ghidra 解析仍是已踩的坑）。
- 结论：U 盘对"实体逆向"提供**基础垫脚石**（WiFi 用户态框架 x86_64 可逆），不改变任何
  结构性结论（剪贴板/AWDL/E-4 维持）。

## 十五、WCM 注册条件逆向闭环（bluetoothd 25G332，2026-11-07）

- 实体：`/usr/sbin/bluetoothd`（23.5MB，x86_64+arm64e）→ `~/rea-tools/pkg/entities-25G332/`
- 方法：python 定位 __cstring 中 `com.apple.WirelessCoexManager`（VA 0x1009919d4）→ RIP-relative
  LEA xref 扫描（唯一引用 0x100799741）→ lldb 反汇编注册路径。
- 注册逻辑（反汇编实证）：
  ```
  cmpq $0, [GOT 0x100AA3C18]          ; 弱绑定槽检查
  je   skip                           ; 空 → 打日志跳过注册
  xpc_connection_create_mach_service("com.apple.WirelessCoexManager", 0, 0)
  xpc_connection_set_event_handler + resume   ; 存对象+0xa8
  skip: BTUCM / BTUCMForceFacetimeWifiCoexOverride 配置读取 → 0xb2/0xb4
  ```
- 绑定真相（dyld_info -fixups）：`__got 0x100AA3C18 bind WirelessCoexManager/_WCMEnabled [weak-import]`
- 符号存在性（cache 全串 grep）：**x86_64h 与 arm64e（M5）cache 均无 `_WCMEnabled`** →
  弱绑定必为 NULL → **macOS 26 全平台（含真机）都不注册该 XPC 服务** —— 与真机行为一致，
  非 Hackintosh 故障；"WCM 缺失"彻底关闭（无需修）。
- 附带：kWCMRegisterProcess_ProcessId / kMessageId（WCM 客户端注册协议键，服务不注册故无客户端）、
  BTUCM 配置读取点。MacCoex 通告独立工作（kCBAdvDataIsCoexDenied=0 实证）不受影响。
- 方法资产：python Mach-O fat/节解析 + RIP-rel xref 扫描 + lldb 反汇编 + dyld_info 绑定解码 =
  首个完整"实体→xref→反汇编→绑定→跨平台对照"闭环，后续逆向复用。

## 十六、E-4 PHY 显示：实验 A 负结果 + kernelcollection 逆向进行中（2026-11-07 夜）

- **实测管线落地**（零重启）：
  - `~/rea-tools/phyprobe`：dlopen 私有 IO80211.framework 调 `Apple80211CopyValue(0x0E)`，
    26.7.2 实机返回 `PHYMODE_ACTIVE=0x80 (ac)`、`PHYMODE_SUPPORTED=0x9F (a/b/g/n/ac, 无 ax)`——
    System Profiler "PHY Mode: 802.11ac" 的直接数据源。
  - 我们 kext 分发表无 0x0E → 字典由 IO80211Family 聚合（用户态只建字典，键名 PHYMODE_* 在
    IO80211.framework 用户侧；内核空间无该字面串 —— 全 kc 字符串扫描实证）。
- **实验 A（1 次重启，失败）**：getCARD_CAPABILITIES 的 PHY 字对 0x0e/0x0f 加 ax 位(0x100)，
  装机重启后探针仍 0x9F/0x80 → **family 不读该字**；另一字对 0x06/0x07 (0xa5bd 已含 ax 位)
  也无效 → 聚合输入另有其源。结论：不再盲试。
- **kernelcollection 逆向（进行中）**：
  - 活体 `BootKernelExtensions.kc`（25G332，64MB，kc2 格式）含 IO80211Family（REGION347，
    明文 x86_64 Mach-O，文件偏移即 kc 全局偏移）；`kc-slice.py` 提取器 + plist 缓存已归档。
  - IO80211Family 内 ioctl 名字表确认（SSID→LOCALE 全序，PHY_MODE=0x0E）。
  - AppleBCMWLANCoreMac（真机驱动，2.2MB）同集合可对照。
  - Ghidra 11.3.2 后台全量分析 kc 中；完成后定位 0x0E 聚合器 → 确定真正输入（候选：
    HE_CAPABILITY 应答 / 接口 PHY getter），再做最终驱动侧修复（预计还需 1 次重启）。
- 结论暂记：E-4 前判"结构性不可修"依据（NoneProtocol 纯虚）与显示链（ioctl 0x0E 字典）
  为**两条不同路径**；显示路径存在驱动侧杠杆（待 Ghidra 定论），维持"可修候选"状态。

### E-4 第三轮（实验 B）与终判（2026-11-07 深夜）
- 实验 B：handleCardSpecific 应答 HE_CAPABILITY(0x23a)（版本=1 + 偏移 +4/+8/+0xc/+0x10 置 HE
  位，依据 kc 反编译的真机 BCM 解析锚点）；装机重启后探针仍 PHYMODE_ACTIVE=0x80/
  SUPPORTED=0x9F → 无效果；且运行时 IORegistry 无 "he:card" trace → **家族自始至终未调用
  该卡特定通道** → 该输入与聚合无关（排除）。
- 至此驱动侧表面三轮全排除：CARD_CAPABILITIES PHY 字(0x0e/0x0f，补 ax 位无效)、
  CARD_CAPABILITIES HE 位(0x09)（原样镜像 BCM 常数）、卡特定 HE_CAPABILITY 应答（未被调用）。
- kc 全量逆向（Ghidra，12006 函数）补充证据：内核空间无 PHYMODE_* 字面键（用户态建键填值）、
  family 文本无 phyMode 类符号；聚合值 = 接口内部数值态。真机(M5)显示含 ax（system_profiler
  实证）依赖 family/接口内部机制，非第三方控制器可及。
- **终判：E-4 维持"结构性不可修"**，证据从早期"NoneProtocol 纯虚"推断升级为
  "驱动侧 apple80211 表面三层排除 + kc 全量反证"；phyprobe 为恒久复测工具。
- 实验 A/B 改动为真实性声明（MT7927 确为 HE/ax 能力），保留无害；如需回退可随时（代价 1 重启）。
