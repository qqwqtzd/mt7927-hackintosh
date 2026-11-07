# UPSTREAM / 可回馈补丁集

本目录是 **可回馈社区的补丁包**：把本项目（MT7927 macOS niative 驱动）中通用性较强、
与上游同源的可复用改动，整理成独立补丁，方便作者把改动贡献给同许可的上游项目，
也方便其它基于相同前端（itlmt7925 / IO80211 native 前端）的开发者直接采用。

## 补丁内容

### roam-* — MT7927 自主漫游（WTLMtkWireless 核心）
| 补丁 | 内容 |
|---|---|
| roam-0001-add-autonomous-roaming.patch | 自主 Wi-Fi 漫游（多 AP 同 SSID），RSSI 迟滞选强 AP（默认关）|
| roam-0002-fix-picking-enable-by-default.patch | 修复候选锁（去掉 wantBssid 锁），默认开启 |
| roam-0003-changelog.patch | 版本日志 |
| roam-0004-tuning-delta8-absthreshold-hepref.patch | 调参：delta 8dB / 绝对阈值 -80/-75 / HE 优先 / 诊断 |

### cmd-* — IO80211 通用命令应答（WTLWifiMenu 前端）
| 补丁 | 内容 |
|---|---|
| cmd-0001-unblock-16-probes.patch | 放行系统高频探针的 16 个 apple80211 命令（含注释映射）|
| cmd-0002-implement-common-commands.patch | 实现 getNSS / getCHANNELS_INFO / setPM_MODE / setROAM_PROFILE / setWCL_ROAM_PROFILE_CONFIG + MAX_NSS_FOR_AP 放行 |

## 许可与兼容性

- 补丁代码许可：**ISC**（部分含 BSD-3-Clause-Clear 声明，源自 itlmt7925 参考）。
- **可回馈对象**：相同 / 兼容许可的 MTK-native 前端项目，如
  [thanhnndev/ga403wm-hackintosh](https://github.com/thanhnndev/ga403wm-hackintosh)（itlmt7925，BSD-3-Clause-Clear/ISC 系）、
  以及其它 ISC/BSD 的 IO80211/Skywalk 前端。
- **不可** 并入 GPL-2.0 项目（如 OpenIntelWireless/Itlwm）：ISC→GPL-2.0 为许可**不兼容方向**，勿直接搬运。
- 采纳前请保留各文件头部 SPDX 声明，并参照对应仓库的 NOTICE/LICENSES。

## 应用方法

```sh
# 目标前端仓库根目录
git am --directory=<原仓库相对子目录可忽略> UPSTREAM/patches/roam-*.patch
git am UPSTREAM/patches/cmd-*.patch
```

- `cmd-*` 涉及 `abi/<build>/iface_stubs.inc`（generated）：目标项目用不同 build 时，请以各自 ABI 重新
  gen 到对应目录，或仅采纳 `src/MtkWlanNativeReq.cpp` 的实现（把 stub 对应 slot 移除）。
- `roam-*` 仅涉及 `src/` 与 `Info.plist`，可直接应用。

## 配套参考

- 命令→槽→实现速查与逆向依据：根仓库 `docs/IO80211-REV-NOTES.md`。
- 漫游行为与调参理由：根仓库 `ROAM-AWDL-ANALYSIS.md`、`OPTIMIZE-ROADMAP.md`。

*整理：2026-11-07。补丁来自本项目已提交的小仓库定向 commit（`git format-patch`），未含任何本机信息。*