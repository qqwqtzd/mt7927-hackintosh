# AirDrop 网络链路只读调查与后续实施方案

日期：2026-11-01。范围：MT7922SkyWalk 当前 AngusCore 源码，重点是 `MT7922_NATIVE_DARWIN == 25`（项目中 Darwin 25 / macOS 26 路径）；参照 `/Users/angus/Desktop/80211brcm/反编译资料汇总/IDA/AWDL_NATIVE_INTEGRATION.md` 及各版本既有记录。本文是调查和实施计划，不代表修复或新硬件测试。本次只新增本文，没有修改驱动、构建或运行设备测试。1.2.39 记录为当前目录中最新的 AirDrop 专题版本记录，不等同于当前机器实际加载版本。旧 STATUS 开头、旧 README 的版本状态与后续演进并存，应按具体版本和源码判断。

## 1. 分层流程和证据边界

Finder/sharingd 发起 AirDrop → BLE 邻近发现 → airportd/CoreWiFi 申请 AWDL → WCL 虚拟接口请求 → role 6 proximity 接口及 awdl0 注册 → IO80211AWDLPeerManager configure/enable、模板、选举、频道序列 → MT7922 次级 BSS、ROC grant、PSF/MIF 发送及 Action 接收 → Family 消费 Action、建立 peer/服务状态 → 对端共同频道/时隙 → WTBL 和 AWDL 数据帧 → awdl0 IPv6 邻居发现、mDNS、TCP → sharingd 传输。

BLE 发现和 Wi-Fi 数据链路需要分别确认；不应把一种的成功视为另一种成功。反编译资料显示 Family 有 peer、选举、AirDrop traffic registration、TLV 和 UMI 状态机；BCM 的 iovar 只作为功能参照，不是 MT7922 固件 ABI。资料指出 AirDrop TLV 发送类型 0x23、接收分派 0x21，原因未知，应以目标版本和抓包验证。

证据等级：**源码事实**指可直接从当前代码得出；**历史硬件观察**只对记录中的构建和场景成立；**假设**须通过新证据确认。1.2.38/39 的 host 测试和构建记录不证明实机互通；本文未重新运行它们。

## 2. 系统自动建接口：第一优先级

源码事实：`Native/WCLCoreRuntime.hpp:119` 的 Darwin 24/25 `APPLE80211_IOC_VIRTUAL_IF_CREATE` 分支对非空 data 返回 Unsupported，因为角色和 WCL payload 的真实布局未确认；它目前还会按传入长度至多复制 64 字节作诊断。注意属性 `MT7922AWDLCreatePayloadRead=false` 和日志 `payload-read=0` 与实际有界 memcpy 不一致，不能用该属性证明未读取数据。须先确认 bytes 是否确实是已验证的可访问长度，不能仅因调用者传入就假定内核缓冲区有效。

历史观察：`1.2.39-airdrop-slot-windows-20261031.txt` 记载 1.2.38 会话中 awdl0 不存在，airportd 报 role=awdl parent=null；手动操作 17/3/21 是实验路径。该日志没有证明同次 WCL 请求一定到达上述分支。

可行方案：
1. 静态追踪目标 Family/WCL 的 `VIRTUAL_IF_CREATE` 调用者、拷贝边界、结构版本、长度、role/输出名字/MAC 偏移，以及销毁请求。交叉对照 BCM `createProxInterface`、`enableVirtualInterface` 和当前 `buildProximity` 所需的对象/注册顺序；不得把 Darwin 22 的结构直接当 Darwin 25 ABI。
2. 在已有记录或下一次经授权的实机诊断中关联打开 Finder 前后的 `MT7922AWDLCreateRequests`、selector、bytes、result、airportd 日志；确认请求是否到达。记录可能包含设备地址，不要不加筛选地公开。
3. 完成 ABI 及 buffer 生命周期核实后，仅对确认为 AWDL 的请求分派到真实 proximity 构建，成功后返回实际注册的名字/地址；其他角色继续明确拒绝。处理重复创建、部分创建失败回滚、系统关闭、卸载、睡眠和并发请求，禁止虚报成功。
4. 检查 CoreWiFi 父接口、role 和注册属性是否与 BCM 的系统可见对象一致。若接口已创建仍报 parent=null，单独解决映射，不能再次创建同名接口。

验收：无需手动 proximity 操作，Finder 发起时系统能创建并匹配 awdl0；反复开关、Wi-Fi 切换、失败中断后没有重复接口或资源泄漏。抓取请求和注册的同一时间线，确保这是系统创建而非旧接口残留。

## 3. Family enable 与无线电状态机

源码：`Native/TemplateRuntime25.hpp:95` 获取 Family manager，捕获模板并调用 enable；`Native/EngineRuntime25.hpp` 在真实 ROC grant 中组织 PSF/MIF、AW 事件和数据发送。`Compatibility/NetworkPolicy.hpp` 中异步 scheduler 标志关闭，但另有周期引擎；不能以此推断没有周期工作。历史 b229/b231 等记录证明部分旧版本达到过特定配置/窗口步骤，不证明 1.2.39 成功。

