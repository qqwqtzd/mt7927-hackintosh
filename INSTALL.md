# MT7927 macOS 安装与首次测试

本目录的 `dist/` 是已经编译好的第一轮 bring-up 版本：

```
dist/
  MtkWlan.kext            # Wi-Fi 核心（MT7925 + MT7927，Ethernet 前端）
  MtkWlanNative.kext      # 可选：macOS 原生 Wi-Fi 菜单，26.7.2/25G332
  MtkBt.kext              # 蓝牙固件加载器（MT7925 + MT7927/MT6639）
  BlueToolFixup.kext      # 让 bluetoothd 接受第三方控制器（acidanthera 2.7.2）
  UTBMap-MT7927.kext          # 13 条：去掉全部 4 个前置 USB2.0，加 MSI input 15 + 蓝牙 17
  firmware/               # 里面嵌入的 MT6639 固件，供核对
  LICENSE.* / NOTICE.* / LICENSES.*
```

> 重要：这是**未上机验证**的移植版本。先备份 EFI，最好保留一个能启动的 U 盘 EFI。

## 0. 先确认硬件被系统看到

当前这台机器已经能看到 Wi-Fi PCIe 设备：

```
pci14c3,7927@0   class 0x028000   subsystem 105b:e0fe   PCI 9:0:0
```

确认命令：

```bash
ioreg -l -p IOService -w0 | grep -iE '14c3|7927|6639|0738|mediatek'
```

蓝牙 USB 半边目前**没有**枚举到（`system_profiler SPUSBDataType` / `ioreg -p IOUSB`
里没有 MediaTek）。如果 Wi-Fi PCIe 设备也不见了：

1. 关机，确认 M.2 E-key 卡插好、天线接好。
2. 进 BIOS，确认对应的 M.2 E-key / Wi-Fi 插槽启用；有些 AMD 板把它和某个 SATA/PCIe 槽共享。
3. 在 Windows 或 Linux 里用 `lspci -nn | grep -i 14c3` 确认卡本身工作。
4. 如果 Windows/Linux 也看不到，先解决硬件/插槽，驱动无从下手。

蓝牙是同一张卡的 USB 半边。装好卡后先在 macOS 里看：

```bash
ioreg -p IOUSB -l -w0 | grep -E '"USB Product Name"|"idVendor"|"idProduct"'
```

应该出现 MediaTek（`0x0489` / `0x13d3` 等）。如果没有，先修 USB map，见第 4 节。

## 0.5 先做 live test（推荐）

先不要改 EFI。`test-live.sh` 只是把 kext 加载到当前运行的内核，重启就恢复原状；
即使驱动把机器搞 panic，重启后也不会再加载：

```bash
sudo bash /Users/links/mt7927-port/test-live.sh
```

它会把 `MtkState`、`MtkChipId`、`MtkW2/W3/W4`、`en1`、蓝牙状态和最近 2 分钟内核日志
打印出来。把整段输出贴回来即可定位。确认 Wi-Fi 能到 W6、蓝牙能到 ready，再按下面
步骤写进 EFI。

## 1. 备份 EFI

你已经有 `~/EFI-backup-*` 和 `~/EFI-mac-backup-*`，再加一份当前可启动 EFI：

```bash
diskutil list                       # 找 EFI 分区，例如 disk0s1
sudo diskutil mount disk0s1
cp -R /Volumes/EFI/EFI ~/EFI-before-mt7927-$(date +%Y%m%d-%H%M%S)
```

## 2. 放入 kext

把 `dist/` 里的 kext 复制到 EFI 的 `OC/Kexts/`：`MtkWlan.kext`、`MtkBt.kext`、
`BlueToolFixup.kext`，如果要用原生 Wi-Fi 菜单再加 `MtkWlanNative.kext`。

```bash
sudo cp -R ~/mt7927-port/dist/MtkWlan.kext          /Volumes/EFI/EFI/OC/Kexts/
sudo cp -R ~/mt7927-port/dist/MtkBt.kext            /Volumes/EFI/EFI/OC/Kexts/
sudo cp -R ~/mt7927-port/dist/BlueToolFixup.kext    /Volumes/EFI/EFI/OC/Kexts/
sudo cp -R ~/mt7927-port/dist/MtkWlanNative.kext    /Volumes/EFI/EFI/OC/Kexts/
```

