# 1.2.93：Mac → iPhone 首个接入断点

## 采集证据

读取用户提供的 `/Users/angus/Desktop/MT7922-AWDL-20261104-152106`，实际加载1.2.92。采集15:21:11开始，15:21:38用户标记seen，15:21:48标记send；没有received标记。

- 32/32组ifconfig快照：`interface awdl0 does not exist`。
- 32/32组Core快照：`AWD1NativeProximity=No`；AWDL接口类快照为空。
- airportd出现34次 `Failed to match cached interface (role=awdl parent=(null))`，包括点击发送前后。
- AWDL窗口、peer、专用Action handoff及数据面计数未被观测，IPv6链路本地地址未出现。
- 通用Action/基础数据接收计数增长不能证明来自目标iPhone或AWDL数据链路。Finder显示手机也不能证明文件请求送到了手机。
- 旧bonjour.log为空，但旧采集器把dns-sd输出重定向到文件，可能有进程缓冲；不能据此判断没有Bonjour服务。

当前能确认的最早断点是本机AWDL接口没有注册出来。不能从这份记录推断手机收到后拒绝，也不能证明接口注册后后续同步、IPv6和传输一定正常。

## 源码路径与修复

现有ModernCore已实现buildProximity：role 6工厂、资源池、虚拟接口start、资源注册、registerService、控制器AWDL接口登记和失败清理。原来只有系统创建请求或诊断操作会进入；这一轮没有看到该路径完成。控制器常规启动和tick没有补充注册入口，接口缺失可以一直持续。

1.2.93新增serviceAwdlAutoCreate，由现有控制器tick调用，并沿用同一nativeWork/threadWork门内的buildProximity。基础包处理先执行。没有新增另一套接口工厂，没有修改蓝牙能力或发送文件协议。

自动入口需要显式Info.plist开关MT7922AwdlAutoCreate=true，并同时满足：启动完成、未停止、Wi-Fi通电、基础BSD接口存在、基础连接可见、无关联/扫描/取消扫描/连接清理、联网后的5秒保护期结束。dataPathLock只用于读取条件，调用工厂前释放。原有builder继续检查硬件状态和生命周期、处理失败清理。

每个Core生命周期至多自动调用工厂一次。系统已建好接口时不重复分配；自动失败时记录原始返回值，不在1ms tick里反复创建。AWD6AutoCreateAttempted/Stage在工厂调用前发布，Result/Ready在返回后发布，以便区分未满足条件、卡在工厂和明确失败。Ready表示资源注册完成，不表示BSD客户端已附着或无线链路可用。后续系统role 6创建请求仍返回真实已注册接口的地址和名称。

保留1.2.92联网修复、握手/带宽、TX/RX、DHCP保护和所有既有功能；不主动启动AWDL无线窗口，不绕过setAWDL_SYNC_ENABLED，不伪造peer/IPv6/文件成功。新选项false只关闭这次新增的自动入口。

采集器同时补齐AWD2/3/5属性、接口存在标记和创建/引擎状态。Bonjour改用PTY实时读取；子进程独立进程组，结束时回收。原有seen/send/received人工标记保留。

## 验证及边界

Release x86_64编译通过。执行真实生产serviceAwdlAutoCreate方法的宿主测试（ASan/UBSan）覆盖：缺失依赖、关闭/停止/启动未完成、开关关闭、缺失BSD接口、未联网、关联、扫描/取消、连接清理、保护期边界、正常一次创建、已有接口、工厂失败以及调用工厂时没有持有dataPathLock。静态集成检查确认先处理基础数据、共用控制器门。既有AWDL控制所有权/回收测试与关联MCU顺序检查通过。采集器证据分类自测及shell语法检查通过。

采集器10秒运行检查保存了dns-sd即时输出及当前执行环境的`Service Not Running`错误，验证错误不会被输出缓冲吞掉；这不是AirDrop硬件验证。未在用户机器上加载新驱动，未验证Apple私有ABI的实际注册、频道同步或端到端文件传输。

## 实机复测

安装本包1.2.93替换现有测试kext并重启；先连接普通Wi-Fi，确认网页正常，等待10秒。避免让目标iPhone同时提供个人热点。iPhone解锁、Wi-Fi/现有蓝牙开启，隔空投送设为所有人10分钟。运行随包AWDL-capture/start.command，保持15秒基线后在Finder向iPhone发送send-test.txt，按seen/send/received标记，卡住也保留整个目录。

验收顺序：注册返回值及阶段 → 实际awdl0 → IPv6及系统使能/引擎 → 同步窗口/peer → 双向数据 → sharingd会话 → iPhone实际打开文件。若awdl0已出现但仍等待接收，用新记录定位下一个断点，不把注册成功当作AirDrop完成。采集后确认网页、关闭Wi-Fi和重连仍正常。

源码前备份位于Build/awdl-autocreate-20261104/source-before；1.2.92联网测试包保留。
