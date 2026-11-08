# MediaTek MT7927 (Filogic 380 / MT6639) Wi-Fi + Bluetooth for macOS Hackintosh

在 macOS（Tahoe 26.x 实测 / 更高或相近版本可尝试）上驱动 MediaTek MT7927 / MT6639
（AMD RZ717 及同类 M.2 Wi-Fi 6E 模块的对等芯片组）的内核扩展移植。目标：**任何
使用该网卡的黑苹果用户都可以按本手册自行构建、安装、排障**。

> ## 📌 项目状态（2026-11-08 完结）
>
> **驱动功能已完备**：Wi-Fi 2.4/5GHz + HE 160MHz（协商 2064 Mbps）+ WPA2 + IPv6、
> 蓝牙 A2DP/LE、自研 RSSI 选强漫游、25+ apple80211 系统接口全应答、睡眠唤醒、
> 固件自恢复、断线重连 —— 全部实测通过，可日常使用。
>
> **系统侧边界已实证闭合**（详见 [`docs/REVERSE-PATH-ANALYSIS.md`](docs/REVERSE-PATH-ANALYSIS.md)）：
> - 通用剪贴板 Mac→iPhone：**结构性不可解**（依赖 Secure Enclave 硬件签名，对端验签）
> - AirDrop / 随航 / Universal Control：**可修但工程量大**（已获第三方 MT7922 参考实现证明：复用 IO80211Family 内置 AWDL 引擎，见 `REV/reference/mt7922-awdl/`；端到端仍未验证）
> - WCM 服务注册：**全平台设计行为**（真机同样不注册，非缺陷）
> - PHY 标签 802.11ac→ax（当前链路行）：**内核补丁路径实测会致系统挂起**（4 种实现
>   全部验证），扫描记录行已正确显示 ax
>
> 上述结论均来自真二进制逆向 + 双机对照（本机 vs Apple Silicon 真机）+ 实机实验，
> 非推测。**这些功能的缺失不影响 Wi-Fi/蓝牙的日常使用。**
>
> **逆向资产**（25G332 真二进制、内核切片、工具链）保留在 [`REV/`](REV/)，供研究复用。
> 文档索引：[`docs/INDEX.md`](docs/INDEX.md)

本仓库是"总装工程"，包含：

| 组件 | 路径 | 说明 |
|---|---|---|
| Wi-Fi 驱动 | `WTLMtkWireless/` | 核心 kext（扫描/关联/WPA2/HE/Block-Ack/诊断）|
| 原生前端 | `WTLWifiMenu/` | `MtkWlanNative.kext`：macOS 原生 Wi-Fi 菜单 |
| 蓝牙加载器 | `WTLMtkBluetooth/` | `MtkBt.kext`：MT7925/MT7927 固件下载 |
| 构建/安装/恢复 | `tools/` | 脚本化工作流（见下）|
| 移植笔记 | `PORTING.md` | 逐项改动与 Linux 对照（含已知限制）|
| 优化清单 | `OPTIMIZE-ROADMAP.md` | 完成项与待定项 |
| 芯片能力分析 | `MT7927-CAPABILITIES.md` | MT7927/Wi-Fi7 规格、驱动现状、完善方案 |
| 漫游/AWDL 分析 | `ROAM-AWDL-ANALYSIS.md` | 方案分析；驱动侧 RSSI 选强漫游已实现（见完成区），系统层 11k/v/r、AWDL 未做 |
| 已构建产物 | `dist/` | kext + 固件（发布用）|

> 三个 `WTLMtkWireless/` 等子目录为独立 git 仓库（本仓库对其跟踪说明）；
> 未来上游更新可在各自目录内 fetch。

## 硬件前提

- 网卡：MediaTek MT7927 或兼容 MT6639 的模块（AMD 主板常见的 RZ717，PCIe 设备
  `14C3:7927` / `14C3:6639` / `14C3:0738`）
- 系统：macOS Tahoe 26.7.1 (25G241) / 26.7.2 (25G332)（`MtkWlanNative` 对 build 有
  ABI 校验；其它 macOS 版本可用 Ethernet 前端降级使用）
- 引导：OpenCore

## 快速开始

