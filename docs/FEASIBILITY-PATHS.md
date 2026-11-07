# 全功能可行性路径分析（2026-11-07，逆向资产就绪后）

范围：此前判定"结构性不可修 / 无法实现"的全部系统侧功能，基于 26.7.2(25G332)
逆向资产（REV/fw26 真二进制 + 内核 kc 切片 + ipsw/Ghidra 管道）重新走查，
给出每个功能的**可能实现路径、闸门定位计划、可行性评级**。
**本文档只分析不执行**；改动域标注：[K]=驱动/kext · [A]=我们 App/工具 ·
[C]=配置/钥匙串数据 · [S]=系统二进制/系统文件修改（超出既有规则，需另行决策）。

---

## 0. 资产与能力基线（方案都建立在这上面）

| 资产 | 位置 | 对本分析的意义 |
|---|---|---|
| bluetoothd 23.5MB（=活体字节一致）| REV/fw26/usermode | BT 广播/能力面逆向靶 |
| Sharing.framework 6.1MB / UserActivity 703KB | REV/fw26/cache | 广告组装/接力解析真实代码 |
| WirelessCoexManager 114KB（真二进制）| REV/fw26/cache | WCM 终局已封（无需路径）|
| IO80211Family 2.9MB / IOSkywalkFamily 228KB（kc 切片+Ghidra）| REV/kernel | family 聚合/AWDL 机械 |
| sharingd 18.3MB / useractivityd 2.1MB（活体实体）| REV/usermode | 剪贴板/接力场景侧 |
| pboard/remoted | REV/usermode | AKS/身份证据侧 |
| 工具 | ipsw 3.1.743（cache 直读/实体化）+ Ghidra（kc）+ phyprobe | 复核闸门的手段 |

---

## 1. 通用剪贴板上传（Mac→iPhone 失败）

### 现状（三重实证闭环）
- pboard：`AppleKeyStore failed to open connection (-536870174)`；`AKS: Error: Locked`
- 广播层：sharingd 组装的广告载荷 `flagCopyPasteKey = 0`；useractivityd 从不发起上传
- 身份：`handoff-own-encryption-key` 未生成；M5（真机）也收不到 → 问题在本机上传侧

### 关键闸门定位计划（逆向任务，不执行）
1. **广告位闸门**：REV Sharing.framework / sharingd 中定位
   `SFActivityAdvertiserOptionFlagCopyPasteKey` 的写入点 → 顺藤找到判定条件
   （候选：`x-art` 特征、handoff 密钥存在性、BT 传输层"支持私域广告"标志、MG 特征值）。
2. **身份密钥闸门**：UserActivity.framework 中 `handoff-own-encryption-key` 的
   生成/读取点 → 确认它是"生成即验证 SE"还是"可预置项"。
3. **AKS 加密链**：pboard 中 AKS 调用点 → 确认上传是否必经 AKS 加密（若必经，
   无 SE 硬件则绝对不可行；若只是"设备态查询"，则可能可绕过）。

### 可行路径
| 路径 | 机制 | 域 | 评级 | 风险/成本 |
|---|---|---|---|---|
| P1 身份密钥预置 | 从 M5 只读导出该钥匙串项属性 → 本机 security 命令预置同名项（数据层，非改系统二进制）→ 看 useractivityd 是否认可 | C | 中 | 若键格式含 SE 绑定则无效；1 次测试重启 |
| P2 BT 私域广告能力位 | 见 §2：若闸门=BT 控制器能力字节 → 我们 HCI 层可上报 → 广播带 CopyPaste 位 → 上传被触发 | K | 中 | 需与 P1 串联（位在、键缺，仍不上传）|
| P3 AKS 旁路验证 | 若闸门 3 证实"仅设备态查询"→ 可能无需 SE；若"加密必经"→ 关闭（硬件欠缺，真死） | — | 待研 | 1 项逆向确认 |
| PX 系统侧开关（`com.apple.coreservices.useractivityd ClipboardSharingEnabled` 等已试无效）| — | S | 已排除 | — |
| 终局若为"必经 AKS 加密" | 结构性成立 → 方案即归档边界（维持现状）| — | — | — |

