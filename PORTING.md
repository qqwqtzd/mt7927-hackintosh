# MT7927 (Filogic 380 / MT6639) macOS 移植笔记

目标：在 macOS 26.7.2（25G332，x86_64）的 Hackintosh 上，为 MediaTek MT7927 提供
Wi-Fi 7 + Bluetooth 5.4 支持。

当前状态：**源码完成第一轮 bring-up，三个 kext 均可编译；MT7927 的 Wi-Fi PCIe 设备已出现
（`14c3:7927`，subsystem `105b:e0fe`，PCI 9:0:0），但尚未加载驱动验证。**

蓝牙 USB 半边也已通过 Windows 注册表确认存在：

```
USB\VID_0489&PID_E110&MI_00  MediaTek Bluetooth Adapter
LocationInformation = Port_#0017.Hub_#0004
父控制器 = PCI 11:0:0 1022:43fc (AMD xHCI)
```

主板是 MSI MPG B850M EDGE TI MAX WIFI。后置 4x10Gbps A、3x10Gbps C、4x5Gbps A，
前置 2x5Gbps A、4xUSB2.0、1x20Gbps C，内部 Wi-Fi/BT。后置大部分端口走 Realtek
RTS5429E / Genesys GL3523 / GL9905V，不在 `1022:43fc` 的 15 条 map 里；43fc 的 15 条
是 B850 芯片组自己的 root ports，都是检测到的有效端口。蓝牙是第 16 个（port 17）。

移动硬盘（Ugreen，`0bda:9201`）接在前置 USB 3.0 A，macOS 显示在 43fc SS0005，
由此确认：

- SS0001–SS0005（port 1–5）+ HS0001–HS0005（port 6–10）是 USB 3.x 配对；
- 前置 USB3.0 A = SS0005 + HS0005（另一个口是另一组 SS/HS）；
- HS0006–HS0009（port 11–14）是 4 个前置 USB 2.0 header（机箱未接）；
- HS0010（port 16）是 USB Audio；
- port 15 是 MSI input，port 17 是蓝牙，原 map 都缺。

用户要求去掉全部 4 个前置 USB2.0（HS0006–HS0009，port 11–14），加入 15/17：

- `dist/UTBMap-MT7927.kext`（13 条）：保留 SS 1–5、HS 1–5（USB3 配对）、HS 10（音频）；
  加入 port 15（MSI input）+ port 17（蓝牙）；去掉全部 4 个前置 USB2.0。
- 13 条远低于 macOS 26 的 15 条上限；不再需要 16/17 条实验版。

该版本补了 macOS Tahoe 的 `usb-port-number`，`port-count` 为 17。
Wi-Fi 的 `MtkWlan` 和蓝牙的 `MtkBt` 都已包含 `0489:e110`。

下一步：

1. 先运行 `sudo bash test-live.sh` 验证 Wi-Fi（只加载到当前内核，不写 EFI）。
2. 换推荐 map，重启确认 `0489:e110` 出现（见 `INSTALL.md` 第 4 节）。
3. 再加载 `MtkBt.kext` 验证蓝牙固件下载。

## 1. 为什么从 MT7925 驱动开始

Linux 的 MT7927 支持是加在 `mt7925` 驱动里的，不是新写一个驱动：

- Wi-Fi：PCI ID `14c3:7927` / `14c3:6639` / `14c3:0738`，MT6639 硅片。
- 与 MT7925 共享固件接口，但 CBTOP/CBInfra 复位、DMA ring、IRQ map、prefetch、
  band_idx、ASPM/runtime PM 不同。
- Bluetooth：USB 侧是 MT6639（chip id `0x6639`），使用不同的固件
  `BT_RAM_CODE_MT6639_2_1_hdr.bin`，并且固件包里有 9 个 section，只有
  `(dlmodecrctype & 0xff) == 0x01` 的 5 个是蓝牙段。

因此本项目基于：

