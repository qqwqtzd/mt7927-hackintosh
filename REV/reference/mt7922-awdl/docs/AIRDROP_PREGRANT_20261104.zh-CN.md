# 1.2.97：首次AWDL授权被同步配置消耗

## 163618采集结果

实际加载1.2.96。16:36:21开始，16:36:37 seen，16:36:44 send，16:38:57结束，共74组快照。用户确认手机未收到请求。

自动注册返回0，awdl0与IPv6已出现，证明独立注册入口修复生效。但EngineStartCount=1、EngineStarted=No、EngineRefused=window-tx；整个采集无周期窗口/MI/数据发送计数。唯一一次窗口收到并接受1个授权，BSS数据激活及RLM成功，构帧尝试1次、拒绝1次、TX=0、TimingStatus=1（Invalid）。窗口整体耗时400ms，停止与清理成功，未持有资源。主机awdl0抓包0包、内核丢包0。

sharingd随后仍有Ask失败、DNS Refused及en3的受限/代理路径。本轮驱动更早就没有启动无线引擎，不能归因于手机拒绝、跟随编码失效或RX解码；也不能把接口出现当作无线链路可用。

## 代码原因

独立模式申请不超过64ms的ROC。旧顺序是：DEV/BSS/group创建 → 申请ROC → 接受grant → 同步BSS数据激活（stage115）→ 同步RLM（116）→ 已知peer准备（118）→ 尝试发送PSF。

后三步使用同步MCU确认，会消耗已开始的授权。首帧尝试时仍使用原始receivedAt+duration作为截止时间，正确的到期校验将其拒绝。TimingStatus=Invalid与这条过期路径吻合。本轮未记录逐命令耗时，400ms包含配置和清理，不能把全部400ms当成纯配置时间或空中停留时间。

首次window-tx失败后，nativeAwdlStartEngine不创建周期线程，所以后面一直没有MI、Action handoff或数据流量。此时继续点击发送不会修复驱动启动状态。

## 修复

RadioSession::start增加可选的授权前准备回调。顺序改为：创建辅助资源 → 选择频道 → 同步辅助BSS/RLM及已知peer准备 → 重新读取单调时钟 → 申请ROC → 接受grant → 构帧与发送。准备失败不发ROC、不消耗token，保留资源责任供stop确认清理；带准备回调必须提供实际时钟，不能用配置前的旧now开始授权等待。

引擎使用该回调移动原有配置命令，命令载荷不变。准备完成后再检查停止、关电、扫描/取消、关联/连接清理，遇到新任务不发ROC。窗口循环预算也从准备完成后计时，避免长配置让接收grant的循环直接超时。

原64ms/1000ms申请政策不变，真实授权截止、token匹配、到期拒绝、描述符提交准入及确认清理不放宽。已知peer提前准备；新目的peer仍可能通过既有动态路径尝试配置，并受真实deadline限制，本次没有宣称所有MCU活动都已消除。没有修改基础网络TX/RX、握手、蓝牙、代理或默认路由。

新增AWD6PreGrantSetupMs（成功准备耗时）、AWD6GrantDurationMs（实际授权时长）、AWD6FirstTxAttemptDelayUs（授权捕获到首次构帧尝试）。采集器在接口存在但引擎拒绝时提示engine-refused，不再仅提示接口已出现。

## 验证与限制

ASan/UBSan测试真实RadioSession和构帧器：准备耗时1/63/64/300/1000/4000ms后才发ROC，首帧在新grant内可构建，真实到期后仍拒绝；准备失败不发ROC、不消耗token；缺少时钟在资源创建前拒绝；清理失败保留所有权，成功重试释放。旧顺序模拟300ms授权后配置，稳定复现Invalid。生产源码顺序检查确认BSS/RLM/已知peer准备位于回调，原grant循环中不再执行这三个准备块。

既有radio-session测试（含token耗尽、拒绝旧grant、parked上下文、清理失败保留等）通过；独立注册、MI容量、授权编码、数据队列、跟随帧和关联MCU顺序回归通过；采集器自测及shell语法通过。Release x86_64编译通过。宿主测试没有真实固件；尚未实机确认1.2.97引擎启动或文件接收。

## 复测顺序

替换1.2.97并重启，Wi-Fi开启，可保持USB上网且不加入AP。运行随包180秒采集，目标手机解锁、热点关闭、所有人10分钟，发送一次并保留45秒。

先看EngineStarted是否为Yes、EngineWindows/Tx是否增长；若仍拒绝，看GrantDurationMs与FirstTxAttemptDelayUs、TimingStatus及准备阶段。引擎运行后再查Action/MI、双向数据、服务解析及手机实际收到文件。系统en3代理/成本政策仍可能单独阻断会话，不能用本次修复声称已解决全部AirDrop问题。结束后检查普通联网、关闭Wi-Fi和重连。

相关文件修改前保存在Build/awdl-pregrant-20261104/source-before；旧测试包保留。
