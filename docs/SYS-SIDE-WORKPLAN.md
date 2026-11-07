# 系统侧全服务逆向工作簿（SYS-SIDE WORKPLAN）

目标：系统安装包体系内全部 Wi-Fi/蓝牙/AWDL 相关资源 → 逆向各服务工作逻辑 →
映射"系统能力 → 消费的驱动接口" → 输出可修项清单（实证驱动）。

## 0. 资源地图（已收割，2026-11-07 夜）

### 内核层（活体 25G332 KernelCollections 三件套，kc-slice.py 切片 → entities-25G332/kernel/）
| 资源 | 来源段 | 大小 | 关注面 |
|---|---|---|---|
| IO80211Family | boot/REGION347 | 2.9MB | scan 记录构造/BSS/roam/PHY/channel/awdl 请求 |
| IOBluetoothFamily | boot | 218KB | BT 内核面/共存 |
| IOSkywalkFamily | boot | 228KB | 接口抽象/awdl vif |
| AppleSkywalkAVB | boot | 4KB | 音视频桥 |
| AppleBluetooth{Debug,DebugService,Module,Multitouch} | boot | ~96KB | 调试/多点触控面 |
| AppleBluetoothHID{Keyboard,Mouse}, IOBluetoothHIDDriver | boot | ~90KB | HID |
| AppleBluetoothRemote | sys | 33KB | 遥控面 |
| AvpFairPlayDriver | sys | 9.7KB | （筛出噪音，不分析）|

### 用户态守护进程（活体 25G332，独立实体）
| 进程 | 大小 | 关注面 |
|---|---|---|
| bluetoothd | 22.5MB | 私域广播组装/共存/MacCoex（WCM 闭环用其反汇编）|
| sharingd | 18.3MB | AirDrop/接力场景组装/广告载荷 → 剪贴板/AWDL 证据源 |
| useractivityd | 2.1MB | 接力扫描/广告解析（flagCopyPasteKey 消费点）|
| pboard / remoted | 0.1MB/1MB | 剪贴板/身份（AKS 证据已闭环，仅补证）|

### 用户态框架（15.6.1 USB BaseSystem 独立实体，结构参考层）
CoreWiFi 4.4MB / IO80211 606KB / WiFiPolicy / BluetoothManager
> 26.7.2 用户态均为 dyld cache-only；逻辑=15.6.1 结构 + 活体 daemon + 内核面 + 探针交叉验证。
> 全量 cache 逆向属可选增强（需 ipsw 或 Ghidra-cache 重试），暂缓；需要时另行征求同意。

## 1. 工作簿划分
- **W1 IO80211Family 表面**：scan 记录构造（E-4 扫描支路）、roam 咨询/触发链、PHY/channel 聚合 ——
  价值最高（漫游=旗舰；扫描 ax=可修候选）
- **W2 bluetoothd 广播面**：私域广告载荷组装（bit 由系统按芯片能力定）——补证归档
- **W3 sharingd/useractivityd**：接力场景组装流——补证归档（剪贴板已闭环）
- **W4 CoreWiFi/WiFiPolicy**：扫描/漫游判定逻辑（与 W1 联动）
- **W5 Skywalk/AWDL 栈**：AWDL 传输（已判结构性；本簿=证据完善收口）

## 2. 交付物
- 每簿：REVERSE-Wn 报告 + 「系统能力→驱动接口」映射表
- 汇总：可修项清单（含重启预算）/ 结构性边界清单（实证）

## 3. 约束
- 分析免费、可后台；实验每 1 次重启一测；变更全在 kext 侧；不做 6GHz/WPA3/11kvr/LE Audio/AWDL。