| 仓库 | 用途 |
| --- | --- |
| `watchthelight-labs/WTLMtkWireless` | MT7925 的 macOS Wi-Fi 驱动（ISC），本项目的 Wi-Fi 基线 |
| `watchthelight-labs/WTLWifiMenu` | 原生 Wi-Fi 菜单前端（`MtkWlanNative.kext`），26.7.2/25G332 可直接构建 |
| `watchthelight-labs/WTLMtkBluetooth` | MT7925 的蓝牙固件加载器（`MtkBt.kext`），本项目改为同时支持 MT6639 |
| `jetm/mediatek-mt7927-dkms` + Linux v7.2 | MT7927 的寄存器、DMA、复位、固件、BT section 过滤参考 |
| linux-firmware | MT7927 Wi-Fi 固件 |
| ASUS MT7925/MT7927 Windows 驱动包 | MT6639 蓝牙固件（目前 linux-firmware 主分支还没有这个 blob） |

## 2. Wi-Fi 改动（WTLMtkWireless）

### 2.1 PCI 匹配与 chip id

`Info.plist` 的 `IOPCIPrimaryMatch` 增加：

```
0x792714c3 0x663914c3 0x073814c3
```

`MtkWlan.cpp`：

- 从 PCI config 读 device id，`14c3:7927`、`14c3:6639`、`14c3:0738` 视为 MT7927。
- 芯片寄存器 `MT_HW_CHIPID` 读回 `0x0000`、`0x6639`、`0x0738` 时，和 Linux 一样强制
  规范化为 `0x7927`，同时保留 `MtkRawChipId` 方便诊断。
- MT7927 关闭 PCIe ASPM L0s/L1（Linux `mt76_pci_disable_aspm`），并把 Link Control
  修改前后值发布为 `MtkPcieLinkCtlBefore/After`。

### 2.2 WFSYS 复位

MT7927 不走 `WFSYS_SW_RST_B`（`0x7c000140`），而是走 CBInfra：

- `MT7927_CBINFRA_RGU_WF_RST` `0x1f8600` bit 4 置位/清除；
- `MT7927_CBINFRA_MCU_OWN_SET` `0x1f5034` 写 1；
- 轮询 `MT7927_ROMCODE_INDEX` `0xc1604`，等待 `0x1d1e`；
- 复位前后通过 `MT7927_PCIE2AP_REMAP_WF_0_54` `0x21008` 读
  `SEMA_OWN_STA` `0x40000` / `SEMA_OWN_STA_REP` `0x40400`。

以上地址都小于 `0x200000`，在 WTL 的 `regAddr()` 里是 BAR 直接偏移，不需要 remap。

### 2.3 WFDMA

| 项目 | MT7925 | MT7927 |
| --- | --- | --- |
| RX MCU ring | 0 | 6 |
| RX data ring | 2 | 4 |
| 额外 WA ring | 无 | 7（`RXQ_DATA2`） |
| RX data IRQ | `BIT(2)` | `BIT(12)` |
| RX MCU IRQ | `BIT(0)` | `BIT(14)` |
| RX WM2 IRQ | 无 | `BIT(15)` |
| prefetch 表 | 0x0000/0x0040/... | 0x0000/0x0080/0x0100/... |
| GLO_CFG | 基线 | `ADDR_EXT_EN`、`FW_DWLD_BYPASS_DMASHDL`，清 `CSR_LBK_RX_Q_SEL_EN` |

- 新增 `rxWa` ring（index 7），在 `pollRx()` 里 drain 掉。WTL 驱动轮询 TX 状态，
  不消费 WA 事件，但 ring 必须有 buffer，否则固件可能停住。
- `mt792x_dma_disable` 的 MT7927 分支会写 `RST_DTX_PTR` / `RST_DRX_PTR`；
  `dma_enable` 只有 MT7925 写 `RST_DRX_PTR`，代码已按芯片区分。

### 2.4 固件

`tools/gen_firmware.py` 现在同时嵌入两组固件：

