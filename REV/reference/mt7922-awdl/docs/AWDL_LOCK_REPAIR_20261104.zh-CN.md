# 1.2.101：AWDL 与普通连接的锁竞争、睡眠退出修复

## 结论及证据范围

本轮修改源码、生成测试驱动；尚未加载到用户机器，不能宣称已经恢复联网或消除所有睡眠 panic。保留 AWDL 自动注册、20MHz 认证选项、现有网络功能和 mbuf 分段防线，不整体回档，不修改蓝牙。

用户提供的 174922 采集明确加载 1.2.100。10 次 GET POWER 均 err=0，AdmissionPowerOn=Yes，接口 UP/RUNNING 但 inactive，无 IP；21 份样本 ConnectionState=0，无新的 WCLJoin 准入属性，不能据此认定已经进入 Auth、EAPOL 或 DHCP。AWDL 在采集前已有窗口70、TX420、data TX22/RX27，采集期没有增加，EngineStarted=No、StopSource=close、ResourcesHeld=No。旧计数不能当作此次进展。

额外只读保存的 airportd 日志：17:48:56.896 请求关联 iPhone12；17:48:56.907 系统报告 DISASSOC succeeded；17:50:56.949 ASSOC 在120.053秒后 err=60。因此旧连接退出的 airportd 请求已返回，不能把整个120秒都归为 userDisconnect 正在执行。驱动关联提交属性未出现，故下一次重点是进入驱动前的 ready/MAC/队列准入，不能直接修改 EAPOL 协议或 DHCP。

## panic 精确匹配

panic1104.rtf 是 Sleep transition timed out after 180 seconds，不是先前 mbuf 越界。

- 驱动版本1.2.100，UUID 1F147DC9-EDEB-3078-AD26-21FF659103FD，与保存的 dSYM 完全一致。
- 驱动基址0xffffff801dd17000，返回地址0xffffff801dd83db8，偏移0x6cdb8；解析为 MT7922AWDLInterface::setLinkState，ModernCore.cpp:1600，正在调用 Apple 基类。
- IO80211Family 和 IOSkywalkFamily 参考 macho 的 UUID 也与 panic 相符，不是拿其他系统版本猜符号。
- 调用链：Skywalk BSD client powerOffInterface → network interface disable → IO80211VirtualInterface::setInterfaceEnable → 驱动 setLinkState → Apple setLinkState → peerManager disable/disableGated → AWDL disableAWDL → removeAllPeers → IORecursiveLock → workqueue commandSleep。

这直接证明睡眠回调卡在 Apple AWDL peer 数据路径锁上。单份指定线程栈没有持锁者栈，不能声称已证明哪一个后台线程是持锁者；但源码确实存在后台 reporter 直接 reportDataPathEvents、radio worker 直接 actionFrameInput/findPeer、radio worker 同步 runAction 等交叉调用，需要修复。

## 修改一：控制器队列统一承载后台 Apple 调用

ModernCore.cpp 中的原生工作队列新增非虚拟 tryDriverAction：tryCloseGate 失败立即返回 Busy，不排队、不等待；成功在 gate 内执行 action，所有正常返回均 openGate。使用现有 SDK IOWorkLoop 门锁，不添加 Apple 私有虚表槽。

EngineRuntime25.hpp 的后台 template refresh、RX action handoff 和 AW 报告均走该入口。handoff 与报告消费函数额外检查 inGate；报告线程在 gate 忙时不消费 pending 位，下一次再试。radio worker 不再同步等待 nativeWork->runAction，因此 gated close/stop 等待 radio thread 退出时不会形成“退出线程等队列、队列等退出线程”的循环。无线电线程仍只负责窗口/ROC/固件清理，不在 controlLock 内调用上述 Apple 回调。

关闭时先取消运行标记；已进入的回调与 gate 上的关闭操作串行，未进入的回调延后或因关闭而拒绝。保留原来的 ROC token/截止时间、peer 删除、filter 恢复和资源未释放拒绝逻辑，不强制假报清理成功。

## 修改二：就绪查询与 MCU 锁占用分离

hardwareReady 原来抢 controlLock，AWDL 正常窗口持锁也会导致 isChipInterfaceReady、enableDatapath 等查询返回 false。现在直接 acquire 读取 powerBlocked、firmwareReady、firmwareRunning、wfdmaReady、stopping。MT7922.cpp 与 PowerRuntime.hpp 对三个 firmware/DMA 就绪标志的全部常量赋值改为 release 发布；保留原有生命周期顺序。固件未运行、DMA停止、睡眠阻断、teardown 仍返回未就绪，未把检测改成无条件成功。GET POWER 的1.2.100提交快照修复保留。

## 修改三：普通无线电事务优先

真实扫描、WCL关联、设置基础设施 MAC、WCL离开网络和开启电源在抢 MCU 锁前发布5秒优先期。AWDL窗口在拿锁前后检查，运行中的窗口在轮询处发现优先请求后走正常清理，不能继续发新的窗口。准入等待最多300次×5ms，超时真实返回Busy，记录 AWD6InfraPriorityTimeouts，不无限等待或强拆仍有所有权的会话。

既有 scan/join/cleanup 状态与关联成功后5秒 DHCP 保护继续有效。连接中的缓存扫描先完成缓存分支，既不抢 MCU 锁也不发布5秒保护，避免周期性缓存请求反复打断 AirDrop。这里没有实现全局 MCU 事务调度器，不能宣称所有锁竞争被消除。

诊断新增 AWD6InfraPrioritySource/WaitMs/Timeouts；WCLLeaveStage 区分 radio-priority/radio-busy/hardware-cleanup/hardware-returned。数据不含口令或密钥。

## 验证

- 提取实际 tryDriverAction，在另一个真实线程持有工作队列门锁时连续10000次不进入回调；释放后执行成功，回调返回后门锁确实释放。
- 提取实际 hardwareReady，MCU mutex 被另一线程持有时10000次仍就绪；固件/DMA/阻断/停止等真实未就绪均拒绝。
- 提取实际普通事务优先函数：真实锁持续占用时在模拟累计1500ms后拒绝且优先期已发布；释放后成功。该用例不能证明 MCU 固件命令延迟上界。
- 检查生产后台调用点无同步 runAction/直接 handoff；真实窗口在入口和循环检查优先期；缓存扫描在优先分支之前返回。
- 原有GET POWER、自动注册、生产AWDL队列及mbuf复制检查、关联BSS/RLM/STA/filter顺序检查通过。
- Release x86_64构建和包内版本/二进制/ZIP完整性检查另见checks与manifest。编译不等于实机验证。

## 测试流程

替换包内kext后重启。先不开Finder隔空投送，运行 WiFi-check/start.command：保持15秒，连接一次普通网络，等45秒。正常后检查取得IPv4且网页可用，再做一次关闭/开启Wi-Fi。仍失败就结束采集并给出桌面新目录，脚本现在同时保存主接口属性和定期内核快照；不会再用AirDrop提示引导本轮测试。

普通网络正常后再独立验证睡眠/唤醒、重新连接，最后恢复Mac→iPhone发送测试。脚本每约10秒保存内核快照并fsync，便于panic后读到已落盘记录；不能保证任何panic下最后瞬间日志都保存。

采集脚本只读取状态，未安装驱动、改变网络/蓝牙/代理/路由或自动发送文件。测试ZIP不包含本机原始日志与panic；原始证据、源码前备份、patch、符号文件保存在Build/awdl-lock-repair-20261104。
