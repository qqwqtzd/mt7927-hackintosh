# 项目文档索引（INDEX）

> 阅读顺序建议：**1 → 2 → 3 → 4 → 专题**。每个文档一句话导读。
> 更新：2026-11-07 深夜（逆向资产全量就绪后终版）

## 入门与总览

| # | 文档 | 一句话导读 |
|---|---|---|
| 1 | [README.md](../README.md) | 项目总装工程：硬件前提/快速开始/功能实测终审表（含系统侧边界） |
| 2 | [OPTIMIZE-ROADMAP.md](../OPTIMIZE-ROADMAP.md) | 总进度与决策簿：已完成/已判定/待触发/剩余可选 |
| 3 | [INSTALL.md](../INSTALL.md) | 安装/升级/回滚手册（EFI 自动备份 + Windows 侧恢复） |
| 4 | [VERIFY-MATRIX.md](../VERIFY-MATRIX.md) | 验证矩阵：每项功能的实测口径 |

## 实现与维护

| 文档 | 导读 |
|---|---|
| [PORTING.md](../PORTING.md) | 逐项移植改动与 Linux 驱动对照（含已知限制） |
| [OPTIMIZATION-GUIDE-zh.md](../OPTIMIZATION-GUIDE-zh.md) | 调参指南（漫游/功耗/性能） |
| [MT7927-CAPABILITIES.md](../MT7927-CAPABILITIES.md) | 芯片规格/Wi-Fi7 现状/完善方案 |
| [AI-GUIDE.md](../AI-GUIDE.md) | 给 AI/维护者的工程背景与改动纪律 |

## 逆向与系统侧（重点）

| 文档 | 导读 |
|---|---|
| [REVERSE-PATH-ANALYSIS.md](REVERSE-PATH-ANALYSIS.md) | **逆向主档案**（12+ 节）：AWDL/WCM/E-4/剪贴板/广播层的全证据链与终判 |
| [M5-PHYMODE-REPORT.md](M5-PHYMODE-REPORT.md) | M5 真机 PHY 显示链完整反汇编报告（含驱动改动清单） |
| [FEASIBILITY-PATHS.md](FEASIBILITY-PATHS.md) | 全部"判死项"的实现路径重审：闸门定位/schema/评级（分析不执行） |
| [SYS-SIDE-WORKPLAN.md](SYS-SIDE-WORKPLAN.md) | 系统侧工作簿 W1-W5 状态（W1 已收官） |
| [IO80211-REV-NOTES.md](IO80211-REV-NOTES.md) | 家族代码逆向笔记（命令码/结构/探针） |

## 历史专题

| 文档 | 导读 |
|---|---|
| [ROAM-AWDL-ANALYSIS.md](../ROAM-AWDL-ANALYSIS.md) | 漫游方案 + AWDL 可行性（驱动侧 RSSI 漫游已实现） |
| [AWDL-CONTINUITY.md](../AWDL-CONTINUITY.md) | 连续性功能（Handoff/AirDrop）分析 |
| [USB-INVENTORY.md](../USB-INVENTORY.md) | USB 安装盘资源盘点（15.6.1 已移至 REV/legacy 停用） |

## 逆向资产（REV/，均 25G332 真二进制）

| 目录 | 内容 |
|---|---|
| `REV/kernel/` | 13 个内核 kext 切片（kc REGION 提取） |
| `REV/fw26/cache/` | 用户态框架实体化 8 件（CoreWiFi 6.7MB/Sharing/WiFiPolicy/WCM 等，--objc 版带方法符号） |
| `REV/fw26/usermode/` | 守护进程（bluetoothd/sharingd/useractivityd…，与活体逐字节一致） |
| `REV/tools/` | kc-slice.py / phyprobe.c / 分析模板（kcq-*） / token 图谱 |
| `REV/scratch/` | 反编译产物暂存 |
| `REV/legacy/` | fw15-15.6.1（**停用存档**，仅查历史） |

## 工具链（≈rea-tools，不入 git）

- ipsw 3.1.743（`~/.rea-tools/bin/ipsw`）：dyld cache 直读/实体化/ota 解包
- Ghidra 11.3.2 + kc 项目（`~/rea-tools/kcproj`，可脚本重建）
- phyprobe（Apple80211CopyValue 探针）；m5.sh（M5 只读管道）
- 下载规范：GitHub 一律走本地代理 127.0.0.1:7893（见 ~/rea-tools/notes/net-mirrors.md）