用 ProperTree / OCAT 打开 `config.plist`，在 `Kernel > Add` 里加入以下条目。
顺序建议：`Lilu`（已有）→ `BlueToolFixup` → `MtkBt` → `MtkWlan` → `MtkWlanNative`。

`MtkWlan.kext`：

```xml
<dict>
    <key>Arch</key><string>Any</string>
    <key>BundlePath</key><string>MtkWlan.kext</string>
    <key>Comment</key><string>MediaTek MT7927 Wi-Fi (port)</string>
    <key>Enabled</key><true/>
    <key>ExecutablePath</key><string>Contents/MacOS/MtkWlan</string>
    <key>MaxKernel</key><string></string>
    <key>MinKernel</key><string></string>
    <key>PlistPath</key><string>Contents/Info.plist</string>
</dict>
```

`MtkBt.kext`：

```xml
<dict>
    <key>Arch</key><string>Any</string>
    <key>BundlePath</key><string>MtkBt.kext</string>
    <key>Comment</key><string>MediaTek MT7927 Bluetooth firmware</string>
    <key>Enabled</key><true/>
    <key>ExecutablePath</key><string>Contents/MacOS/MtkBt</string>
    <key>MaxKernel</key><string></string>
    <key>MinKernel</key><string></string>
    <key>PlistPath</key><string>Contents/Info.plist</string>
</dict>
```

`BlueToolFixup.kext`：

```xml
<dict>
    <key>Arch</key><string>Any</string>
    <key>BundlePath</key><string>BlueToolFixup.kext</string>
    <key>Comment</key><string>BlueToolFixup 2.7.2</string>
    <key>Enabled</key><true/>
    <key>ExecutablePath</key><string>Contents/MacOS/BlueToolFixup</string>
    <key>MaxKernel</key><string></string>
    <key>MinKernel</key><string></string>
    <key>PlistPath</key><string>Contents/Info.plist</string>
</dict>
```

`MtkWlanNative.kext`（可选，原生 Wi-Fi 菜单）：

```xml
<dict>
    <key>Arch</key><string>Any</string>
    <key>BundlePath</key><string>MtkWlanNative.kext</string>
    <key>Comment</key><string>MediaTek native Wi-Fi menu</string>
    <key>Enabled</key><true/>
    <key>ExecutablePath</key><string>Contents/MacOS/MtkWlanNative</string>
    <key>MaxKernel</key><string></string>
    <key>MinKernel</key><string></string>
    <key>PlistPath</key><string>Contents/Info.plist</string>
</dict>
```

## 3. 蓝牙的 NVRAM 与 boot-args

在 `NVRAM > Add > 7C436110-AB2A-4BBB-A880-FE41995C9F82` 里加：

```xml
<key>bluetoothExternalDongleFailed</key>
<data>AA==</data>                                   <!-- 1 byte, 0x00 -->
<key>bluetoothInternalControllerInfo</key>
<data>AAAAAAAAAAAAAAAAAAA=</data>                   <!-- 14 zero bytes -->
```

同时在 `NVRAM > Delete` 的同一 GUID 下加上这两个 key，避免旧值残留。

Wi-Fi 原生菜单需要把下面两个参数加到现有 `boot-args` 末尾：

```
mtkfe=native mtknativeif=1
```

只装 `MtkWlan.kext` 不装原生前端时，不需要这两个参数；驱动会把卡当作以太网口
（`en1`），可以用 WTL 仓库的 `tools/mtkwifi` 命令行工具扫描/连接。

## 4. USB map：15 口上限与 Windows 端口

macOS 26 的 `AppleUSBXHCI` 仍然限制**每个 XHCI 控制器最多 15 个映射端口**。
这里的 15 指的是 map 里的条目数（HS + SS 都算），不是物理插槽数；超过 15 的条目会被
忽略，或者让端口行为不稳定。`XhciPortLimit` 在 macOS 26 上不可靠（11.3 之后基本失效，
26 需要二进制 patch），不建议依赖。

Windows 注册表显示，`1022:43fc` 控制器（`Hub_#0004`）上实际使用的端口是：

| Windows port | 设备 | macOS map |
| --- | --- | --- |
| 5 | `0bda:9201` Realtek UAS | `SS0005` |
| 9 | `0781:5583` / `1395:*` / `35d8:*` | `HS0004` |
| 10 | `1395:80ac` | `HS0005` |
| 15 | `0db0:0076` MSI input | `HS0012`（新增） |
| 16 | `0db0:83ed` MSI composite | `HS0010` |
| 17 | `0489:e110` MediaTek Bluetooth | `HS0011`（新增） |

