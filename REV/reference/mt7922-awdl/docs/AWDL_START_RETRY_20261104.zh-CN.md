# 1.2.102 AWDL 临时启动失败恢复

## 实机证据与边界

用户201157按独立AWDL流程测试，en4 inactive、ConnectionState=0。EngineStartCount/Fails从上一轮3增加到4，但本轮12个样本始终4，EngineStarted=No，没有EngineStartResult/PreGrantSetupMs/周期窗口计数。同步状态 mode1 请求有到达，模板110字节已准备；没有文件接收证据。

窗口函数在优先期、控制锁、硬件状态、资源占用和扫描/关联/DHCP保护等前置检查处直接返回false；调用方将零grant一概标记window-grant。因此当前日志不能证明固件拒绝授权，也无法唯一确定哪一个前置检查导致本次失败。上一轮指示断开AP，已由本轮inactive状态验证；继续要求相同重测不能补足缺失分支证据。

源码有两个确定缺陷：1.2.101每次真实扫描请求都发布5秒禁入，扫描已完成后仍可能拦住AWDL；同步启动若碰上临时阻塞即让configure失败，之后没有驱动恢复路径。这是本轮针对修复范围，仍需实机确认是否覆盖本次具体阻断。

## 修改

- 普通扫描的优先期从5秒改为100ms；扫描仍在进行时，原scanInProgress/scanCancelPending检查继续阻止AWDL；持有controlLock的扫描命令依旧串行。其他事务5秒保护、关联成功后的DHCP保护不变。缓存扫描不触发优先期的1.2.101修复保留。
- 每次初始窗口前置拒绝记录AWD6WindowBlocked及AWD6WindowBlockTransient，区分infra-priority、control-lock-busy、scan-busy、join-cleanup-busy、dhcp-bootstrap与不可重试的硬件/资源问题。EngineRefused保留具体原因；实际已尝试窗口后的失败才使用window-grant/tx/stop等标签。
- 仅对系统请求的启动在上述临时前置检查处失败登记恢复请求。控制器tick在radio/扫描/关联/DHCP/优先期都空闲时，重新进入现有nativeAwdlSet(true)路径，不直接伪造SYNC成功、不主动disable Apple peer manager。
- 最多3次恢复，最晚首次登记后30秒；仍忙时不发MCU，不1ms重复启动。真实固件失败、资源持有、关闭电源、系统SYNC关闭和停止不会强制重启；硬失败清除旧的待恢复状态。
- nativeAwdlSet的启用路径只有真实EngineStarted及运行标志成立才arm和开启数据队列，避免“请求过启用”被当作启用成功。
- 保留先前mbuf链检查、GET POWER与hardwareReady查询、后台非阻塞gate、退出路径、普通TX与关联流程。未改蓝牙、未安装驱动/修改EFI。
- 采集器区分engine-deferred/engine-started/engine-refused，并保存恢复次数和具体阻断原因。

## 验证

实际生产恢复函数与临时失败登记代码提取到ASan/UBSan宿主用例，覆盖等待扫描/关联/DHCP/优先期、3次上限、30秒到期、成功才标记运行、关电取消、硬失败停止、持有资源不启动。该测试的Apple enable函数为桩，不能证明私有Family FSM会接受重新启用。

实际工作队列try-gate及hardwareReady并发用例、普通事务优先有界等待、扫描100ms与关联5000ms区别、GET POWER、生产AWDL队列及mbuf防线、关联MCU顺序检查通过。Release x86_64构建及包内二进制版本/UUID/hash/ZIP检查另存。

未实机验证。若Family拒绝重新configure，或WindowBlocked显示其他前置条件，仍需按新分支证据处理，不宣称已打通AirDrop或并行联网。

## 测试

替换1.2.102并重启。首轮保持Mac Wi-Fi开关开启但不连接AP；可用USB上网。目标iPhone解锁，个人热点关闭、隔空投送所有人10分钟。运行AWDL-capture/start.command，15秒基线后打开Finder隔空投送。engine-deferred时保留等待，engine-started后发送一次send-test.txt；实际收到并打开才记received。未启动则保留60秒目录，无需反复发文件。

重点核对AWD6WindowBlocked、StartRetryPending/Attempts/State、EngineStartResult、PreGrantSetupMs、窗口及MI/data的新增长。成功后再另测连接AP共存，不能把独立AWDL成功当作并行联网成功。源码前备份、patch、检查日志和dSYM保存在本审计目录。