```bash
# 1) 构建三个 kext
bash WTLMtkWireless/tools/build-mac.sh      # out/MtkWlan.kext
bash WTLWifiMenu/build.sh                    # out/MtkWlanNative.kext
bash WTLMtkBluetooth/tools/build-mac.sh      # out/MtkBt.kext

# 2) host 测试（不重启，先自检）
bash WTLMtkWireless/tests/run.sh

# 3) 安装到 OpenCore EFI（自动识别带 OpenCore 的分区，自动备份，自动同步恢复工具包）
sudo bash tools/install-efi.sh

# 4) 在 config.plist 的 NVRAM/Add 的 7C436110-AB2A-4BBB-A880-FE41995C9F82 里加 boot-args：
#    mtkfe=native mtknativeif=1
#    （Kernel/Add 里按顺序加：Lilu(如需要) -> BlueToolFixup -> LegionBtFix -> MtkBt -> MtkWlan -> MtkWlanNative）
```

## 功能与实测结果（2026-11-07 终审）

### ✅ 已完成并在本机实测

| 域 | 明细 |
|---|---|
| 连接 | 2.4/5GHz、WPA2-Personal、802.11ax/HE **80/160MHz**（协商 2064 Mbps），IPv6 |
| 诊断/队列 | TXS 速率遥测、WMM 多队列、RX 重排、Block-Ack（MtkW9* 属性）|
| 稳定性 | 睡眠/唤醒（wakeRc=0）、固件卡死自恢复、断线自动重连 |
| 漫游 | 自研 **RSSI 选强漫游**（多 AP 同 SSID，默认开；delta 8dB / 绝对阈值 -80/-75 / HE 优先 + 诊断统计）|
| 系统联动 | 解析机场 ROAM_PROFILE 并联动自研漫游参数（`MtkRoamAdvise`，待系统触发自动生效）|
| 蓝牙 | AirPods A2DP 稳定（无 HCI logging 依赖）、LE/经典设备；BTCOEX 系命令应答（系统端就位）|
| 系统接口 | **25+ 个 apple80211 命令**正确应答（NSS/信道/PM/漫游配置/BTCOEX/信标…），`MtkNativeRejected` 6200+ → **0** |
| 界面修正 | **Channel 160MHz 显示**（真实 bug：核心 `bw==3` 落入 else 误报 20MHz，已修）|
| 工程 | `tools/diag.sh` 一键诊断包、验证矩阵、kcabi 工具链、`UPSTREAM/` 可回馈补丁、REA+Ghidra 逆向栈 |

### ⛔ 未完成功能与技术原因（均实证，非估计）

| 功能 | 为什么无法完成（技术原因）|
|---|---|
| **AWDL / AirDrop 传输** | 系统能力 gate 在驱动不可达路径。实证：AWDL 命令零调用、虚拟接口创建零调用（`vif:*` 探针）、erg handler 0 次；AirDrop 发现（BLE）与剪贴板/接力（iCloud）可用，**传输需 awdl0** → 封死 |
| **PHY Mode 显示"802.11ax"**（标签显示 ac）| 显示判定 = `APPLE80211_IOC_PHY_MODE (0x0E)` 的 `PHYMODE_ACTIVE`；读取该值需 **IO80211NoneProtocol 服务**（vtable 1194 槽中 **1156 纯虚**），第三方内核驱动无法实现该子类 → 架构不可达（实际 HE160 功能正常，仅标签）|
| **WCM 共存闭环**（BT/WiFi 自动协调）| `WirelessCoexManager` 用户态在 26 **x86_64 生态缺失**（macOS 26 起无官方 Intel 构建；arm64 原生组件不可移植）→ 系统组件缺口，驱动不可补 |
| **802.11k/v/r 系统级无缝漫游** | 协议由系统（CoreWiFi/airportd）层管；第三方内核驱动无系统接入点。我们以自研 RSSI 漫游 + ROPAM_PROFILE 联动替代（切换为"断+重连"，非 FT 无缝）|
| **WPA3-SAE / 802.1X / 6GHz** | 家用 WPA2 已够 / 场景不适用 / 中国大陆未开放 6GHz（均为决策不做）|
| **LE Audio** | 依赖 Apple 蓝牙全栈支持，驱动不可达 |
| **802.11r 无缝 FT** | 固件 + macOS 无 FT 路径（自研漫游为 1-3 秒断连切换）|

