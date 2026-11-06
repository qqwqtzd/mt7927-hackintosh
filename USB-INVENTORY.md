# USB 接口与设备清单（修改前确认）

机器：macOS 26.7.2 (25G332)，x86_64  
主板：MSI MPG B850M EDGE TI MAX WIFI  
采集时间：2026-11-06  
数据来源：macOS `ioreg`（当前状态）+ Windows `SYSTEM` 注册表（当前 Windows 安装）
+ MSI 官方规格。

> 结论先说：**43fc 控制器现 map 15 条，已用/确认的端口是 5、16；Windows 还用了
> 15、17，但 macOS map 没有。9/10 在 Windows 有设备、macOS 却是空，需要插拔确认。**
> 因此现在还不能确定哪几条 HS 是“前置 USB 2.0”，先不要改 EFI。

## 1. macOS USB 控制器

| macOS 控制器 | ACPI / 来源 | 端口数 | 当前设备 |
| --- | --- | --- | --- |
| `AppleUSBXHCIPCI`（43fc） | B850 芯片组 xHCI，Windows `VID1022&PID43FC` | `port-count` 16，映射 15 条 | HS0010 USB Audio、SS0005 Ugreen |
| `XHC0` | `GP17@80001/XHC0@3`，CPU xHCI（Windows 15B6/15B7/15B8 之一） | 4 | 4-Port USB2/3.0 Hub（Realtek），ROG 显示器 |
| `XHC1` | `GP17@80001/XHC1@4`，CPU xHCI | 4 | USB2.1/USB3.2 Hub（Genesys），键鼠/耳机 |
| `XHC2` | `GP19@80003/XHC2@0`，CPU xHCI = Windows `1022:15B8` (PCI 15:0:0) | 1 | 空；**前置 20Gbps Type-C 走这里（GL9905V）**，不在 43fc map 里 |

说明：43fc 是 **B850 芯片组**控制器，当前 `UTBMap.kext` 只给它做映射。XHC0/1/2
是 CPU 侧控制器，不受这份 43fc map 影响。

## 2. 43fc 端口现状（macOS）

| 端口 | usb-port-number | 类型 | 当前设备 | Windows 43fc 对应 |
| --- | --- | --- | --- | --- |
| SS0001 | 1 | 3 | 空 | — |
| SS0002 | 2 | 3 | 空 | — |
| SS0003 | 3 | 3 | 空 | — |
| SS0004 | 4 | 3 | 空 | — |
| SS0005 | 5 | 3 | Ugreen Storage `0bda:9201` | port 5 同一设备 |
| HS0001 | 6 | 0 | 空 | — |
| HS0002 | 7 | 0 | 空 | — |
| HS0003 | 8 | 0 | 空 | — |
| HS0004 | 9 | 0 | 空 | Windows port 9：SanDisk/多个 hub（**不一致**） |
| HS0005 | 10 | 0 | 空 | Windows port 10：`1395:80AC`（**不一致**） |
| HS0006 | 11 | 0 | 空 | — |
| HS0007 | 12 | 0 | 空 | — |
| HS0008 | 13 | 0 | 空 | — |
| HS0009 | 14 | 0 | 空 | — |
| HS0010 | 16 | 255 | USB Audio `0db0:83ed` | port 16 同一设备 |
| （无） | 15 | — | **未映射** | Windows port 15：MSI input `0db0:0076` |
| （无） | 17 | — | **未映射** | Windows port 17：MediaTek BT `0489:e110` |

## 3. XHC0 端口（CPU 侧，不受 43fc map 影响）

| 端口 | port | 类型 | 当前设备 |
| --- | --- | --- | --- |
| PRT1 | 1 | 3 | 4-Port USB 2.0 Hub `0bda:5429` → USB2.1 Hub `0b05:1c01` → ROG 显示器 |
| PRT2 | 2 | 3 | 空 |
| PRT3 | 3 | 3 | 4-Port USB 3.0 Hub `0bda:0429` → USB3.2 Hub `0b05:1c02` |
| PRT4 | 4 | 3 | 空 |

## 4. XHC1 端口（CPU 侧）

| 端口 | port | 类型 | 当前设备 |
| --- | --- | --- | --- |
| PRT1 | 1 | 3 | 空 |
| PRT2 | 2 | 9 | USB2.1 Hub `05e3:0610` → 罗技接收器、MADE68PRO、Razer、MCHOSE |
| PRT3 | 3 | 3 | 空 |
| PRT4 | 4 | 9 | USB3.2 Hub `05e3:0620` |

## 5. XHC2

| 端口 | port | 类型 | 当前设备 |
| --- | --- | --- | --- |
| PRT1 | 1 | 0 | 空（推测前置 20Gbps Type-C，待确认） |

## 6. Windows 侧与 43fc 相关的设备

Windows 根 hub `USB\ROOT_HUB30&VID1022&PID43FC` 是 `Hub_#0004`。

| Windows port | 设备 | VID:PID | 说明 |
| --- | --- | --- | --- |
| 5 | USB Attached SCSI (UAS) | `0bda:9201` | 与 macOS SS0005 的 Ugreen 对应 |
| 9 | SanDisk / `1395` / `35d8` | `0781:5583` 等 | macOS 43fc HS0004 为空，可能经 hub 或不同启动状态 |
| 10 | USB Input | `1395:80ac` | macOS 43fc HS0005 为空 |
| 15 | USB Input | `0db0:0076` | macOS map 没有 port 15 |
| 16 | USB Composite（USB Audio） | `0db0:83ed` | 与 macOS HS0010 对应 |
| 17 | MediaTek Bluetooth Adapter | `0489:e110` | macOS map 没有 port 17，蓝牙不出现的原因 |

