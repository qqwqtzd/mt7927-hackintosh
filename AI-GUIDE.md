# AI 协作指南（AI-GUIDE.md）

本项目是一个 macOS **内核扩展驱动移植**——修改、验证、排障的流程和普通软件很不同。
本文件写给**协助本项目的 AI 助手**（DeepSeek / Claude / ChatGPT 等）以及使用 AI 的人类维护者。

---

## 给 AI 助手：先读这些再动手

1. **`PORTING.md`**：本仓库最主要的上下文。它逐项记录了驱动与 Linux 的对应关系
   （`mt76` / `btmtk`），以及每次实验的结论（包括"试过但没用/导致开机失败"的反例）。
   **跳过它 = 可能重复踩坑**。
2. **`OPTIMIZE-ROADMAP.md`**：已做/待做清单，含每一项的收益与风险估计。
   改代码前先看对应项当前状态。
3. **`tests/`（WTLMtkWireless/tests）**：5 组 **host 测试**（WPA2/TXS/BA/WMM/supervisor）。
   **任何改动都要先跑 `bash WTLMtkWireless/tests/run.sh`**——它不重启机器、秒级完成。
   凡是能做成纯逻辑/字节解析的改动，尽量先用 host 测试覆盖再上机。

## 给 AI 助手：本驱动的工作方式（避免危险操作）

- 三个 kext 通过 **OpenCore 注入**，只能**开机加载**。改代码 = 编译 → `install-efi.sh`
  装到 EFI → **重启**才生效。没有热替换（macOS kext 机制限制，DriverKit 化是月级工程）。
- 每次安装会在 EFI 留下 `*.before-<time>` 备份；`EFI/OC/RECOVERY/` 有 Windows 侧
  一键禁用/回滚脚本。**在任何改动前，确认恢复路径存在**。
- 曾有两次开机失败的前科，根因都是"实验代码被编进了正式版"：
  **实验性改动必须保持其可控性**（默认关闭的开关、独立分支、host 测试），
  不许把未验证路径直接做成默认行为。
- `tools/install-efi.sh` 会**自动探测带 OpenCore 的 ESP**（活动 EFI 所在盘编号会变，
  按"含 EFI/OC/OpenCore.efi"识别，不写死磁盘号）。请勿改成硬编码盘号。

## 推荐调试循环（重启次数最少）

```
1. 读 PORTING.md / OPTIMIZE-ROADMAP.md 对应项
2. 能 host 测的先写 tests/ 用例并跑绿
3. 改内核代码 + 重新构建 + tests 全绿
4. install-efi.sh（自动备份+恢复包）
5. 用户重启 → 自动采集 out/autotest/<time>.txt（若装了 LaunchAgent）
   或让用户跑 ioreg / 粘贴快照
6. 以快照为准回环，不要一个属性猜到底
```

**给人类维护者的建议**：把"重启"当作稀缺资源。优先用 host 测试 + 诊断属性（MtkW* 系列）
缩小范围，一次重启携带尽量多的验证点。

## 常用诊断属性（ioreg -r -c MtkWlan -l -w0）

| 属性 | 含义 |
|---|---|
| `MtkState` | 状态机：W1..W6，`W6 pass` 表示通过 |
| `MtkW8Mode` | 当前模式：HE/VHT/HT + 带宽 + 流数 |
| `MtkW8Ap` / `MtkW8He` | AP 能力解析结果 |
| `MtkW6TxFailed` / `MtkW6TxFreed` | TX 失败/完成 |
| `MtkW9Txs` / `MtkW9TxsLast` | TX 状态遥测（速率/功率/重试）|
| `MtkW9Ba` | Block-Ack 会话（rx buffered / timeout / slides / hwm）|
| `MtkW9Wmm` | WMM TID/AC 分布 |
| `MtkStall` | 链路监督/固件重载计数 |
| `MtkPmSleeps/Wakes/WakeRc` | 睡眠/唤醒计数与结果 |
| `MtkRocRfBand` / `MtkRocCh` | ROC 授权频段/信道 |

蓝牙类似：`ioreg -rc MtkBt -l -w0 | grep MtkBt`（`MtkBtState` / `MtkBtWarmPath` 等）。

## 给 AI 助手：提交流程

- 子仓库（WTLMtkWireless 等）独立 git；改动分属哪个组件就提交到哪个仓库。
- 本根仓库只收文档/工具/脚本/补丁，不收构建产物与固件 zip（见 .gitignore）。
- 提交信息遵循既有风格：`mt7927: <动词短语. 一句话>（原因）`。
- **本地 git 提交**：每次改动都提交，保留完整历史与回滚点。
- **推送到 GitHub**：**只在维护者明确要求时才推送**（如"提交到 GitHub / 更新仓库"），
  绝不自动 push；维护者会按自己的节奏决定何时同步远端。

## 安全须知

- 固件/密钥/PMK 相关代码改动必须**先 host 测试**（WPA2 有 oracle 对照）。
- 不要在你的会话里粘贴用户的 GitHub token 等凭据；用环境变量/一次性命令。
- EFI 操作务必先卸载再拔盘，脚本已处理；人工操作请同样注意。
- 涉及"开机失败"风险时，先改 `DISABLE-WIFI.bat` 可救的配置，再出实验。

---

*最后更新：2026-11-06。随项目演进保持更新。*