| 用途 | 文件 | SHA-256 |
| --- | --- | --- |
| MT7927 patch | `WIFI_MT6639_PATCH_MCU_2_1_hdr.bin` | `7a5aacf6...b65e` |
| MT7927 RAM | `WIFI_RAM_CODE_MT6639_2_1.bin` | `e841f5ed...582d` |
| MT7925 patch | `WIFI_MT7925_PATCH_MCU_1_1_hdr.bin` | 原值 |
| MT7925 RAM | `WIFI_RAM_CODE_MT7925_1_1.bin` | 原值 |

`MtkFw.cpp` 的 `loadPatch()` / `loadRam()` 根据 `isMt7927` 选择对应符号和长度。
`Info.plist` 版本升到 `0.13.0`。

### 2.5 DBDC 与 band_idx

- `hwInit()` 在 `macInitBand(1)` 之后发送 `UNI_MBMC_SETTING`（`cid=0x28`，payload
  4 字节 pad + TLV tag 0 / len 8 / `mbmc_en=1`），对应
  `mt7925_mcu_set_dbdc(&dev->mphy, true)`。
- MT7927 的 BSS basic `band_idx` 使用：2.4 GHz = 0，5/6 GHz = 1；空闲 BSS 用 0。
  MT7925 保持原来的 `0xff`。

## 3. 蓝牙改动（WTLMtkBluetooth）

- chip id 接受 `0x7925` 和 `0x6639`。
- 当 `MTK_BT_READ_DEV_ID` 读回 0 时，读 USB device descriptor 的 VID/PID，
  匹配 Linux `btmtk_mt6639_devs` 里的 ID 后强制 `dev_id=0x6639`：
  `0489:e13a/e0fa/e10f/e110/e116`、`13d3:3588`。
- 固件换成 `BT_RAM_CODE_MT6639_2_1_hdr.bin`
  （SHA-256 `669c5c99...bc76`，688341 字节）。
- 下载时对 MT6639 过滤 section：只发送 `(le32(section + 32) & 0xff) == 0x01`
  的段，其余跳过；否则 BT 子系统会挂到必须断电。
- subsystem reset 对 MT6639 接受复位后 chip id 读回 0（Linux 同样例外）。
- `Info.plist` 的 USB 匹配表已经覆盖上面这些 ID，无需再加。
- 新增 `MtkBtSectionsFiltered`、`MtkBtChipFamily` 诊断属性。

## 4. 构建

这台机器只有 Command Line Tools，没有完整 Xcode；WTL 三个 kext 都不需要 Xcode，
直接用 `clang` 构建：

```bash
cd ~/mt7927-port/WTLMtkWireless && bash tools/build-mac.sh
cd ~/mt7927-port/WTLMtkBluetooth && bash tools/build-mac.sh
cd ~/mt7927-port/WTLWifiMenu && NO_LINKCHECK=1 ./build.sh
```

产物：

- `WTLMtkWireless/out/MtkWlan.kext`
- `WTLMtkBluetooth/out/MtkBt.kext`
- `WTLWifiMenu/out/MtkWlanNative.kext`

已复制到 `dist/`，并附上各自的 `LICENSE` / `NOTICE` / `LICENSES`。

## 5. 已知限制 / 下一步

1. **没有硬件验证**。当前 macOS 的 `ioreg` 里没有 `14c3:7927/6639/0738`，只有
   RTL8126。先确认卡插好、BIOS 里 M.2 E-key 插槽启用、Windows/Linux 能看到
   `lspci -nn | grep 14c3`。
2. **没有 HE/EHT/320MHz**。WTL 基线只做 VHT 80MHz；6GHz 需要 HE，320MHz 需要 EHT。
   第一目标应是 5GHz VHT80 能扫描、关联、跑流量，之后再扩。
3. **没有 sleep/wake、runtime PM**。基线也没有；MT7927 的 runtime PM 在 Linux 里
   因和 BT 共享 CONNINFRA 电源域而被禁用。
