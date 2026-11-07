# MT7927 验证矩阵（VERIFY-MATRIX.md）

每次**改动驱动/调参/升级**后，按本表逐项验证，确认无回归。与 `tools/diag.sh` 配合：
diag 负责采集，本表告诉你**该看什么值**。跑 diag 后按下面命令核对即可。

> 用法：逐项执行命令，与"期望"一致打 ✅，不一致记下差异交给 AI 分析。
> host 测试（第 0 组）每次**构建前**必须全绿，其余按改动范围抽测。

## 第 0 组：构建自检（不重启，必跑）

| # | 命令 | 期望 |
|---|---|---|
| 0.1 | `bash WTLMtkWireless/tests/run.sh` | `supervisor: all passed`、`oracle: all values agree` |
| 0.2 | `bash WTLMtkWireless/tools/build-mac.sh` | `built org.watchthelight.MtkWlan ...` 无 error |

## 第 1 组：驱动加载与状态机

| # | 命令 | 期望 |
|---|---|---|
| 1.1 | `kmutil showloaded --list-only \| grep -E 'Mtk\|Legion\|BlueTool'` | MtkWlan / MtkWlanNative / MtkBt / LegionBtFix 都在 |
| 1.2 | `ioreg -r -c MtkWlan -l -w0 \| grep MtkState` | `W6 pass: en1 passing traffic` |
| 1.3 | `ioreg -r -c MtkWlan -l -w0 \| grep MtkFwVersion` | 形如 `____000000-YYYYMMDD...`（构建日期合理）|
| 1.4 | `ifconfig en1 \| grep status` | `status: active` |

## 第 2 组：Wi-Fi 连接

| # | 命令 | 期望 |
|---|---|---|
| 2.1 | `ioreg -r -c MtkWlan -l -w0 \| grep MtkW8Mode` | `HE, 80/160 MHz, 2 streams`（5G），`HE, 20/40 MHz, 2 streams`（2.4G）|
| 2.2 | `iperf3 -c <局域网服务器>` 或大文件拷贝 | 160MHz 5G 吞吐在百 MB/s 量级以上 |
| 2.3 | `ping -b en1 <网关> -c 60` | 0% 丢包，延迟稳定（<3ms 抖动小）|
| 2.4 | IPv6：`ping6 <路由器 IPv6>` / `curl -6` | 连通（HDR_TRANS 组播修复后）|
| 2.5 | 关联 SSID：`ioreg -r -c MtkWlan -l -w0 \| grep MtkTargetSsid` | 正确 SSID |

## 第 3 组：漫游（多 AP 环境）

| # | 命令 | 期望 |
|---|---|---|
| 3.1 | `ioreg -r -c MtkWlan -l -w0 \| grep MtkRoam` | `MtkRoam=1`，Delta=8，AbsRssi=80/75，PreferHe=1 |
| 3.2 | `ioreg ... \| grep MtkW5RoamEvals` | 随时间增长（15s 一轮后台扫描）|
| 3.3 | 信号好时（>阈值）：观察 Evals 增长但 `MtkW5Roam` 不变 | 绝对阈值兜底，不无谓切换 |
| 3.4 | 走到弱信号区（< -80）：等 1-2 轮扫描 | `MtkW5Roam` +1，回连后 RSSI 明显更好 |
| 3.5 | 漫游断线时间 | 1-3 秒（断+重连，非无缝 FT，预期内）|

## 第 4 组：稳定性

| # | 命令 | 期望 |
|---|---|---|
| 4.1 | 合盖睡眠 1 分钟 → 唤醒 | `MtkPmSleeps/Wakes` +1，`MtkPmWakeRc=0`，自动回连 |
| 4.2 | 重启 AP / 换信道 | 驱动在 `MtkStall` 监督下自动回连（不断网超 10s+）|
| 4.3 | 长时间运行（>8h）| `MtkW6TxFailed` 不持续增长、无固件重载（MtkStall 计数不跳变）|

## 第 5 组：蓝牙

| # | 命令 | 期望 |
|---|---|---|
| 5.1 | `ioreg -rc MtkBt -l -w0 \| grep MtkBtState` | `ready` |
| 5.2 | 连 AirPods 播放音频 5 分钟 | 无断续（HCI logging 已确认非必需）|
| 5.3 | 连键盘/鼠标（LE）| 正常输入，无掉线 |

## 第 6 组：原生前端（可选，装 MtkWlanNative 时）

| # | 命令 | 期望 |
|---|---|---|
| 6.1 | 顶部菜单栏 Wi-Fi 图标 | 显示当前网络、信号条 |
| 6.2 | 菜单切换网络 | 能列出并切换（MtkNativeRejected 计数不狂涨）|
| 6.3 | `ioreg -rc MtkWlan \| grep PhysicalLayerMode` | `physicalLayerMode=64`（11AX；显示层仍显示 ac 属已知限制 E-4）|

## 第 7 组：恢复路径（改动前确认存在）

| # | 检查 | 期望 |
|---|---|---|
| 7.1 | EFI：`ls EFI/OC/Kexts/MtkWlan.kext.before-*` | 至少 1 个备份 |
| 7.2 | EFI：`ls EFI/OC/RECOVERY/` | DISABLE-WIFI.bat / RESTORE-LAST.bat 存在 |
| 7.3 | macOS 侧 | `sudo bash tools/restore-kexts.sh` 可执行 |

---

## 变更记录

| 日期 | 版本/改动 | 结果 |
|---|---|---|
| 2026-11-07 | 漫游调参（delta 8 / 绝对阈值 / HE 优先）装机后 | 1.1-1.4、2.1、3.1-3.2 ✅；漫游切换待 3.4 日后观察 |