结合 **MSI MPG B850M EDGE TI MAX WIFI** 的规格：

- 后置：4x USB 10Gbps Type-A、3x USB 10Gbps Type-C、4x USB 5Gbps Type-A；
- 前置：2x USB 5Gbps Type-A、4x USB 2.0、1x USB 20Gbps Type-C；
- 内部：M.2 E-key 的 Wi-Fi/BT。

后置的大部分 10Gbps/5Gbps 走 Realtek RTS5429E、Genesys GL3523/GL9905V，不在
`1022:43fc` 这 15 条 map 里；43fc 的 15 条是 B850 芯片组自己的 root ports（包括各 hub
的上游和前置 header）。这 15 条都是检测到的有效端口，没有明显“无效”条目可删；蓝牙
port 17 是第 16 个。

你用移动硬盘确认了布局：移动硬盘接在前置 USB 3.0 A，macOS 显示在 43fc SS0005
（port 5）。因此：

- SS0001–SS0005（port 1–5）+ HS0001–HS0005（port 6–10）是 USB 3.x 配对；
- 前置 USB3.0 A = SS0005 + HS0005（另一个口是另一组 SS/HS）；
- HS0006–HS0009（port 11–14）是 4 个前置 USB 2.0 header（机箱未接）；
- HS0010（port 16）是 USB Audio；
- port 15 是 MSI input；
- port 17 是蓝牙（原 map 没有 15/17）。

按你的要求，**4 个前置 USB2.0 全部去掉**（`HS0006`–`HS0009`，port 11–14），
加入 port 15（MSI input）和 port 17（蓝牙）；保留 SS1–5、HS1–5（USB3 配对）、
HS10（音频）：

- **`dist/UTBMap-MT7927.kext`（13 条）**
  - 去掉 `HS0006`–`HS0009`（port 11–14，全部前置 USB2.0）；
  - 加入 `HS0012`（port 15，MSI input）和 `HS0011`（port 17，蓝牙）；
  - 保留 SS 1–5、HS 1–5（含前置 3.0 A 的 SS0005/HS0005）、HS 10（音频）；
  - 13 条，远低于 15 条上限；
  - 前置 20Gbps C 在 XHC2/GL9905V，后置 10Gbps 在 CPU/Realtek/Genesys 上，不受影响。

安装（先备份原来的）：

```bash
cd /Volumes/EFI/EFI/OC/Kexts
sudo mv UTBMap.kext UTBMap.kext.before-mt7927 2>/dev/null || true
sudo cp -R ~/mt7927-port/dist/UTBMap-MT7927.kext ./UTBMap.kext
```

`config.plist` 里保留原来 `UTBMap.kext` 的条目即可（BundlePath 不变），
`USBToolBox.kext` 继续保留。重启后确认：

```bash
ioreg -p IOUSB -w0 | grep -iE 'MediaTek|0489|Bluetooth'
```

出现 `MediaTek Bluetooth Adapter` / `0489:e110` 后，再跑：

```bash
sudo bash /Users/links/mt7927-port/test-live.sh
```

> 这个布局已经用你的移动硬盘确认：前置 3.0 A = SS0005 + HS0005。现在删的是
> HS0006–HS0009（port 11–14，4 个前置 USB2.0）。如果重启后发现前置 2x USB 3.0 A、
> 后置 5Gbps 或 USB Audio 不工作，告诉我具体哪个口。前置 20Gbps C 与后置 10Gbps
> 不在这个 map 上，不受影响。

## 5. 重启与验证

重启，在 OpenCore 里 Reset NVRAM（第一次装蓝牙 key 时建议做一次），然后：

```bash
# kext 是否加载
kmutil showloaded | grep -E 'MtkWlan|MtkWlanNative|MtkBt|BlueToolFixup'

# Wi-Fi 状态机
ioreg -r -c MtkWlan -l -w0 | grep -E 'Mtk(Pci|RawChip|Chip|State|W2|W3|W4|W5|W6|Fw|PcieLinkCtl)'

# 蓝牙状态机
ioreg -rc MtkBt -l | grep MtkBt
system_profiler SPBluetoothDataType | head -30

# 网络接口
ifconfig en1
```

