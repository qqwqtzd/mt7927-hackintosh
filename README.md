# MediaTek MT7927 (Filogic 380 / MT6639) Wi-Fi + Bluetooth for macOS Hackintosh

移植 Linux `mt76` / `btmtk` 的 MT7927 驱动到 macOS 26.2（Tahoe 26.7.2 / 25G332），
在 AMD Hackintosh（MSI MPG B850M EDGE TI MAX WIFI）上经 OpenCore 加载，完整可用。

## 仓库结构

```
mt7927-port/
├── WTLMtkWireless/      Wi-Fi 驱动（kext，fork/移植自 watchthelight-labs）
├── WTLMtkBluetooth/     Bluetooth 固件加载器（kext）
├── WTLWifiMenu/         原生 Wi-Fi 前端（MtkWlanNative，iOS 的 IO80211 菜单）
├── tools/               构建/安装/恢复/诊断脚本
├── dist/                已构建 kext + 当前版本
├── linux-ref/           Linux v7.2 mt76 参考源码（只读对照）
├── third-party/         LegionBtFix / BlueToolFixup 等
├── PORTING.md           移植细节与完整历史（逐项记的改动）
├── INSTALL.md           安装步骤
├── OPTIMIZE-ROADMAP.md  优化清单（完成/待定）
├── USB-INVENTORY.md     USB 端口清单
└── RECOVERY-MT7927.txt  恢复指引
```

三个子目录（`WTLMtkWireless` 等）是独立的 git 仓库（各有 origin），本仓库是它们的
"总装/工程"包装，方便一次性 clone 全部相关资产。

## 状态（2026-11-06）

- **Wi-Fi**：2.4/5GHz，WPA2，IPv6，802.11ax HE **160MHz**（协商 2064 Mbps），TXS 遥测，
  WMM 多队列，固件卡死自动恢复，断线自动重连，睡眠/唤醒驱动重建。
- **原生前端**：Wi-Fi 菜单、macOS 自动 join、private MAC。
- **蓝牙**：AirPods 音频稳定（HCI logging 已确认非必需并移除）。
- 详见 `PORTING.md` 第 16 节"当前最终状态"。

## 构建

```
bash WTLMtkWireless/tools/build-mac.sh      # out/MtkWlan.kext
bash WTLWifiMenu/build.sh                    # out/MtkWlanNative.kext
bash WTLMtkBluetooth/tools/build-mac.sh      # out/MtkBt.kext
bash WTLMtkWireless/tests/run.sh             # host 测试（5 组）
```

## 安装 / 恢复

```
sudo bash tools/install-efi.sh      # 装到带 OpenCore 的 EFI（自动备份+同步恢复包）
sudo bash tools/autotest.sh         # 开机自动采集诊断快照 → out/autotest/
```

活动 OpenCore EFI 在 **512GB SAMSUNG（MZVL4512HBLU-00BH1）** 盘上（disk 编号会变，
认容量/型号）。EFI 内 `EFI/OC/RECOVERY/` 有 Windows 侧一键禁用/回滚脚本。
详见 `PORTING.md` / `INSTALL.md` / `tools/recovery/`。

## 许可

见各子仓库的 LICENSE / NOTICE / LICENSES（ISC / Clear BSD / GPL 兼容声明），
以及 `LICENSES/LICENCE.mediatek`（MediaTek 固件）。