4. **原生 Wi-Fi 菜单**需要 `MtkWlanNative.kext` + `mtkfe=native mtknativeif=1`；
   它只接受 26.7.1/25G241 和 26.7.2/25G332，本机是 25G332，匹配。
   如果前端失败，核心会回退到 Ethernet 前端，不会断网。
5. **蓝牙**还需要 `BlueToolFixup.kext`、NVRAM 两个 key，以及 USB map 覆盖 BT 端口。
6. **DBDC 命令**已按 Linux 发送，但失败只发布 `MtkW4Dbdc`，不阻止 bring-up；
   如果扫描/关联异常，先看这个值。
7. **band_idx** 目前按目标信道设置；如果 2.4GHz/5GHz 切换异常，检查
   `MtkW5` 日志和 BSS basic 的 band_idx。MT7927 的 5GHz 还需要两个 band 相关字段：
   - `TXD1_TGID`（`mt76_connac3_mac.h`：GENMASK(13,12)，不是 connac2 的 BIT(30)）；
     `txQueue()` 在 `tgtCh > 14` 时置 bit 12，否则固件会把 5GHz 帧排到 band 0，
     表现为 auth timeout、`MtkW6TxFailed` 增长。
   - `DEV_INFO` 的 `band_idx`：加入 5GHz 前按 Linux `mt7927_reconfig_band` 先
     `devInfo(false, 1)` 再 `devInfo(true, 1)`；对应 `MtkW5DevBandOff/On`。
   诊断：`MtkW9Txd1`（最近一帧 TXD1）、`MtkW9TxCh`、`MtkRocRfBand/DbdcBand`。
8. **ASPM** 在 macOS 上直接改 PCIe Link Control；如果 WFDMA 寄存器访问仍不稳定，
   在 BIOS 里关闭 PCIe ASPM，或检查 `MtkPcieLinkCtlAfter`。

## 6. Linux → macOS 对照

| Linux | macOS |
| --- | --- |
| `mt7925_pci_probe` | `MtkWlan::start` |
| `mt7927_wfsys_reset` | `MtkWlan::hwStart` 的 `isMt7927` 分支 |
| `mt7927_dma_prefetch_setup` | `MtkWlan::hwStart` 的 prefetch 分支 |
| `mt7927_wfdma_setup` | `MtkWlan::hwStart` 的 GLO_CFG 分支 |
| `mt7927_dma_init` | ring 索引 6/4/7 + `rxWa` |
| `mt7925_mcu_set_dbdc` | `hwInit()` 的 `uniCmd(0x28, ...)` |
| `mt7927_band_idx` | `bssInfo()` 的 `tgtCh > 14 ? 1 : 0` |
| `btmtk_usb_setup` 0x6639 | `MtkBt::setup()` |
| `btmtk_setup_firmware_79xx` section filter | `MtkBt::download()` |
| `btmtk_usb_subsys_reset` | `MtkBt::subsysReset()` |

## 7. MT7927 5GHz 验证（2026-11-06）

成功关联 5GHz `Maintain`（ch36, VHT80, 2 streams, 866 Mbps）：

- `MtkState = "W6 pass: en1 passing traffic"`
- `MtkTargetSsid = "Maintain"`，IP `192.0.2.112`
- `MtkRocRfBand = 2`、`MtkRocDbdcBand = 1`、`MtkRocCh = 36`
- `MtkW9TxCh = 36`，`MtkW9Txd1` 低 32 位含 `TGID=1`（如 `0x800C9001` / QoS 帧 `0x800D9001`）
- `MtkW2Sec`：m1/m2/m3/m4 全为 1
- `MtkW6TxFailed = 0`、`MtkW6TxDrop = 0`、`MtkW6TxStall = 0`、`MtkW6TxRingFull = 0`
- 实测 `ping -b en1 192.0.2.1`（1400 字节）0% 丢包；`curl --interface en1` 约 3 MB/s（与有线 en0 同量级）