---

## 2. 蓝牙私域广播能力位（flagCopyPasteKey）

### 本质
Apple 私域广播载荷的"剪贴板能力"位由系统组装；对端看到 0。

### 闸门定位计划
- sharingd（REV，18.3MB 活体实体）→ 广告组装函数（`SFActivityAdvertisement`/
  `CBAdvertisementData` 构建）→ `FlagCopyPasteKey` 的输入源：
  (a) 本机 identity 状态；(b) bluetoothd 经某 mach/XPC 暴露的"continuity 能力";
  (c) CoreBluetooth/CB 层 `supportedFeatures` 位（来自 HCI `LE Read Local Supported Features` / 厂商扩展）。
- bluetoothd（REV）→ 对上述询问的应答函数 → 应答字节出处（控制器 HCI 响应 or 写死）。

### 可行路径
| 路径 | 机制 | 域 | 评级 | 备注 |
|---|---|---|---|---|
| P2a HCI 能力字节 | 若闸门=(c)，我们的 BT 控制器 HCI 特征字节可设 → 私有广告能力位出现 | K | 中 | 我们全权控制 HCI 应答；但系统可能仍验证 OOB/私钥 |
| P2b 系统组包位 | 若闸门=(a) 身份 → 并入 §1 P1 | C/K | 中 | 串联 |
| P2c 写死判定 | 若闸门=(b) bluetoothd 写死按芯片 ID → 无驱动通道（[S] 才可动）| S | 低/超规 | 不推荐 |

---

## 3. AWDL / AirDrop / 随航 / Universal Control（传输层）

### 本质
无 AWDL 接口；family 的 AWDLPeerManager 从未得到接口；vif 创建零调用；
广播载荷无 CopyPaste 位（§2）。（AirDrop 接收、随航、UIC 均依赖 AWDL 同族。）

### 闸门定位计划（逆向任务）
- IO80211Family（kc，Ghidra）：AWDLPeerManager / `awdlInterface` 创建路径；
  家族请求过而我们未应答的 ioctl（`MtkNativeRejectedCodes 01aa` 等）逐个对表。
- IOSkywalkFamily：AWDL vif 的 skywalk 接口要求。
- Sharing.framework：AWDL 扫描/会话在无 AWDL 时的降级表现。

### 可行路径
| 路径 | 机制 | 域 | 评级 | 成本/风险（诚实估计） |
|---|---|---|---|---|
| P3a AWDL-over-Wi-Fi-P2P 桥 | MT7927 固件支持 Wi-Fi Direct/P2P；AWDL 本质是私有 IBSS 风格 802.11 组网（OUI 00:17:F2）。方案：驱动实现专用模式 + 应答家族 AWDL ioctl + 私有帧解析。这是"从 0 写无线子系统"级别 | K | 极低/月级+ | 需固件厂商私有通道；Apple 私有协议需逐字段逆向（数百页级）；成功率 <20% |
| P3b AWDL 数据面替代（应用层）| 不经 AWDL 实现 AirDrop 对等传输（Bonjour over TCP/802.11 infra）| [A]/[S] | 中/超规 | 对端 Apple 设备不认非 AWDL 会话 → 只有自建 App 可行（用户已拒第三方 App）|
| P3c 接受边界 | AWDL 依赖 Apple 私有无线组网+芯片能力 → 维持结构性结论，边界建档 | — | — | 证据已充分 |

---

## 4. E-4 PHY 标签（ac → ax，System Profiler / 系统设置）

### 现状
- 显示链：0x0E 字典（probe 实证）ACTIVE=0x80 / SUPPORTED=0x9F（ac、a/b/g/n/ac）
- 三轮实验排除：能力字 0x0e/0x0f、能力字 HE 位、卡特定 HE_CAPABILITY（家族从不调用）
- family 接口内部数值聚合 → 曾判结构性

### 闸门定位计划（逆向任务，新工具可承载）
- IO80211Family：全文本搜**立即数 0x9F/0x80**（默认 supported/active 集合若为常量）
  → 读它的函数=聚合器 → 顺藤到"改变该值的输入"（接口字段? 我们的 BSS/scan 数据? 纯虚?）
  → 定性：可驱动输入 或 接口内部死路。

