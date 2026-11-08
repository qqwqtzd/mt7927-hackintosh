# 1.2.94：AWDL 发送、授权时间校验与数据队列

## 这轮记录证明了什么

来源：`/Users/angus/Desktop/MT7922-AWDL-20261104-153729`，实际驱动1.2.93。15:37:33开始，15:39:07标记seen，15:39:20标记send，15:39:35自动结束；无received标记。

| 环节 | 本轮证据 | 判断 |
|---|---|---|
| 接口注册 | AutoCreateResult=0、Stage=registered；58/58次awdl0存在且有fe80地址 | 1.2.93的接口缺失断点已越过 |
| 外部Action | Serial 366→1046；handoff 49→137；peer确认48→136 | 有外部AWDL接收及系统交接，不是88台设备，也未确认这些帧全部来自目标手机 |
| 周期窗口 | SameChannelWindows 70→170；DataBssReady 51→129 | 周期窗口实际在执行。EngineWindows=1、EngineTxTotal=6是旧代码仅更新首次启动造成的陈旧统计 |
| 服务MI | 长度一直767，Submitted=0；Rejected 28→73；Dropped 17→39；最后Reason=build/EINVAL | 明确存在驱动构帧失败 |
| 同信道发现发送 | WindowTx在58组快照中都为0 | 源码及离线复现确认1秒授权被64毫秒校验拒绝；不是仅凭统计猜测 |
| 数据面 | 无DataPathEnabled属性，无DataTxSent，RxPublished=0 | 没有证据证明数据链路已通；源码缺少现代系统路径的队列推进 |
| 发送会话 | sharingd 15:39:18 ASK START；15:39:22 Ask failed；15:39:34 DNS Refused/resolve-failure | 卡在本机建连/解析阶段；没有证据证明手机已收到请求或拒绝接收 |

目标会话名称是iPhone16pro，基础网络名称是iPhone12热点。系统选择的路径出现en4、expensive/constrained/proxy，而会话要求use awdl、prohibit expensive/constrained及prefer no proxy。这可能影响建连，不能从日志直接判定代理是唯一根因。本次没有改代理、VPN、网络成本标记或系统设置。

## 三个可复现的源码缺口

### 1. MI复用了640字节探测帧上限

IO80211Family的MI缓冲按现有反编译契约允许0x4b0（1200）字节，但buildMasterIndication另行要求40字节头加payload不超过DiscoveryCapacity=640；两个调用点也只分配640字节输出。

本轮TLV长度摘要为24:40,12:47,16:40,21:2,6:13,2:58,2:46,2:89,2:101,2:155,2:143。加上每项3字节TLV头正好767字节；封装后807字节，必然被拒。不能删掉或截断服务TLV来凑640。

修复：为MI定义独立的1200字节payload上限和1240字节帧容量，修改即时及暂存MI两个输出缓冲。继续校验TLV边界、调用方输出容量、真实lease和发送截止时间。PSF的DiscoveryCapacity仍为640，基础TX描述符和DMA路径未改。

参考本项目AWDL/1.2.1-rx-handoff-mi-tx-20261030.txt记录的Family MI缓冲/请求ABI，并核对FrameCore解析上限和submitNetworkFrame实际长度准入。此资料是参考证据，没有作为额外操作指令执行。

### 2. 1秒物理授权被当作64毫秒探测窗口

nativeAwdlRunWindow同信道申请1000ms，却把完整grantStart/grantEnd直接传入DiscoveryTiming。discoveryTimingStatus一开始要求end-origin≤64000us，因此还没有进入slotClock分支就拒绝，后续PSF构帧失败。

修复：新增discoveryTimingInGrant。仅对长授权建立不超过64ms、不会越过真实授权终点的局部校验区间，同时保留原始grantStart为epoch，继续按原相位计算线上down-counter。TxContext截止时间仍是原始真实grantEnd；不延长物理授权、不强制跳频道、不修改短探测窗口校验、不缩短/重置线上时钟。

