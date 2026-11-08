# 1.2.96：修正AWDL注册对基础连接的依赖

## 本轮证据

读取用户的/Users/angus/Desktop/MT7922-AWDL-20261104-162205，实际加载1.2.95。16:22:11开始，16:22:20 seen，16:22:26 send，16:23:39结束，共42组快照。

- en4全程status inactive，CoreConnectionState=0，但MT7922AdmissionPowerOn=Yes、MT7922AwdlAutoCreate=Yes。
- awdl0全程不存在；AWD1NativeProximity=No；AutoCreateAttempted、Result、Stage均未出现。没有AWDL窗口、MI或数据面计数，因而不能评价1.2.95的跟随编码效果。
- sharingd目标仍为iPhone16pro，会话选择en3并进入proxy路径，报告Expensive path prohibited。日志中的en3选择与受限路径是真实证据，但不能只凭接口编号认定其硬件类型，也不能证明目标手机收到请求。
- 扫描仍有完成记录，WCL扫描代次5→11；不能把没有AWDL接口解释为整个驱动没有加载或扫描锁死。

## 原因与修改

1.2.93为保护基础握手和DHCP，在serviceAwdlAutoCreate中把networkVisibleLink设为必要条件。这个条件过严：Wi-Fi开关开启但没有加入AP时，自动注册永远不会尝试。使用其他接口上网也不能满足这个条件。该限制属于此前实现的问题，本轮需要修正注册前提，不能要求AirDrop长期依赖普通路由器连接。

1.2.96取消“必须已有基础连接”，改为：启动完成、Wi-Fi已通电、基础BSD接口存在、选项开启，等待5秒启动稳定期；避开扫描/取消、关联和连接清理。若已经连接Wi-Fi，继续遵守networkAwdlHoldUntilMs的5秒DHCP保护；未连接时不受旧连接遗留的hold时间限制。调用工厂前检查现有hardwareReady，暂时未就绪不消耗一次创建机会。关电会清除尚未完成的稳定期，重新开电重新等待。

每个Core仍最多自动调用工厂一次，系统已创建的接口不会重复创建；实际工厂失败保留原有失败记录与清理。dataPathLock只读取条件，调用硬件检查和工厂时已释放。没有新增等待硬件的阻塞循环，没有修改射频调度、基础握手或蓝牙。

新增可观察等待阶段：waiting-power、disabled、waiting-bsd、waiting-scan、waiting-join、waiting-cleanup、waiting-infra-bootstrap、waiting-startup-settle、waiting-hardware。只在阶段变化时发布属性；工厂调用及返回仍显示building/registered/failed。采集15秒后会显示接口/等待状态，避免只凭Finder看见手机就继续误判。

## 验证

执行真实生产注册函数的ASan/UBSan宿主测试：复现旧代码在未连接AP时不尝试注册；新代码覆盖无AP/其他接口上网场景、稳定期边界、未就绪硬件、扫描/取消、关联/清理、连接后DHCP保护、关电重置稳定期、已有接口、一次创建、工厂失败及调用工厂时未持有dataPathLock。

上一轮MI容量/授权时间、生产数据队列、关联MCU顺序以及外部PSF跟随编码检查通过；采集器自测和启动脚本语法通过。Release x86_64编译通过。没有实机加载1.2.96；这些检查不证明独立模式频道同步、固件收发或文件送达。

## 实机复测与边界

替换1.2.96 kext并重启，保持Wi-Fi开关开启。可不加入AP、保留USB等其他网络。目标手机解锁、关闭个人热点，AirDrop设为所有人10分钟。运行随包180秒脚本，等接口出现后发送一次，发送后留45秒；没有接口也保留完整目录，先查waiting阶段。

本次解决的是接口注册入口。独立模式仍沿用既有ROC调度；1.2.95跟随编码仍受原有同信道准入条件限制，不能宣称本次已验证所有独立模式的无线时间安排。下一步依次检查实际awdl0/IPv6、引擎启动/拒绝、FollowerWindowBuilt及双向包，再判断系统解析和接收。en3代理/受限路径可能独立影响发送，本次未改系统设置、默认路由或代理。

复测结束检查普通联网、关闭Wi-Fi及重连。源码前备份位于Build/awdl-standalone-20261104/source-before，1.2.95测试包保留。
