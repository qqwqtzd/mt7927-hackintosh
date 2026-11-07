# MediaTek MT7927 (Filogic 380 / MT6639) Wi-Fi + Bluetooth for macOS Hackintosh

在 macOS（Tahoe 26.x 实测 / 更高或相近版本可尝试）上驱动 MediaTek MT7927 / MT6639
（AMD RZ717 及同类 M.2 Wi-Fi 6E 模块的对等芯片组）的内核扩展移植。目标：**任何
使用该网卡的黑苹果用户都可以按本手册自行构建、安装、排障**。

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