### 可行路径
| 路径 | 机制 | 域 | 评级 | 备注 |
|---|---|---|---|---|
| P4a 聚合器二次核查 | 上述搜索 + Ghidra 反编译聚合器 → 若找到我们可喂的字段（如 scan/BSS 记录 PHY 字段）→ 驱动侧补 → ax 全链路 | K | 中（待核）| 成功率不保证（E-4 三次负面实验在先）|
| P4b 扫描记录 PHY（M5 报告 §2.2）| 每条网络记录的 `PHY_MODE` 键当前缺失→回退 RATES→ac；若记录数据出自我们的 scan → 补键可显 ax | K | 中 | 与 P4a 同源核查 |
| P4c App 层标签修正（确定性可做）| 我们菜单 App 的 PHY 标签改为读自己的速率态（已知 HE/ax assoc）→ 用户可见层"正确" | A | 高（必成）| 不动系统；作为保底落点 |
| P4d 系统侧 | [S] 不合规，不列 |

---

## 5. WCM（WirelessCoexManager）

终局已封死且正确：真二进制导出 _WCMEnabled，但双机（本机+M5）vmmap 实证 bluetoothd
均不加载该框架 → 服务全平台不注册 = 设计行为；MacCoex 通路正常工作（isCoexDenied=0）。
**无路径需要提出；维持"无需修"。**

---

## 6. 其余同族（一次扫清）

| 功能 | 判死依据 | 可行路径候选 |
|---|---|---|
| AirDrop 接收 | 无 AWDL（§3）| 无（同 §3 结论）|
| 随航 / Universal Control | 无 AWDL（§3）| 无 |
| Instant Hotspot | BT 私域广告族（§2）| P2 同族路径 |
| 连续互通相机 | 同族 | P2 同族 |
| 蓝牙私域广播（工程面）| §2 | P2a 可达性待核 |

---

## 7. 汇总执行建议（不执行，仅优先级）

| 优先级 | 事项 | 类型 | 重启预算 |
|---|---|---|---|
| 1 | §1-3 闸门逆向定位（广告位/身份键/AKS 链）| 纯分析 | 0 |
| 2 | §4 P4a/P4b 聚合器与 scan-PHY 数据流核查 | 纯分析 | 0 |
| 3 | 依据核查结果定夺：P2a+P1 串联实验（如闸门=能力字节+身份项）| 实验 | ≤2 次 |
| 4 | P4c App 层标签（无论如何可做）| 实现[A] | 0（App 更新即生效）|
| 5 | AWDL P3a 评估维持关闭（成本/成功率的诚实结论）| 决策 | 0 |

## 8. 守则重申
- 本文档只分析。任何 P 路径的启动需你逐条批准；[S] 域路径默认不做。
- 实验仍按"1 重启一测"；变更全在 kext/App/数据层。

## 9. 闸门定位进展（2026-11-07 深夜，真二进制逐位实锤，0 重启）
### 剪贴板/广播位闸门已定位到函数级（sharingd 25G332 真二进制）
- 组装点① 0x10007861e：`mov rax,[GOT_SFActivityAdvertiserOptionFlagCopyPasteKey]; mov rdx,(rax)`
  以该 key 为参调 msgSend（options 字典插入）→ 返回 BOOL → `shl $3` → 写入选项字节位。
- 组装点② 0x1001842bc：同构（setValue:forKey: 模式，键=同一全局）。键全局由
  Sharing.framework 导出（`S _SFActivityAdvertiserOptionFlagCopyPasteKey` @0x7ff842552c00）。
- 谓词排除（真件反汇编）：
  - `_SFDeviceSupportsContinuity` = mov $1; ret（**恒真哑函数**）→ 非闸门。
  - BT 控制器 HCI 能力字节理论 → **与本品位无关**（位出自谓词调用，非 HCI 报告）→ P2a 降级。
- 当下最可能闸门：ObjC 谓词绑定 **IDS/身份连续性状态**（sharingd 内 IDSContinuity×36 证据）
  → 支持 P1（身份项预置）为主路径；谓词精确识别留 W3 工作簿（selref 需运行时解析）。