> 显示器路径的精确判定已由 MacBook Air M5 逆向厘清：见 `docs/M5-PHYMODE-REPORT.md`；
> 全部项目决策与进展见 `OPTIMIZE-ROADMAP.md`、`docs/REVERSE-PATH-ANALYSIS.md`。

## 日常排障 / 诊断（不重启也能做）

```bash
# 一键打包全部诊断（出问题时跑这个，把 zip 交给维护者/AI 分析）
bash tools/diag.sh                    # -> ~/Desktop/mtkwlan-diag-<时间戳>.zip

# 盯一个属性
ioreg -r -c MtkWlan -l -w0 | grep -E 'MtkState|MtkW8Mode|MtkW6TxFailed'

# 开机自动采集诊断快照（想免手工就装一下）
sudo bash tools/autotest.sh                 # 手动跑一次
#   或安装 LaunchAgent（替换 plist 里 REPO 路径）：
#   cp tools/com.mtkwlan.autotest.plist ~/Library/LaunchAgents/
#   launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.mtkwlan.autotest.plist

# 恢复：macOS 侧
sudo bash tools/restore-kexts.sh            # 回滚 MtkWlan 到上一稳定版 + 重启用被禁 kext
```

## 硬件层排障：蓝牙 + USB 口同时失效（断电复位）

**症状**（双系统用户常见，与驱动无关）：
```
- Wi-Fi 正常（PCIe 通路）
- 但蓝牙检测不到，系统显示：
    Chipset: BCM_4350C2   Address: NULL   Transport: UART   ← 占位值
- 同时部分 USB 口（前置 USB / U 盘 / 移动硬盘）不识别
- Windows 下同样检测不到（关键特征：两个系统都受影响）
```

**原因**：承载 M.2 蓝牙（USB 2.0 通路）与部分 USB 口的 XHCI 控制器进入异常状态。
主板关机后仍给 USB 供电（如为鼠标充电开启的关机供电），普通重启不会复位该控制器。

**解决**（90% 一次修复）：
```
1. 关机
2. 拔掉电源线（台式机）；笔记本同时拔充电器
3. 长按电源键 15 秒放电
4. 等 30 秒 → 插电开机
```

**若无效**：重新插紧 M.2 网卡（Wi-Fi 走 PCIe、蓝牙走 M.2 上的 USB 2.0 金手指，
轻微位移会导致 PCIe 正常而 USB 不通），并检查机箱前置 USB header 接线。

**判断表**：

| 现象 | 结论 |
|---|---|
| Wi-Fi 正常 + 蓝牙/部分 USB 失效（双系统一致） | USB 控制器状态异常 → 断电复位 |
| 断电无效，重插 M.2 后恢复 | M.2 USB 金手指接触不良 |
| 重插仍无效 | 该 USB 控制器或 M.2 卡硬件故障 |

**预防**：双系统频繁切换后如遇此现象，优先做完整断电（而非重启）。

## 双系统切换：蓝牙/Wi-Fi/USB 保持正常

**macOS + Windows 双系统用户**，本驱动已做跨系统接管硬化（MtkBt 0.2.5+）：

```
原理：蓝牙芯片会被任一系统的驱动加载固件并保持运行状态。
     切到另一个系统时，芯片处于"已运行/半加载"状态，
     旧版驱动直接下载固件会超时失败（MtkBtState=error, MtkBtWarmPath=half-loaded）。

修复：macOS 侧在下载固件前先做 subsystem reset（与 Linux btmtk 一致），
     把芯片强制清成干净状态再下载 → 无论之前是什么状态都能接管。

验证（实机双系统）：
  macOS 普通重启 → 蓝牙正常
  macOS 断电重启 → 蓝牙正常
  macOS → Windows（多次）→ Windows 蓝牙正常
  Windows → macOS       → macOS 自动复位接管，蓝牙正常
  Wi-Fi 与 USB 端口全程正常
```

