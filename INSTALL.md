# MT7927 macOS 安装与首次测试（通用版）

本文件是**通用安装流程**。机型相关的细节（USB map 布局、端口号、磁盘型号等）
因每台机器不同，请参考 `USB-INVENTORY.md`（含如何排查你的 USB map）。

## 0. 确认硬件被系统看到

```bash
# 网卡（PCIe）
ioreg -r -c IOPCIDevice | grep -i '14c3' || lspci 2>/dev/null
# 蓝牙（USB）
ioreg -p IOUSB -l -w0 | grep -E '"USB Product Name"|idVendor|idProduct'
```

- 期望看到 PCI `14c3:7927 / 14c3:6639 / 14c3:0738`（MT7927）或 `14c3:7925`（MT7925）
- 蓝牙是 USB 设备（`0489:e110` 或 `0e8d:0717` 等 MediaTek 常见 id）；**若没出现，
  先修 USB map**，别急着怀疑驱动。

## 0.5 先做 live test（推荐）

```bash
bash test-live.sh
```

它会现场验证驱动能不能加载、卡在哪个状态机（W1..W6），不写 EFI。

## 1. 备份 EFI

在改 OpenCore 前，先整体备份你的 EFI 分区（例如复制到桌面/移动盘）。

## 2. 放入 kext

将构建产物复制到 `EFI/OC/Kexts/`，并在 `config.plist` 的 `Kernel > Add` 按顺序加入
（`MinKernel` 按需）：

```
Lilu.kext              （如需要）
BlueToolFixup.kext
LegionBtFix.kext
MtkBt.kext
MtkWlan.kext
MtkWlanNative.kext
```

顺序很重要：`MtkWlanNative` 依赖 `MtkWlan` 的核心，`MtkBt` 需要 `BlueToolFixup`。

也可以用脚本自动做（自带备份 + 恢复包同步）：

```bash
sudo bash tools/install-efi.sh
```

## 3. NVRAM / boot-args

`config.plist > NVRAM > Add` 的 `7C436110-AB2A-4BBB-A880-FE41995C9F82` 里加：

```
mtkfe=native mtknativeif=1
```

- 想用原生 Wi-Fi 菜单（`MtkWlanNative`）必须有这两项；不想用就只装 `MtkWlan`，
  退到命令行/`mtkwifi` 工具（Ethernet 前端）。
- 蓝牙还需 `BlueToolFixup` 配合两个 NVRAM 键（见其 README）。

## 4. USB map

- 用 `usb-list.py` 或 `系统信息` 看你蓝牙/网卡所在端口。
- 用 USBToolBox 或你的现成 map；**确保 MediaTek 蓝牙所在端口在 map 里**。
- 15 口上限与端口细节参考 `USB-INVENTORY.md` 的排查方法，不要照抄机型表。

```bash
python3 usb-list.py
```

## 5. 重启与验证

重启后：

```bash
# kext 是否加载
kmutil showloaded --list-only | grep -E 'Mtk|Legion|BlueTool'

# Wi-Fi 状态机（目标：W6 pass）
ioreg -r -c MtkWlan -l -w0 | grep -E 'MtkState|MtkW8Mode|MtkW6TxFailed'

# 蓝牙
ioreg -rc MtkBt -l -w0 | grep -E 'MtkBtState|MtkBtWarmPath'

# 网络接口
ifconfig en1
```

期望：

- `MtkState = "W6 pass: en1 passing traffic"`
- `MtkW8Mode = "HE, ... MHz, 2 streams"`（若 AP 支持 160 则 160MHz）
- `MtkBtState = "ready"`
- en1 拿到 IP

## 6. 恢复 / 回滚

- 安装脚本每次自动备份到 EFI（`*.before-*`），并同步 `EFI/OC/RECOVERY/`。
- macOS 一键回滚：`sudo bash tools/restore-kexts.sh`
- Windows 侧应急：EFI 里 `EFI/OC/RECOVERY/DISABLE-WIFI.bat`（禁用驱动保开机）、
  `RESTORE-LAST.bat`（回滚 MtkWlan）。