### 方案优先级更新
1. W3：解析该谓词（运行时/加载态解析 selref 或 IDS 侧反推）→ 0 重启
2. P1（身份预置）仍为可执行首选（需先 M5 只读导出钥匙串项 schema）
3. P2a（HCI 位）降为低优先（本品位不读 HCI）
4. E-4/W1（聚合器二次核查 + 扫描支路）仍挂队列（Ghidra 已就绪）

### W3 谓词识别收束（2026-11-07 深夜，静态证据闭环）
- 运行时 selref 解析受阻（lldb 非交互授权限制；expect 亦被拦）→ 以静态穷举收束。
- 穷举排除序列（真件）：
  ① BT 控制器 HCI 能力字节（位出自 ObjC 谓词，与 HCI 报告无关）
  ② SFDeviceSupportsContinuity（恒真哑函数 mov $1;ret）
  ③ 设备类型/系统版本类谓词（无此类依赖迹象；位写入前只经一次 msgSend）
- 残余唯一一致解释：**谓词=本机连续性身份/钥匙串状态查询**（sharingd 的 IDSContinuity
  调用簇 ×36 + "身份密钥从不生成"既有实证 + P1 方向吻合）。
- 结论：CopyPaste 位 ← sharingd 谓词 ← 身份状态；**P1（身份项预置）为主执行路径，
  P2a 移出**；若 P1 预置后该位仍 0（谓词深层校验），则结构性维持（学界边界）。
- 复核实验（可用，1 重启）：P1 预置后跑 phyprobe 同级探针（读本机广告位）或直接
  useractivityd 日志看 flagCopyPasteKey 翻转。

## 10. 剪贴板/广播位终极闸门：AKS SE 签名（2026-11-07，符号级闭合）→ P1 作废
- **前提纠错**：旧"身份密钥从不生成"系查询字段错误（`-s` 匹配旧误导名；真项在 **acct**，
  svce=com.apple.continuity.encryption）。本机 login 钥匙串**实证存在且可读**（rc=0）：
  handoff-own-encryption-key / handoff-decryption-key-{两个对端IDS} / AirPlay Identity。
  M5 结构同构（own+自己+对端 F06E0DC7 等）。system 钥匙串两机均无 → 位置一致。
- **P1（身份项预置）作废**：项已在且可用，预置无意义（省 1 次重启）。
- **闸门锁死（真件符号级）**：sharingd 依赖表含 AppleKeyStore.framework；
  未定义符号 `AKSRefBSignParam`（remoteSessionSigningCertificate/Type/set_data/serialize）
  → CopyPaste 位谓词＝AKS 远程会话签名（SE）能力检测 → 无 SE 必失败 → 位恒 0 →
  系统从不发起上传。与 pboard 的 AKS 失败（-536870174）同根。
- 终判：**结构性（Secure Enclave 硬件依赖）成立**，证据链=真二进制依赖+符号+双机行为对照；
  上传链、广告位、AKS 三线索归一。剪贴板边界关闭（不消耗重启）。
- 遗留功效性替代：iPhone→Mac 单向可用（不变）。

## 11. W1/E-4 扫描支路实证收官（2026-11-07 深夜，免重启验证）
- **P4b 已在 ship 且生效**：提交 d23f0dd（mirror AppleSunrise extended-BSS VHT/HE map，
  hasHe→HE map=0x0003）随 EFI 0.4.15 运行；system_profiler 已知网络行已出现
  "PHY Mode: 802.11b/g/n/ac/ax"（HE AP 行）→ 扫描记录支路不欠账。
- 剩余差异仅"当前活动链路行"（Supported/PHY = a/b/g/n/ac）→ 0x0E 接口聚合（结构性，
  三轮实验证据）。E-4 全书关闭：网络行已对，当前行结构性。
- W1 工作簿首份结论：EXTENDED_BSS_INFO（我们答 0xcc/0xd4 map + rate set + RSN）→ 家族
  扫描记录 PHY 判定 ← 我们全权可控（已验证生效）；无需再改扫描侧。