**诊断命令**（蓝牙异常时）：
```bash
ioreg -rc MtkBt -l -w0 | grep -E 'MtkBt(PreReset|State|WarmPath|Tries)'
# 正常：PreReset=done  State=ready  Tries=1（无 WarmPath）
# 失败：State=error + WarmPath=half-loaded → 见下方硬件排障
```

**若仍遇到蓝牙+USB 同时消失**（USB 设备在硬件层不枚举）：见下一节硬件排障。

## 恢复方案（重要）

- OpenCore EFI 挂载后：`EFI/OC/RECOVERY/` 内含 **Windows 侧**一键脚本
  （`DISABLE-WIFI.bat` 禁用 5 个 kext 保开机，`RESTORE-LAST.bat` 回滚 MtkWlan）
  及 `RECOVERY-MTKWLAN.txt` 说明。
- 每次 `tools/install-efi.sh` 都会同步恢复工具包 + `*.before-*` 备份到 EFI。
- macOS 侧 `tools/restore-kexts.sh` 一键恢复。

## 已知限制

- WPA3-SAE / 802.1X 企业网 / 6GHz 实质连接 / AWDL(隔空投送) / 802.11r 无缝切换(FT) 未实现（见 OPTIMIZE-ROADMAP.md）
  （多 AP 同 SSID 的 **RSSI 选强漫游已实现并默认开启**，见 OPTIMIZATION-GUIDE-zh.md 完成区；非 802.11r 无缝）
- 驱动二进制随 EFI 更新，修改代码后需重启生效（macOS kext 机制）
- `MtkWlanNative` 只接受指定 macOS build（ABI 校验），其它 build 请用 Ethernet 前端

## AI 协作指南

给 AI 助手（如 DeepSeek / Claude / ChatGPT）使用本项目的提示词模板、调试流程、安全须知，
见 **[AI-GUIDE.md](AI-GUIDE.md)**。

## 许可

- 核心代码：ISC（部分文件 Clear BSD / GPL 兼容声明，见各子仓库 `NOTICE.md` / `LICENSE`）
- Linux mt76 / btmtk 参考：ISC / BSD-3-Clause-Clear
- MediaTek 固件：MediaTek 许可（`LICENSES/LICENCE.mediatek`）
- 分发二进制时保留 LICENSE / NOTICE / LICENSES

## 系统侧边界终审（2026-11-07 深夜，逆向资产全量就绪后）

> 详档：`docs/REVERSE-PATH-ANALYSIS.md` + `docs/FEASIBILITY-PATHS.md`；资产：`REV/`。

| 功能 | 终审结论 | 证据等级 |
|---|---|---|
| 剪贴板 Mac→iPhone | **结构性**：sharingd 的 CopyPaste 位由 AppleKeyStore **SE 会话签名**门控（符号级 `AKSRefBSignParam`）；无 SE 必然位 0、系统从不发起上传，真机行为一致 | 真二进制依赖+符号 |
| 蓝牙私域广播能力位 | 同源：广告位=SE 签名能力宣告；HCI 位/SFDeviceSupportsContinuity 均排除 | 反汇编 |
| AWDL / AirDrop / 随航 | **可修**（第三方 MT7922 参考实现已证明：复用 Family 内置 AWDL 引擎）；工程量大、数据面为难点、端到端未验证 | 参考实现在 `REV/reference/mt7922-awdl/` |
| WCM 服务注册 | **设计行为**：bluetoothd 弱引用 _WCMEnabled（真二进制导出存在），但启动闭包双机（本机+M5）均不加载 → 全平台不注册；MacCoex 正常工作 | vmmap 双机+真件 |
| E-4 扫描记录行 | ✅ **已 ship**：网络行显示 `b/g/n/ac/ax`（d23f0dd HE map 修补） | 实机 Profiler 行 |
| E-4 当前链路行 | 结构性：0x0E 接口内部聚合（三轮实验否定驱动表面） | 探针+kc 全量 |
| 漫游 advise 触发 | **系统设计静默**：自适应漫游真机同样零活动（M5 日志仅类注册）；ROAM_PROFILE(0xd8×1)+WCL 配置(0x1ae×2)实证在消费我们的调参 | 计数器+双机日志 |

**工程纪律不变**：变更只在驱动/kext/数据层；实验 1 重启一测；6GHz/WPA3/11kvr/LE Audio 不做。