IPv6：`en1` 只有 link-local，`networksetup -getinfo "Wi-Fi"` 显示 `IPv6 Router: none`；
`ping6 -I en1 ff02::1` 正常，说明 Wi-Fi 的 IPv6 二层没问题，是路由器未在该 SSID 下发 RA。

## 8. Wi-Fi 6 / IPv6（2026-11-06，测试版）

WTL 基线只做 VHT。这一版加入了 HE（802.11ax）关联路径：

- `parseAp()` 解析 HE Capabilities（ext id 35），保存 AP 的 MAC/PHY caps 和 RX MCS map
- 关联请求加入 HE Capabilities（2 streams，80 MHz，MCS 0-11）
- `STA_REC_HE_V2`（tag 0x19）、`UNI_BSS_INFO_HE_BASIC`（tag 5）、BSS phymode 的 AX2/AX5 位
- 速率信息按 HE MCS 0-11 计算（2SS 80 MHz MCS 11 = 1201 Mbps）
- 原生前端解析 HE map（+0xd4），`getMCS_VHT` 也返回 HE

验证（2026-11-06 重启后）：`MtkW8Mode = "HE, 80 MHz, 2 streams"`，`MtkW8He active 1`，
TX 速率 1032 Mbps（HE MCS9 2SS80），确认固件/AP 已按 HE 关联。`system_profiler` 仍显示
`802.11ac`，是原生前端的显示层还没实现 `getHE_CAPABILITY`（slot 543），不影响实际 HE 链路。

IPv6 定位（已找到根因）：

- 5 分钟抓包：`en0` 收到路由器 RA（`xx:xx:xx:xx:xx:xx`，前缀 `xxxx:xxxx:xxxx:xxxx::/64`）；
  `en1` 收到 0 个 RA、0 个入站组播。
- 从 `en0` 向 `192.168.31.255` 发广播 ping 后，`MtkW6RxTrans` 和 `MtkW2Sec` 的 `trans`
  从 868 增到 873，正好等于广播包数。说明广播/组播**已经到达驱动**，但在 `rxFrame()`
  的 `HDR_TRANS`（RXD2 bit 7）分支里因为 `tgtRsn` 被当成“未解密”丢掉。
- 修复：`HDR_TRANS` 帧在 `portOpen` 且硬件已解密（`decrypted`）时交给协议栈，
  不再因为 WPA2 一律丢弃；同时把 MT7927 RXQ_DATA2（ring 7）里的普通帧 dispatch，
  不再整环丢弃。新增 `MtkW9RxTransDec`、`MtkW9RxWa/RxWaFrames` 诊断。
- RFCR 的 DROP_MCAST 位两个 band 本来都是 0，不是根因。

验证（2026-11-06 重启后）：

- `en1` 获得 `240e:390:3e6:4550:14:67b3:2a69:c040` 和临时地址，默认路由 `fe80::f66d:2fff:fea8:f81%en1`
- `ping6 -I en1 fe80::f66d:2fff:fea8:f81` 和 `ping6 -I en1 2400:3200::1` 均 0% 丢包
- `curl -6 --interface en1 https://www.baidu.com` 返回 HTTP 200
- `MtkW2Sec trans = 0`（不再丢 HDR_TRANS 帧），`MtkW9RxTransDec` 持续增长
- `MtkW9RxRa` 仍为 0，因为 RA 走的是 HDR_TRANS 路径，RA 计数只在 802.11 路径里；
  有全局地址和默认路由即证明 RA 已被协议栈处理

显示层最终结论（2026-11-06，用户选择保持现状）：

- 实际链路 `MtkW8Mode = HE, 80 MHz, 2 streams`，速率 1032 Mbps；扫描记录
  `Maintain` 的 `physicalLayerMode = 64`（11AX）、`fastestSupportedPHYMode = 6`。
- 但 `system_profiler` 的当前网络仍显示 `802.11ac`，`Supported PHY Modes` 仍为
  a/b/g/n/ac。CoreWiFi 的 `CWFInterface` 返回 `PHYMode = 128`（0x80/ac）、
  `supportedPHYModes = 159`（0x9F）、`maxPHYModeDescription = 11AC`。