实施方案：为目标版本逐项对照 Family `configure`、`enableAWDL`、驱动 protocol slot，把每次调用整理为：参数 ABI、返回值、只缓存/真实固件动作、成功和回滚条件。重点检查 channel sequence、sync state、election、template、AW start/end、MI、P2P coex。未实现的请求不能无条件返回成功；先识别哪些是 AirDrop 实际运行必需。

`EngineRuntime25.hpp` 中 AW start 根据 Family 表和本驱动窗口构造，需核对 AW 编号、master、起点时间单位及 grant 起止；私有对象偏移必须对应目标二进制。引擎停止须按停止新提交 → drain TX → ROC abort → 确认次级 BSS/peer 删除 → 恢复 STA 信道/RLM → 释放 lease 的实际所有权顺序检查；发生超时保留所有权、报告失败。验证系统 enable/disable、scan、STA 信道变化和设备关闭的锁顺序及竞争。不要仅打开当前未验证的 async 标志。

验收：配置调用与实际硬件动作可追踪；每次 start 有 grant、合法信道和配对的结束；重复启停、扫描与关闭后资源归零、STA 可恢复，没有死锁或错误上报 link up。

## 4. Action RX、Family peer 和服务交接

Darwin 25 的实际路径是 `Native/ModernCore.cpp:1073` 在 `awd6RxProbeActive` 时捕获最后一个外部 Action、metadata、host capture 时间和 serial；窗口结束后 `Native/EngineRuntime25.hpp:111` 的 `handoffActionFrame()` 取得 Family 管理帧 buffer，调用 `MT7922ActionFrameInput`，再检查 peer。此前对话提到的 `AWD4NativeRx*` / `bpfTapInput` 属于另一编译分支，不应作为本路径的主要计数。

源码风险：只有最后一帧的共享缓存，多个 Action 在处理前可能被后来的帧覆盖；当前代码以 serial 标记，需审计 RX 写入与窗口后读取之间是否存在并发撕裂、跳帧、生命周期和内存序问题。这个风险是代码推断，尚无丢失频率的实测。`AWD6HandoffAttempts/Serial/Calls/Written/Skipped/Skip/LastResult` 和 `AWD6PeerFound` 应与外部 Action 序列、帧 MAC/频道关联；Written 或 result 本身不能证明 peer 服务状态完成。

实施：先复核目标 Family 的 actionFrameInput 签名、0x48 metadata、RSSI、时间戳、buffer ownership 与返回值；检查 skip=manager/pool/capture/short/not-awdl/rssi/buffer/capacity/write。抓包后比较输入数据与 Family 找到的 peer、选举、模板更新、traffic registration、UMI。如果证实覆盖影响工作，设计有界队列：RX 只复制、工作线程批量消费，规定满队列策略、停止时清空、引用及顺序，避免在持有无线电锁时调用 Family。最终必须验证 peer 超时、删除和重建。

验收：外部帧 capture → handoff → Family 对应 MAC 的 peer → 选举/服务事件均可关联；无需手工伪造 peer；高频 Action 不发生不可解释的漏帧或崩溃。

## 5. 频道和共同 slot：明确的准入缺口

源码事实：`Native/DataPathRuntime25.hpp:218` 仅在 `enginePeerScheduleValid && awdlCurrentWindowChannel && fresh(nowUs)` 为真时检查共同频道和 slot 尾部 3 TU；无 schedule、过期或频道为零会绕过该检查，继续取队首发送。该事实不证明它是当前实机唯一故障。检查位于取队首之前，且当前有单个 `enginePeerSchedule`；需确认单播目标 MAC 和所选 schedule 的绑定，多 peer 情况不能假定正确。还需按抓包/规范证实 slot 起始保护、组播和发送完成裕量，不能把现有尾部 3 TU当成完整规则。

`AWDL/PeerSchedule.hpp` 接受特定 73 字节 sync、41 字节 sequence、40 字节 election 等形态，支持的频道/operating class 有限制，anchor 来自主机接收时间而非硬件 TSF，fresh 期限 2 秒。`EngineRuntime25.hpp` 的同频道长 ROC 与 PSF phase 是 1.2.39 新方案，尚无相应实机结果。严格拒绝本身是已知兼容边界，不应直接放宽到无法解释的结构。

实施：将单播发送准入改为需要目标 peer 的新鲜、可信 schedule，计算本端及对端共同频道、起止保护及预计完成时间；缺失或过期时暂存，定义超时/队列满/peer 消失的释放策略，避免八项 TX 队列永久阻塞。组播单独验证允许时隙及保护期。按 peer 管理 schedule，处理主节点变化、过期、乱序、16 位计数回绕。核实 RX 硬件时间戳可用性及 host/TSF/ROC 时钟换算；若仅有 host 捕获，测量抖动并保守纳入误差。比对 iPhone 原始 PSF 和实际空口发送，确认 57/73 字节字段和时间单位，不依据单个样本泛化。