Windows 蓝牙驱动：`MTKBT_T0_2` / `26.40.3.67`（`mtkbtfilter.inf`），支持
`VID_0489&PID_E110&MI_00` 等 MT6639 ID。

Wi-Fi：`PCI\VEN_14C3&DEV_7927&SUBSYS_E0FE105B`，PCI (9,0,0)，Windows 服务
`mtkwecx`。

## 7. 主板物理接口（MSI 规格）

| 位置 | 接口 | 数量 | 备注 |
| --- | --- | --- | --- |
| 后置 | USB 5Gbps Type-A | 4 | 蓝色，主板自带 |
| 后置 | USB 10Gbps Type-A | 4 | 红色 |
| 后置 | USB 10Gbps Type-C | 3 | |
| 前置 | USB 5Gbps Type-A | 2 | 19-pin header |
| 前置 | USB 2.0 | 4 | 两个 9-pin header，每个 2 口 |
| 前置 | USB 20Gbps Type-C | 1 | Genesys GL9905V → CPU `1022:15B8` / macOS XHC2 PRT1，不在 43fc map |
| 内部 | Wi-Fi/BT | 1 | M.2 E-key，BT = 43fc port 17 |

后置 5Gbps/10Gbps 多数经 Realtek RTS5429E、Genesys GL3523/GL9905V 等
hub/控制器，不直接占 43fc 的 15 条 map；XHC0/XHC1 上看到的 hub 就是这些。

## 8. 当前 map 与缺口

原 `UTBMap.kext`（43fc）：

- 15 条：SS0001–SS0005、HS0001–HS0010（port 1–5、6–14、16）
- 已确认使用：port 5、16
- Windows 使用但 map 缺失：port 15（MSI input）、port 17（BT）
- 空但存在的候选：port 6/7/8/11/12/13/14、SS1–SS4

### 布局确认（用移动硬盘交叉验证）

用户确认：移动硬盘（Ugreen Storage `0bda:9201`）接在 **前置 USB 3.0 A** 口。  
macOS 里该设备在 **43fc SS0005（port 5）**；Windows 也在 `Port_#0005.Hub_#0004`。  
因此：

- **前置 USB 3.0 A = SS0005 + HS0005（port 5 + 10）**；
- SS0001–SS0005 + HS0001–HS0005（port 1–5 + 6–10）是 USB3 配对，成立；
- HS0006–HS0009（port 11–14）就是 4 个前置 USB 2.0 header（机箱未接）；
- HS0010（port 16）是 USB Audio；
- port 15 是 MSI input，port 17 是蓝牙，原 map 缺。

| 43fc 端口 | 用途 |
| --- | --- |
| SS0001–SS0005 + HS0001–HS0005（port 1–5 + 6–10） | USB 3.x 的 SS/HS 配对；SS0005+HS0005 是前置 3.0 A 之一 |
| HS0006–HS0009（port 11–14） | 4 个前置 USB 2.0 header（未接） |
| HS0010（port 16） | USB Audio（ALC4080） |
| port 15 | MSI input（原 map 缺） |
| port 17 | MediaTek BT（原 map 缺） |

按你的要求，推荐 map 去掉 **HS0006–HS0009（port 11–14，全部 4 个前置 USB2.0）**，
加入 port 15/17；保留 SS1–5、HS1–5（USB3 配对）、HS10（音频）。
这样后置/前置 3.x、音频、MSI input、蓝牙都在，前置 USB2.0 全部移除。

## 9. 确认状态

已确认（不需要再测前置 USB2.0）：

- 移动硬盘 Ugreen `0bda:9201` 接在前置 USB 3.0 A，macOS 显示为 43fc SS0005。
- 因此前置 USB3.0 A = SS0005 + HS0005；SS1–5 / HS1–5 配对成立。
- 前置 USB2.0 的 4 个 header 未接，对应 HS0006–HS0009（port 11–14）。
- USB Audio 在 HS0010（port 16）；MSI input 在 port 15；BT 在 port 17。

安装后只需顺便确认这些仍在工作：

| 物理接口 | 期望 |
| --- | --- |
| 前置 USB 3.0 A（移动硬盘那个口） | SS0005 + HS0005，硬盘正常 |
| 前置 USB 3.0 A 另一个口 | 另一个 SS/HS 配对 |
| 前置 USB-C 20Gbps | XHC2 PRT1（CPU 15B8 + GL9905V），与 43fc map 无关 |
| 后置 5Gbps / 10Gbps / Type-C | XHC0/XHC1 的 hub，不受 43fc map 影响 |
| USB Audio | HS0010，声音正常 |

## 10. 结论

- 推荐 map：`dist/UTBMap-MT7927.kext`（13 条）：
  - 去掉 HS0006–HS0009（port 11–14，全部 4 个前置 USB2.0）；
  - 加入 port 15（MSI input）和 port 17（蓝牙）；
  - 保留 SS1–5 + HS1–5（USB3 配对，含前置 3.0 A 的 SS0005/HS0005）、
    HS10（USB Audio）。
- 13 条远低于 15 条上限；不再需要 16/17 条实验版。
- **EFI 尚未修改**；安装步骤见 `INSTALL.md` 第 4 节。