- 已测试：`getCARD_CAPABILITIES` 的 HE 位、完整 Wi-Fi 6E 能力表（会触发 WPA3/SAE，
  已回退）、扩展 BSS 的 VHT/HE map、`getMCS` 上报 MCS 11、Apple Wi-Fi 6E 的
  PHY 模式字（`d[6]/d[7]`、`d[0x0e]/d[0x0f]/d[0x10]`）。这些都不改变
  `CWFInterface` 的值；它走 CoreWiFi 自己的 IOReport/PHY-state 通道，第三方
  驱动未实现该通道。
- 结论：这是显示层的 CoreWiFi/IOReport 集成问题，不是链路问题；继续修需要
  单独的逆向工程项目。当前保留 MCS Index 11 的显示提示（实际 HE MCS9 /
  1032 Mbps），Wi-Fi 6、IPv6、WPA2 均正常。

## 9. 许可

- WTL 三个仓库：ISC（部分文件 Clear BSD / GPL 兼容声明，见各仓库 `NOTICE.md`）。
- Linux mt76 / btmtk 参考代码：ISC / BSD-3-Clause-Clear。
- MediaTek 固件：MediaTek 许可，见 `LICENSES/LICENCE.mediatek`。
- 分发二进制时保留 `LICENSE`、`NOTICE.md`、`LICENSES/`。

## 10. W-08 实测结论（2026-11-06）

- TX BA 窗口：`MtkTxBaWin` 可配置（Info.plist，默认 64，幂 2，16..256），host 测试覆盖钳制/ADDBA 参数。
- 实测请求 128：AP（小米路由器）只授予 64，`MtkW9Ba win=64`；BA 窗口由 AP 上限决定，驱动侧调大无效。已回滚 64。
- 内网质量实测：60×0.2s ping 192.0.2.1 avg 2.4ms / 0% loss / stddev 0.56ms。
- 观察项：`MtkW9Ba rx buffered>600, timeout>30` 在持续下载时出现，A-MPDU 重排压力偏大，
  若后续延迟类问题出现，先看此处。

## 11. W-10 WMM 多队列（2026-11-06）

- 完整链路已就位并经 host 测试：DSCP/VLAN-PCP → TID → AC → LMAC 队列（RFC 8325 精简映射），
  `edca()` 记录 AP 的 WMM 元素（apWmm），仅当 `MtkWmm + apWmm` 时 QoS 帧按分类走对应队列。
- 实测：`Maintain` AP 广播 WMM（apwmm 1），分流生效（`MtkW9Wmm` 统计），
  但当前全部流量 DSCP=0（tcpdump 采样确认），macOS 默认不标记，故全部落 BE（TID 0）。
- 结论：与 Linux/macOS 原生一致——无标记→BE 是正确的；带 DSCP/PCP 的 VoIP/视频应用会自动分流。
  未做端口乱标（避免加剧延迟）。`MtkW9Wmm` 保留作诊断。

## 12. S-1 睡眠/唤醒（2026-11-06）

- `setPowerState(0)`: `pmSleep()` on the gate - DMA off, interrupts off, WFSYS reset,
  bus master off (PCIe drops at sleep).
- `setPowerState(1)`: `pmWake()` - bus master on, `reload("wake")` (drv_own -> hwStart
  incl. FLR -> hwInit -> auto rejoin), tick timer restarted.
- 实测 2 次睡眠/唤醒：`MtkPmSleeps/Wakes=2`, `MtkPmWakeRc=0`, 自动回连 Maintain,
  `MtkW6TxFailed=0`, HE 80MHz 保持, BT ready。

## 13. S-3 / S-4（2026-11-06）

- S-3: bssAge keeps the wanted BSS in the cache (AP-restart friendly); verified boot, no
  symptom change, rejoin path intact.