期望的 Wi-Fi 关键属性：

```
MtkPciDeviceId      = 0x7927 / 0x6639 / 0x0738
MtkRawChipId        = 实际读回值（可能 0x0000 / 0x6639）
MtkChipId           = 0x7927xxxx
MtkState            = W6 interface attached ...（再往后 W5/W6 pass）
MtkW2 ...           = pass
MtkW3PatchResult    = 0
MtkW3RamResult      = 0
MtkW4Dbdc           = 0（失败也不应阻止，但需要看）
```

期望的蓝牙关键属性：

```
MtkBtChipId          = 0x6639
MtkBtChipFamily      = MT7927 (MT6639)
MtkBtState           = ready
MtkBtSections        = 9
MtkBtSectionsSent    = 5
MtkBtSectionsFiltered= 3
MtkBtBytesSent       = 488320
```

如果 `MtkBtState` 停在 `error`，看 `MtkBtStep`、`MtkBtLastError`、`MtkBtLog`。

## 6. 首次测试顺序（建议）

1. **只装 `MtkWlan.kext`**，不装原生前端，确认 `MtkState` 能到 `W6`。
2. 用 WTL 的 `tools/mtkwifi` 工具或一个开放网络，确认能扫描、关联、DHCP。
3. 再加 `MtkWlanNative.kext` 和 `mtkfe=native mtknativeif=1`，确认 Wi-Fi 菜单出现，
   能从菜单连接 WPA2。
4. 最后加 `MtkBt.kext` + `BlueToolFixup` + NVRAM key，确认蓝牙控制器出现。

一次只加一层，出问题容易定位。

## 7. 排错

| 现象 | 先看 |
| --- | --- |
| `ioreg` 没有 `14c3` 设备 | 卡/插槽/BIOS；先在 Windows/Linux 验证 |
| `MtkState` 停在 W1 | PCIe 电源、ASPM；看 `MtkPcieLinkCtlBefore/After`、`MtkDrvOwnTries` |
| W2 fail: wfsys reset | MT7927 CBInfra 复位路径；看 `MtkW2ConnOnMiscAfterReset`、`MtkWfsysSwRst` |
| W3 fail: patch/RAM | 选错固件或 DMA；看 `MtkFwPatchSha256`、`MtkW3PatchResult`、`MtkW3RamResult` |
| W4 fail | MCU 命令/DBDC/band_idx；看 `MtkW4Dbdc`、`MtkW4DevInfo`、`MtkW4Domain` |
| 扫描不到网络 | 天线、band_idx、regdomain；看 `MtkW4ScanReq`、`MtkScan` |
| 能扫描不能关联 | BSS band_idx、WPA2 状态机；看 `MtkW5`、`MtkW2Sec` |
| 蓝牙不出现 | USB map、`MtkBtState`、BlueToolFixup、NVRAM key |
| 蓝牙 `error` | `MtkBtLog`；确认固件是 `MT6639_2_1`，section filter 生效 |

抓完整日志：

```bash
bash ~/mt7927-port/WTLMtkWireless/tools/mtk-report.sh   # Wi-Fi，生成到桌面
ioreg -rc MtkBt -l -w0 > ~/Desktop/mtkbt.txt
log show --last 10m --predicate 'eventMessage CONTAINS "MtkWlan" OR eventMessage CONTAINS "MtkBt"' > ~/Desktop/mtk-log.txt
```

## 8. 回滚

- 在 `config.plist` 里把 `MtkWlan`、`MtkWlanNative`、`MtkBt`、`BlueToolFixup` 的
  `Enabled` 改为 `false`；
- 删除 `boot-args` 里的 `mtkfe=native mtknativeif=1`；
- 需要时恢复 `~/EFI-before-mt7927-*`；
- 蓝牙 NVRAM key 可保留，不影响其他设备。

## 9. 已知未完成

- 没有硬件验证；第一轮重点是 bring-up，不是 Wi-Fi 7 性能。
- 只做 VHT 80MHz，没有 HE/EHT/320MHz，也没有 6GHz。
- 没有 sleep/wake、runtime PM、roaming。
- 原生菜单需要 26.7.2/25G332（本机匹配）；其他 macOS build 需要重新生成 ABI 头。
- 蓝牙需要 BlueToolFixup 和 USB map；MT6639 固件来自 ASUS 厂商驱动包。
