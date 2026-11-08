# 1.2.100：AWDL控制锁干扰电源状态查询

## 实机证据

用户报告1.2.99开启Wi-Fi后界面马上关闭。本次直接读取实时IORegistry及近8分钟系统日志，未改任何系统设置或加载驱动。

快照实际版本1.2.99；AdmissionPowerOn=Yes、LastPowerResult=0、PowerTransitionStage=committed，AWDL EngineStarted=Yes、周期窗口135、发现帧提交810。说明快照时控制器仍通电并运行，不能把界面显示关闭直接当成硬件已完成关电。

同一日志段捕获40次GET POWER返回-536870187/0xe00002d5。Kernel SDK IOReturn.h确认该值为kIOReturnBusy。17:36:07.761、17:36:11.293开启命令成功，随后17:36:11.529、17:36:13.118/13.120、17:36:14.411等状态查询返回Busy。期间也有正常返回Result=1的查询，符合控制锁竞争产生的间歇错误。本记录没有完整证明所有界面关闭行为的系统内部决策，但已直接定位一个与症状相符的驱动错误。

## 原因与修改

Requests.cpp的admissionRequest(GET POWER)原来使用Mt7922zOWControlGuard抢bus->controlLock，失败直接EBUSY。AWDL引擎窗口及同步辅助BSS/peer配置持有同一把锁；读电源状态不需要发MCU命令，却因此被拒，返回null reply给WiFiAgent/CoreWiFi。1.2.98关闭AWDL自动入口时较少触发；仅修mbuf越界并不能修复这个查询问题。

新增独立原子powerStateSnapshot，由现有setAdmissionPowerGated的提交分支更新：成功开启发布1，关闭发布0；失败的开启保留上次提交值。teardown清零。GET POWER不再获取控制锁或读取过程中变化的队列标记，而是读取已提交状态，并让真实powerBlocked/stopping覆盖为0；没有bus仍返回ENXIO。

它不是固定返回“开启”：未开启、成功关闭、硬件阻断和停止都返回关闭；失败的开启不会虚报成功。电源SET仍使用原来的串行化、硬件校验及回滚，未改成无条件成功。原有hardwareReady和其他事务准入未放宽；本轮只修已证实的GET POWER竞争。

保留AWDL自动注册开启、授权前准备、mbuf分段修复和现有功能。没有靠再次关闭AWDL来回避竞争，也未修改蓝牙、代理、路由或系统电源偏好。

## 验证

使用实际旧/新GET POWER分支与真实另一个线程持有的mutex：旧代码稳定返回EBUSY；新代码连续10000次查询成功，返回已提交开启状态。测试真实阻断、停止、关闭返回0，缺失bus返回ENXIO。

执行实际提交代码块，验证失败开启保留旧状态、成功开启发布1、关闭发布0。独立注册、生产AWDL队列与mbuf复制防线、关联MCU顺序回归通过。Release x86_64编译通过；未在用户机器上加载1.2.100，仍需验证界面开关、关联及并行AWDL运行下的行为。

## 本轮测试

替换1.2.100并重启，先保持Wi-Fi开启30秒，再关联普通网络并验证网页，关闭后重新开启一次。此版本保留AWDL自动运行，因此可验证两者共存；先不发送AirDrop文件。若仍异常，运行包内WiFi-check/start.command，只操作Wi-Fi开关并保留桌面采集目录，不必输入seen/send。

下一轮日志重点看GET POWER是否仍出现0xe00002d5，以及界面与Core AdmissionPowerOn是否一致；不能仅凭编译或宿主测试断言实机已修复。新的panic需独立分析，不与本次状态查询错误混为一谈。

源码前备份和本次原始证据在Build/power-query-20261104。日志可能含网络标识，只在本地保存；测试ZIP不包含整份系统日志。