- S-4: RX reorder pressure counters (slides/hwm) added to MtkW9Ba; 30 s download measured
  hwm 45/64, timeout 26, dup 3, slides 75 - healthy, no further tuning needed.

## 14. S-5 蓝牙（2026-11-06）

- 实测当前冷启动 MtkBtElapsedMs=3684（Lithium 无 subsys reset，OpenTries=1），
  warm path 有 section 复用（SectionsSkipped），无需提速改动。S 系列全部收尾。

## 15. P-1 160MHz（2026-11-06）

- parseAp: VHT op width >= 3 or HE 160 cap + VHT bit14 -> 160; center 42/106/155.
- assocIes advertises VHT 0x4000 + HE 160/RU; HE BW160 mcs map valid when negotiating.
- rate/display: 160 rates (2x80), MtkW8Mode "160 MHz". MtkMaxBw accepts 3 (default 3).
- 实测 Maintain (小米): HE 160MHz 2SS, 2064 Mbps negotiated, 1400B ping 0% loss
  7.6ms, 30s download TxFailed=0, BA win 64 established.

## 16. B-1 A2DP / HCI logging（2026-11-06）

- 结论：HCI logging workaround **不再是必需**。LegionBtFix 0.3.0（LE 地址 + BT clock）已解决
  AirPods 静音与崩溃，A2DP 数据流在 logging 全关后依然稳定（实测：连看视频 5 分钟无问题）。
- 逐步拆解（每步重连 + 看视频验证均稳定）：
  1) 关 HCIFileTraces（保留 StackDebug+RawAudio）→ 稳定
  2) 关 RawAudioTrace（仅 StackDebug）→ 稳定
  3) 全关（删除 com.apple.MobileBluetooth.debug）→ 稳定 ★
- 收尾：`/Library/Preferences/com.apple.MobileBluetooth.debug` 已删除，HCI 日志不再生成，
  tools/third-party/legionbtfix/bt-hci-logging.sh 保留为诊断工具（非必需）。
- 若未来 A2DP 断流复现，先在此步之后重开 StackDebugEnabled 排查（最小侵入）。

## 17. 当前最终状态（2026-11-06）

MT7927（MT6639）Wi-Fi + Bluetooth 在 macOS 26.7.2 上完整可用：

- **Wi-Fi**：2.4/5GHz 关联、WPA2、IPv6（修复 HDR_TRANS 组播）、802.11ax/HE（含 160MHz，实测协商 2064 Mbps）、
  TXS 遥测、RX 重排诊断、WMM 多队列（标准行为）、固件卡死自动恢复、断线自动重连、睡眠/唤醒驱动重建。
- **原生前端**（MtkWlanNative）：Wi-Fi 菜单、macOS 自动 join、private MAC、自动重连。
  「当前网络 PHY Mode 显示 802.11ac」为 CoreWiFi 显示层限制（实际为 HE），不影响功能。
- **蓝牙**：MtkBt（MT6639 固件下载）+ BlueToolFixup + LegionBtFix 0.3.0；AirPods 音频稳定，
  HCI logging workaround 已确认非必需并移除。
- **工具/工程**：tests/run.sh（5 组 host 测试）、tools/install-efi.sh（自动识别 512GB Samsung OpenCore EFI +
  每版备份 + 恢复工具包同步）、tools/autotest.sh（开机自动快照）、tools/restore-kexts.sh、
  tools/recovery/*（Windows 侧一键禁/恢复）、OPTIMIZE-ROADMAP.md（优化清单）。
- 已知遗留：如 PHY 显示、WPA3-SAE、802.1X、漫游、6GHz 等，见 OPTIMIZE-ROADMAP.md 待定项。

恢复预案（重要）：活动 OpenCore EFI 在 512GB SAMSUNG 盘（disk 编号会变，认容量/型号）；
`EFI/OC/RECOVERY/` 里有 DISABLE-WIFI.bat / RESTORE-LAST.bat 及说明；每次安装自动同步。