验收：无有效 schedule 不放行单播；过期、频道不同、边界保护、peer 切换均有可解释的 defer/丢弃；空口观测证明双方同频道，长时间不因漂移错窗。单独覆盖多 peer 和组播。

## 6. 数据 BSS、WTBL、收发和网络层

`1.2.38-airdrop-datapath-20261031.txt` 记载新加 data BSS 激活、RLM、最多四个 peer 预建立并以确认结果门控 TX；新行为当时无硬件结果。`1.2.24-rx-visibility-20261031.txt` 记载 1.2.23 的 30 次 TX complete、RX routed=0，不能推广到新版本。`Native/DataPathRuntime25.hpp` 有 raw 和 translated AWDL 解码、mbuf 队列及发布路径；translated 分支的 `AWD6DataRxRouted` 在格式检查前增加，所以该值单独增长不是有效 AWDL 帧证据。

分阶段诊断与实施：
1. `AWD6DataBssConfirmed/Rlm/Failures`：若失败，审 MCU 指令编号、BSS ID、band/channel、PHY、响应序号及确认语义，不提前修改 TCP。MAC/WTBL 不得跨失效 lease 使用。
2. `AWD6DataPeersPreopened/PreopenFails`：关联 Action 来源、Family peer 和 WTBL 地址，确定对端 AWDL MAC；检查上限、去重、删除、失败恢复。group WTBL 和单播 peer 分开核验。
3. `AWD6DataTxQueued/Sent/Completed/Dropped/Pending`：区别软件入队、提交、固件回收和无线 ACK；completion 不等于对端接收。配合空口和 peer 响应。
4. `AWD6DataRxCalls/AnyData/Translated/Raw/Frames/Published/Dropped`：无任何数据时查 RX filter、频道/时隙、BSS/WTBL 和对端响应；有原始数据但不识别时对照实际封装、地址、长度、translated 判定；已解码未发布时检查 mbuf、队列和 Skywalk 所有权。AnyData 只统计 raw 数据，不代表 translated 数据缺失。
5. 数据已发布后，确认 awdl0 的 IPv6 link-local 与 ND，随后双向 mDNS 和 TCP；链路层通过不代表 sharingd 一定完成用户态发现和连接。
6. 每窗口销毁 BSS/peer 导致持续 TCP 中断只是候选假设。先把重传/丢包与 lease 删除关联；若成立，再论证保留 BSS/WTBL 跨窗口的固件契约和 STA 共存，处理过期、更新、删除确认及失败回滚，不能直接取消清理。

验收：双向 ND、mDNS、TCP 可持续穿过多个 AW；同时 RX 发布和 TX 回收没有持续增长的 drop、积压或泄漏。

## 7. 后续验证流程（计划，不是本次已执行）

先记录目标 macOS/Darwin、kext CFBundleVersion/UUID、Wi-Fi 频道、设备身份及是否是同一构建。先观察不手动操作时的系统创建路径，再在隔离的实验中按现有版本文档的 17/3/21 操作建立基线，二者分开记录；执行需未来明确授权并由操作者评估驱动加载风险。采集 `MT7922AWDLCreate*`、`AWD6Engine*`、`AWD6Handoff*`、`AWD6Peer*`、`AWD6PeerSchedule*`、`AWD6DataBss*`、`AWD6DataPeer*`、`AWD6Slot*`、`AWD6DataTx*`、`AWD6DataRx*`、airportd/Family 日志，以及可用的同频道空口包。按时间、构建、peer MAC、频道关联；避免把不同版本计数混用。隐去分享前不必要的 MAC、设备名和文件信息。

按第一处失败停止归因：无系统请求→WCL 调用/映射；请求被拒→ABI/role；接口正常但无外部 Action→扫描/频道/过滤；Action 有但 peer 无→handoff ABI；peer 有但 schedule 拒绝→帧形态/时钟；schedule 有但 BSS/peer 不就绪→固件配置；TX 有而 RX 无→空口/窗口/过滤/WTBL；RX 已发布而 ND/mDNS 失败→IPv6/组播；服务成功但 TCP 中断→窗口生命周期和丢包；TCP 成功后再查 sharingd。每一步保留相反方向的证据，不以单一计数证明整条链路。

最终验收：Mac→iPhone 和 iPhone→Mac 均发现、弹出接收提示、文件内容完整；分别覆盖有/无基础设施连接、反复启停、睡眠唤醒、不同文件大小和较长传输，并确认失败后设备可恢复。性能优化如 A-MSDU/Block Ack 应在基础互通之后单独评估。