### 3. Darwin25没有推进AWDL队列，也没有补齐引擎数据面生命周期

现有proximityQuery/Tx、RX解码、completeTx和nativeAwdlDataPath均已实现，但tick只在Darwin22分支调用serviceProximityPackets。现代分支仅调用serviceAsyncRadio（默认关闭的另一实验入口），常规运行缺少发送重唤醒、完成回收和接收上交。现有引擎起停也没有调用nativeAwdlDataPath；本轮系统没有调用enableDatapath，因而数据面一直未启用。

修复：在原控制器tick、基础数据处理之后，增加现代系统的数据面服务。仅在系统已成功启动引擎的运行状态上升沿启用队列，停止/关电时停用；同一运行期内不反复覆盖系统显式disableDatapath。随后推进现有AWDL队列。不会因自动注册就强开引擎，也不在持有radio controlLock的线程里调用Apple队列完成。

为避免这次启用暴露退出死锁，停止时等引擎线程退出，才把软件队列中未发出的包转入完成队列；DMA中的包保留原有完成责任。完成失败保留重试指针，drain同时检查它。无线线程的三处丢包路径改为排队回收，Apple completeTx统一留在控制器工作循环执行。不会提前完成仍可能提交的包。

同时修正EngineWindows/EngineTxTotal周期累计，新增每窗口构帧尝试、构帧拒绝、提交拒绝和时间状态，RX上交计数按变化发布。这些统计仅证明提交/处理，不证明无线ACK或文件送达。注册时的BSDInterfacePresent仍是瞬时快照，以实际ifconfig为准。

## 验证

- 修改前源码复现：767字节MI与1秒grant PSF均拒绝（测试预期退出7）。
- 修改后ASan/UBSan（按日志重建TLV长度布局，值为合成数据，不是原始MI重放）：767→807字节完整MI、1200→1240上限、容量差1字节、越界TLV及到期拒绝；1秒授权内逐137us扫时刻，检查相位/边界与短64ms兼容。
- 执行实际生产数据面服务及队列函数的宿主测试：一次启用、尊重显式停用、线程未退出不回收软件队列、退出后回收、保留DMA责任、完成失败重试、RX上交、关电。
- 既有master-indication、psf-layout、airdrop-datapath、template-sim、election-sim、chain-sim通过；Resources资源回收527项检查通过。
- 1.2.93注册准入、关联MCU顺序与1.2.92联网保护回归通过；采集器分类自测和启动脚本语法检查通过。
- Release x86_64编译通过。上述测试不能验证Apple私有ABI的实际回调、无线送达、目标身份及最终文件接收；尚未实机加载1.2.94。

本次没有改基础网络握手/带宽/TX/RX、5秒DHCP保护、射频调度政策或蓝牙。修复前相关文件保存于Build/awdl-tx-repair-20261104/source-before；旧测试包保留。

## 下一轮怎么判断

1. 安装1.2.94并重启，先确认普通Wi-Fi联网。为隔离系统路径限制，使用普通路由器，测试期间暂时退出代理/VPN，目标手机关闭个人热点。等待10秒。
2. 运行随包AWDL-capture/start.command。15秒基线后打开Finder发送send-test.txt，记录seen/send；点击发送后保留至少45秒。采集上限延至180秒，新增awdl-packets.log被动主机包头记录。
3. 先检查MI Submitted增长、每窗口TX及BuildRejected，再检查DataPathEnabled、Queued/Sent/Completed和RxPublished；随后对照ND/服务解析包头、sharingd解析/建连及手机实际打开文件。
4. 若MI和数据已发送但解析仍失败，继续定位AWDL服务响应/邻居发现及系统路径限制；若数据从未入队，检查原生队列ready和系统数据面回调。不能把peer存在或主机抓到TX当作端到端成功。
5. 测试结束检查普通网页、关闭Wi-Fi、切换网络和重连。停止回收的实机表现也属于本轮验收。
