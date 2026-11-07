# IO80211Family 逆向资料归档（REV-NOTES）

> 本文件整理 2026-11-07 针对 **IO80211Family (1200.13.1)** 的逆向成果：命令映射、探针定位法、
> 槽位落点、结构来源与最终结论。目标是让后续任何维护者 / AI 不需要重新逆向就能：
> 1. 看懂被拒命令清单每个码是什么；
> 2. 快速实现某个 apple80211 命令（知道在哪改、结构怎么填、大小怎么取）；
> 3. 理解为何 AWDL 不可行（能力 gate）。

## 背景与工具链（本机验证可用）

- 目标二进制：`IO80211Family` 从 `BootKernelExtensions.kc` 的 fileset entry 提取并重定位为可导入 Mach-O。
- 逆向：**REA (Reverse Engineer Anything, morluto/rea) CLI v6.8.0** + **Ghidra 12.1.4**
  （需把 Ghidra 11.3.2 的 `os/mac_x86_64/decompile` 拷进 12.1.4，12.x 官方不再带 macOS 反编译器）。
- Ghidra 全量分析：12,341 函数、176,463 字符串、50 个 vtable。
- 私有结构布局：`itlwm` 的 `include/Airport/apple80211_ioctl.h`（OpenIntelWireless，社区完整反编译头）。
- 警告：AWDL 的 70 个 `apple80211get/setAWDL_*` 有反编译符号，但系统**从不发起**（见结论）。

## 一、被拒命令 → apple80211 命令 全量映射（0xcc8 探针法）

“探针法”：Family 里 `apple80211get/setX(skywalkIface, data)` 的骨架固定为
`(*(*iface + 0xcc8))(iface, cmd)`（命令码探针）→ 通过后调 vtable 槽（0xfd0/0xfe0/0x10d0…）。
用每个 handler 反编译中的 `0xcc8` 参数即可拿其 apple80211 命令码。

| 码 | 命令 | 说明 |
|---|---|---|
| 0x15a | getBEACON_INFO | 信标信息 |
| 0x0cf | getCHANNELS_INFO | 信道清单（实现，12 常用信道）|
| 0x0ff | getBTCOEX_PROFILE | 蓝牙共存 |
| 0x01c | getLOCALE | 区域 |
| 0x055 | setIE | 信息元素 |
| 0x1a8 | getWCL_FW_HOT_CHANNELS | 固件热点信道 |
| 0x174 | getBSS_BLACKLIST | BSS 黑名单 |
| 0x187 | getSLOW_WIFI_FEATURE_ENABLED | 慢 Wi-Fi 特性 |
| 0x176 | getTXRX_CHAIN_INFO | 收发链信息 |
| 0x0d8 | setROAM_PROFILE | 漫游配置（实现：accept）|
| 0x233 | getCONNECTION_ID | 连接 ID |
| 0x0c0 | setOFFLOAD_NDP | NDP offload（**未放行**，副作用）|
| 0x161 | getNSS | 空间流（实现：2 流）|
| 0x103 | getMAX_NSS_FOR_AP | btcoex AP 最大 NSS |
| 0x1ae | setWCL_ROAM_PROFILE_CONFIG | 漫游配置（实现：accept）|
| 0x0bf | setOFFLOAD_ARP | ARP offload（**未放行**，副作用）|
| 0x216 | getPLATFORM_CONFIG | 平台配置 |
| 0x188 | setPM_MODE | 电源管理（放行）|

> 361 个带探针的命令系统实际只请求 18 个；其余未放行以避免未知行为。

## 二、实现一个命令的落点（stub 体系）

- 所有纯虚 slot 由 `tools/kcabi/genhdr.py` 生成到 `abi/<build>/iface_stubs.inc`（由
  `src/MtkWlanNativeReq.cpp` 末尾 include）。
- 手写实现放 `src/MtkWlanNativeReq.cpp`，重跑
  `genhdr.py gen <dump> <out> --iface-class MtkWlanNativeIf --impl src/MtkWlanNativeIf.cpp src/MtkWlanNativeReq.cpp src/MtkWlanNative.cpp`
  会自动把已实现的方法从 stub 清单剔除。
- 已有手写的“排除检测”基于 `--impl` 文件内容；**手工改过 stubs.inc 的风险点**：
  `getHE_CAPABILITY` 当前是手改成 `kIOReturnSuccess` 的，重生成时会被改回 unsupported，务必保留。

### 写缓冲的安全准则
- 结构大小从 `itlwm apple80211_ioctl.h` 拿布局；
- 大小/上限从 Family 调用点拿（如 getNSS=8B、getCHANNELS_INFO=0x2720B）——写量远离边界；
- get 类用 `store<uint32_t>` / `store<uint8_t>` 按偏移写，set 类只回 success（不读/写 buffer）除非必须。

## 三、关键 vtable 槽位坐标
- `0xcc8`（探针命令码）、`0xfd0`(约 506)、`0xfe0`(约 508)、`0x10d0`(约 538) 为 Family 对驱动接口的调用槽；均落在
  `IO80211InfraProtocol` 的 vtable 内（672 槽，ABI 头覆盖到 669），可由 `MtkWlanNativeIf` override。
- `IO80211SkywalkInterface` 仅 465 槽；AWDL 的广播类（IO80211AWDLPeerManager 等）不在驱动可达路径。

## 四、AWDL 最终判定（多轮实机验证）
1. 系统**从不发起任何 `apple80211get/setAWDL_*`**（70 个命令 0 次调用）——被拒命令全是通用命令；
2. 能力 gate 在系统初始化路径（PeerManager 分支在 Apple 原生驱动注册后才会创建 awdl0 与协议实例），
   **第三方驱动无注册路径** → AWDL/AirDrop 传输维持“不可行”结论；
3. 详见 `AWDL-CONTINUITY.md` 全文。

## 五、可复用材料（留档）
- `AWDL-CONTINUITY.md`：AWDL 全链研究（契约、三阶段实测、外部项目评估、能力 gate 判定）。
- 本文件：命令→槽→实现速查。
- 16/18 命令放行清单即白名单注释（`src/MtkWlanNative.cpp::commandAllowed`）。
- `tools/kcabi/`：ABI dump / 比对 / 前端生成 原始工具链（用于 macOS 更新后的 ABI 维护）。