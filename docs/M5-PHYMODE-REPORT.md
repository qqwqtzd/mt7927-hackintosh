# E4 逆向报告：系统如何判定 Wi-Fi PHY Mode（physicalLayerMode / fastestSupportedPHYMode）

- 执行机：MacBook Air M5（Apple Silicon / arm64e）
- 系统：macOS 27.2，Build 26B123（比任务书假设的 26.x 更新）
- 原则：全程只读；未修改系统文件、未注入、未关 SIP、未使用 sudo
- 结论针对：`system_profiler SPAirPortDataType` / 系统设置里显示的 “PHY Mode”

---

## 0. 结论（先看这里）

**用户可见的 “PHY Mode” 走的是 Apple80211 ioctl，不是 IOReport，也不是直接读 CARD_CAPABILITIES 的某个字节。**

判定链（自上而下）：

```
系统设置 / system_profiler
  └─ CoreWLANKit：CWPHYMode(枚举/位) → "802.11ax" 字符串（0x213cf5b60）
       ↑
  └─ CoreWLAN.framework
       -[CWInterface activePHYMode]              （枚举 1=a,2=b,3=g,4=n,5=ac,6=ax,7=be）
       -[CWInterface(Private) physicalLayerMode] （位掩码 a/b/g/n/ac/ax/be）
       ↑ 传入的是同一个 Apple80211 PHY_MODE 位掩码
  └─ CoreWiFi.framework
       -[CWFInterface PHYMode]                   （位掩码，经 XPC）
       -[CWFXPCConnection queryPHYModeWithRequestParams:reply:]
       ↑
  └─ CoreWiFi 的 Apple80211 直连层
       -[CWFApple80211 activePHYMode:]           → Apple80211CopyValue(ref, 0x0E)
       -[CWFApple80211 supportedPHYModes:]       → Apple80211CopyValue(ref, 0x0E)
       ↑ 返回 CFDictionary：
         "PHYMODE_ACTIVE"    = 当前 PHY（单位位）
         "PHYMODE_SUPPORTED" = 支持的 PHY 位掩码
       ↑
  └─ IO80211.framework
       _Apple80211CopyValue(ref, 0x0E, 0, dict, 8)
         → CFDictionaryCreateMutable()
         → Apple80211GetWithIOCTL(ref, 0x0E, 0, dict, 8)
       ↑
  └─ IO80211Family.kext / Wi-Fi 驱动（内核侧）
       填充上述 CFDictionary 的两个 key；
       或通过 IO80211Interface 的 PHY 能力 getter 提供同样的位值。
```

扫描到的网络（`CWNetwork` / `CWFScanResult`）是另一条支路：

```
-[CWFScanResult fastestSupportedPHYMode]
  └─ _CWFFastestPHYMode( [self supportedPHYModes] )
       └─ -[CWFScanResult supportedPHYModes]
            └─ -[NSDictionary(ScanRecord) supportedPHYModes]
                 └─ scanRecord["PHY_MODE"]   （NSNumber 位掩码）
                    缺省回退 scanRecord["RATES"]
```

> 任务书里提到的 `GET PHY MODE` / `GET SUPP PHY MODES` 只是 CoreWiFi 的 XPC/日志命令名（位于
> `__cstring` 的命令名簇 0x19c1703e0 附近），真正参与显示判定的是数值型 key
> `PHYMODE_ACTIVE` / `PHYMODE_SUPPORTED`，不是这两个字符串。

---

## 1. PHY 位值表（本机实测，非猜测）

Apple80211 `APPLE80211_PHY_MODE_*` 位值（由反汇编逐位确认）：

| PHY | Apple80211 位 | CoreWLAN `CWPHYMode` 位 | 显示字符串 |
|---|---|---|---|
| 11a | `0x0002` (bit1) | `0x02` | `802.11a` |
| 11b | `0x0004` (bit2) | `0x04` | `802.11b` |
| 11g | `0x0008` (bit3) | `0x08` | `802.11g` |
| 11n | `0x0010` (bit4) | `0x10` | `802.11n` |
| 11ac | `0x0080` (bit7) | `0x20` | `802.11ac` |
| **11ax** | **`0x0100` (bit8)** | **`0x40`** | **`802.11ax`** |
| 11be | `0x0200` (bit9) | `0x80` | `802.11be` |

`_CWFFastestPHYMode` 取“最高位”：be > ax > ac > n > a > g > b。
所以要让系统认为“最快/当前是 11ax”，必须让 `0x100` 出现，且 **不要误置 `0x200`**。

---

## 2. 驱动侧需要满足的具体值（可执行结论）

### 2.1 主路径：`APPLE80211_IOC_PHY_MODE`（ioctl 号 0x0E）

驱动对 `APPLE80211_IOC_PHY_MODE` 的 GET 响应必须包含：

| 字段/key | 类型 | 11ax 时应为 |
|---|---|---|
| `PHYMODE_ACTIVE` | CFNumber / uint32 | **`0x0100`（只置这一位）** |
| `PHYMODE_SUPPORTED` | CFNumber / uint32 位掩码 | **包含 `0x0100`**，例如 `0x019E` = a+b+g+n+ac+ax |

**非常重要（本机反汇编发现的坑）：**

`-[CWInterface activePHYMode]` 的判定顺序是：

```
bit1 → 1(a), bit2 → 2(b), bit3 → 3(g), bit4 → 4(n),
bit7(0x80) → 5(ac),  bit8(0x100) → 6(ax),  bit9(0x200) → 7(be)
```

它是**先命中先返回**。因此：

- `PHYMODE_ACTIVE = 0x180`（ac|ax）→ 返回 5，仍显示 **802.11ac**
- `PHYMODE_ACTIVE = 0x19E`（全家桶）→ 返回 1，可能显示 **802.11a**
- `PHYMODE_ACTIVE = 0x100` → 返回 6，显示 **802.11ax** ✅

> 也就是说：`PHYMODE_SUPPORTED` 可以是位掩码；但 `PHYMODE_ACTIVE` 必须是
> **单一 bit 0x100**。Hackintosh 驱动常见写法是把 active 也填成 supported 的掩码，
> 这会直接导致“跑在 11ax、显示 11ac”。

### 2.2 扫描结果支路：`scanRecord["PHY_MODE"]`

驱动返回的每个 BSS/扫描记录里，`PHY_MODE` 也应是 Apple80211 位掩码，并包含 `0x100`：

- 11ax AP：`PHY_MODE` 至少 `0x100`（推荐 `0x19E`）
- 若缺失该 key，CoreWiFi 会回退解析 `RATES`（`scanRecord["RATES"]`）。
  旧式 RATES 推导通常只能推到 11ac，于是网络详情/排序会继续显示 ac。

### 2.3 次要检查：CARD_CAPABILITIES / HE_CAP（中置信度）

- `APPLE80211_IOC_CARD_CAPABILITIES` 的 ioctl 号是 `0x0C`（本机名字表确认，见 §4）。
- 建议同时确保能力集中声明 11ax/HE 能力，避免 IO80211Family.kext 在聚合
  `PHYMODE_SUPPORTED` 时把 ax 裁掉。
- 但**用户可见的当前 PHY Mode 不是**直接读 CARD_CAPABILITIES 的某个字节；
  它读的是 `APPLE80211_IOC_PHY_MODE`（0x0E）返回的 `PHYMODE_ACTIVE`。
- 本机只有 `IO80211Family.kext/Contents/Info.plist`，没有内核二进制，故
  “kext 如何由能力位聚合 supported”这一层未能直接反汇编，标注为中置信度。

### 2.4 不要做的事

- 不要把 `0x200`(be) 置位来“凑”ax；`fastestSupportedPHYMode` 会优先看到 be。
- 不要把 `PHYMODE_ACTIVE` 填成 supported 掩码（见 §2.1 顺序坑）。
- 不需要、也不建议为此改 IOReport 通道。

---

## 3. 关键方法 + 反汇编证据

> 地址均为 dyld shared cache 的 unslid vmaddr；反汇编用 `ipsw dyld disass --vaddr`。

### 3.1 `-[CWFApple80211 activePHYMode:]`（CoreWiFi @ 0x19be43108）

```asm
0x19be43148  ldur x8, [fp, #-0x20]
0x19be4314c  ldr  x0, [x8, #0x20]        ; self->_apple80211Ref
0x19be43150  mov  w1, #0xe               ; APPLE80211_IOC_PHY_MODE
0x19be43154  bl   _Apple80211CopyValue   ; 返回 CFDictionary
0x19be43184  ldr  x1, [sp, #0x30]
0x19be43188  ldur x0, [fp, #-0x18]       ; 返回的字典
0x19be4318c  adrp x2, 0x1e443b000
0x19be43190  add  x2, x2, #0x9b8         ; CFString "PHYMODE_ACTIVE"
0x19be43194  bl   objc_msgSend           ; -objectForKeyedSubscript:
```

`supportedPHYModes:`（0x19be432c8）同构，只是 key 偏移 `#0x9d8`。

### 3.2 CFString 实体解码（证明 key 名称）

`sig = {isa, flags=0x7c8, pad, cstr, length}`，cstr 实际地址 = `0x180000000 + (raw & 0xffffffff)`：

```
0x1e443b9b8: cstr=0x19c17b2a8 len=14 -> "PHYMODE_ACTIVE"
0x1e443b9d8: cstr=0x19c17b2b7 len=17 -> "PHYMODE_SUPPORTED"
```

对应 CoreWiFi 字符串：
- `PHYMODE_ACTIVE` @ `0x19c17b2a8`
- `PHYMODE_SUPPORTED` @ `0x19c17b2b7`

### 3.3 `_CWFFastestPHYMode`（CoreWiFi @ 0x19bd5be6c）

```asm
0x19bd5be98  tbz  w8, #0x9, ...   ; 0x200 be
0x19bd5bea0  mov  w8, #0x200
0x19bd5beb0  tbz  w8, #0x8, ...   ; 0x100 ax
0x19bd5beb8  mov  w8, #0x100
0x19bd5bec8  tbz  w8, #0x7, ...   ; 0x080 ac
0x19bd5bed0  mov  w8, #0x80
0x19bd5bee0  tbz  w8, #0x4, ...   ; 0x010 n
0x19bd5bee8  mov  w8, #0x10
0x19bd5bef8  tbz  w8, #0x1, ...   ; 0x002 a
0x19bd5bf00  mov  w8, #0x2
0x19bd5bf10  tbz  w8, #0x3, ...   ; 0x008 g
0x19bd5bf18  mov  w8, #0x8
0x19bd5bf28  tbz  w8, #0x2, ...   ; 0x004 b
0x19bd5bf30  mov  w8, #0x4
```

### 3.4 `_CWFDescriptionForPHYMode`（CoreWiFi @ 0x19bd36ca4）

逐位测试并追加字符串，CFString 解码结果：

```
bit2(0x04) -> "b/"    0x1e44358f8
bit3(0x08) -> "g/"    0x1e4435918
bit1(0x02) -> "a/"    0x1e4435938
bit4(0x10) -> "n/"    0x1e4435958
bit7(0x80) -> "ac/"   0x1e4435978
bit8(0x100)-> "ax/"   0x1e4435998
bit9(0x200)-> "be/"   0x1e44359b8
mode==0    -> "none"  0x1e4433678
默认        -> "?"     0x1e44359d8
```

### 3.5 `-[CWFScanResult fastestSupportedPHYMode]` / `supportedPHYModes`

`fastestSupportedPHYMode`（0x19bfcce94）：

```asm
0x19bfccec0  ldr  x0, [sp, #0x10]
0x19bfccec4  bl   0x1a0019620          ; objc_msgSend(self, supportedPHYModes)
0x19bfccec8  bl   _CWFFastestPHYMode   ; 对 supportedPHYModes 取最高位
0x19bfccecc  str  w0, [sp, #0x4]
```

`-[NSDictionary(ScanRecord) supportedPHYModes]`（0x19c0572dc）：

```asm
0x19c05731c  adrp x2, 0x1e443f000
0x19c057320  add  x2, x2, #0x598      ; CFString "PHY_MODE"
0x19c057324  bl   0x1a0002d00         ; -objectForKeyedSubscript:
...
0x19c057348  bl   0x1a0004360         ; -unsignedIntValue
...
0x19c057364  add  x2, x2, #0xcd8      ; CFString "RATES"（回退）
```

CFString 解码：
```
0x1e443f598 -> "PHY_MODE"   （长度 8）
0x1e4442cd8 -> "RATES"      （长度 5）
```

### 3.6 CoreWLAN：`activePHYMode` 的顺序坑（0x1ae77ff64）

```asm
0x1ae77ff98  tbnz w0, #0x1, ...   ; -> mov w0,#1  (a)
0x1ae77ffa4  tbnz w0, #0x2, ...   ; -> mov w0,#2  (b)
0x1ae77ffb0  tbnz w0, #0x3, ...   ; -> mov w0,#3  (g)
0x1ae77ffbc  tbnz w0, #0x4, ...   ; -> mov w0,#4  (n)
0x1ae77ffc8  tbnz w0, #0x7, ...   ; -> mov w0,#5  (ac)   <-- 0x80 先于 0x100
0x1ae77ffd4  tbnz w0, #0x8, ...   ; -> mov w0,#6  (ax)
0x1ae77ffe0  tst  w0, #0x200      ; -> 7 (be) 或 0
```

这就是“驱动返回 ac|ax 仍显示 ac”的原因。

### 3.7 CoreWLAN：`physicalLayerMode` 的位重映射（0x1ae78050c）

```asm
0x1ae780544  and  w20, w0, #0x2      ; a
0x1ae780550  and  w8,  w0, #0x4      ; b
0x1ae780560  and  w21, w0, #0x8      ; g
0x1ae78056c  and  w8,  w0, #0x10     ; n
0x1ae780580  lsr  w8, w0, #0x2
0x1ae780584  and  w21, w8, #0x20     ; ac  <- 0x80 >> 2
0x1ae780590  lsr  w8, w0, #0x2
0x1ae780594  and  w8, w8, #0x40      ; ax  <- 0x100 >> 2
0x1ae7805a8  lsr  w8, w0, #0x2
0x1ae7805ac  and  w8, w8, #0x80      ; be  <- 0x200 >> 2
```

即 CoreWLAN 把 Apple80211 的 a/b/g/n/ac/ax/be = 0x2/0x4/0x8/0x10/0x80/0x100/0x200
映射成 CWPHYMode 位 0x2/0x4/0x8/0x10/0x20/0x40/0x80。

### 3.8 CoreWLANKit 显示字符串（0x213cf5b57）

相邻字符串表：

```
0x213cf5b57  "802.11be"
0x213cf5b60  "802.11ax"
0x213cf5b69  "802.11ac"
0x213cf5b71  "802.11n"
0x213cf5b7a  "802.11g"
0x213cf5b82  "802.11a"
0x213cf5b8a  "802.11b"
0x213cf5c2b  "kAirPortPHYModeInfoKey"
```

`activePHYMode` 返回 6 → 该表取 “802.11ax”。

### 3.9 IO80211：`_Apple80211CopyValue` key 0x0E 的实际动作（0x1c40452c0）

key 分发跳表：key 0x0E（14）→ 入口 15 → `0x1c4045610`：

```asm
0x1c4045610  adrp x8, 0x1e0164000
0x1c4045614  ldr  x8, [x8, #0x70]        ; kCFAllocatorDefault
0x1c4045618  ldr  x0, [x8]
0x1c404561c  adrp x2, 0x1e0164000
0x1c4045620  ldr  x2, [x2, #0x638]       ; kCFTypeDictionaryKeyCallBacks
0x1c4045624  adrp x3, 0x1e0164000
0x1c4045628  ldr  x3, [x3, #0x640]       ; kCFTypeDictionaryValueCallBacks
0x1c404562c  mov  x1, #0
0x1c4045630  bl   _CFDictionaryCreateMutable
...
0x1c4045660  mov  x0, x20                ; aRef
0x1c4045664  mov  x1, x21                ; 0x0E
0x1c4045668  mov  w2, #0
0x1c404566c  mov  w4, #0x8
0x1c4045670  bl   _Apple80211GetWithIOCTL   ; 由底层填充字典
```

结论：字典由 IO80211/kext/驱动侧填充，用户态只负责把结果转成 NSNumber。

### 3.10 ioctl 编号 = 0x0E 的证据（IO80211 名字表顺序）

IO80211 `__cstring` @ 0x1c4097d80 起的名字表严格按 1 基枚举排列：

```
SSID, AUTH_TYPE, CIPHER_KEY, CHANNEL, POWERSAVE, PROTMODE, TXPOWER, RATE,
BSSID, SCAN_REQ, SCAN_RESULT, CARD_CAPABILITIES, STATE, PHY_MODE, OP_MODE,
RSSI, NOISE, INT_MIT, POWER, ASSOCIATE, ...
```

计数：`CARD_CAPABILITIES` = 第 12 项 = `0x0C`，`PHY_MODE` = 第 14 项 = **`0x0E`**。
与 `CWFApple80211` 传入的 `w1=0xE` 完全吻合。

---

## 4. 相关字符串索引（本机）

### CoreWiFi
| 字符串 | 地址 |
|---|---|
| `GET SUPP PHY MODES` | `0x19c1703ea` |
| `GET PHY MODE` | `0x19c1703fd` |
| `supported_phy_modes` | `0x19c173a19` |
| `fastest_supported_phy_mode` | `0x19c173a2d` |
| `PHYMODE_ACTIVE` | `0x19c17b2a8` |
| `PHYMODE_SUPPORTED` | `0x19c17b2b7` |
| `PHY_MODE` | `0x19c184798` |
| `HE_CAP` | `0x19c1895b9` |
| `phy=%@,` | `0x19c185346` |

### IO80211.framework
| 字符串 | 地址 |
|---|---|
| `PHYMODE_SUPPORTED` | `0x1c408d8bd` |
| `PHYMODE_ACTIVE` | `0x1c408d8cf` |
| `PHY_MODE` | `0x1c4089199` |
| `APPLE80211_IOC_CARD_CAPABILITIES` | `0x1c4097e87` |
| `APPLE80211_IOC_PHY_MODE` | `0x1c4097ebd` |
| `APPLE80211_IOC_HE_CAPABILITY` | `0x1c409c457` |

### CoreWLAN / CoreWLANKit
| 字符串 | 地址 |
|---|---|
| `-[CWInterface(Private) physicalLayerMode]`（selector） | libobjc 去重表 |
| `802.11be` / `802.11ax` / `802.11ac` | `0x213cf5b57` / `0x213cf5b60` / `0x213cf5b69` |
| `kAirPortPHYModeInfoKey` | `0x213cf5c2b` |

---

## 5. 方法与复现

### 环境/工具
- 系统为 macOS 27.2（26B123），`CoreWiFi.framework` **无磁盘实体**，只有 Resources；
  实现位于 `dyld shared cache`。
- 实际使用：
  - `ipsw 3.1.743`（Homebrew 安装；`ipsw dyld macho/str/disass/a2s/xref`）
  - `aa`（解 atlas，获取分片/镜像映射）
  - `python3`（解析 Mach-O、CFString、adrp+add 交叉引用）
  - `strings` / `grep` / `otool`（辅助）
- `dyld_shared_cache_arm64e` 位于：
  `/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/`

### 复现片段

```bash
D=/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_arm64e

# 1) 字符串定位
ipsw dyld str "$D" PHYMODE_ACTIVE PHYMODE_SUPPORTED PHY_MODE \
                     APPLE80211_IOC_PHY_MODE physicalLayerMode

# 2) 符号/方法地址
ipsw dyld macho "$D" CoreWiFi -n | grep -iE 'PHYMode|Apple80211'
ipsw dyld macho "$D" CoreWLAN -n | grep -iE 'physicalLayerMode|activePHYMode'
ipsw dyld macho "$D" /System/Library/PrivateFrameworks/IO80211.framework/Versions/A/IO80211 -n \
  | grep -iE 'CopyValue|GetWithIOCTL'

# 3) 反汇编关键函数
ipsw dyld disass "$D" --vaddr 0x19be43108 --count 60   # CWFApple80211 activePHYMode:
ipsw dyld disass "$D" --vaddr 0x19bd5be6c --count 80   # _CWFFastestPHYMode
ipsw dyld disass "$D" --vaddr 0x19bd36ca4 --count 200  # _CWFDescriptionForPHYMode
ipsw dyld disass "$D" --vaddr 0x1ae77ff64 --count 40   # CWInterface activePHYMode
ipsw dyld disass "$D" --vaddr 0x1c40452c0 --count 200  # Apple80211CopyValue
```

兼容性说明：本机没有 Xcode 的 `dyld_shared_cache_util`；`ipsw` 直接读取分片 cache 最省事。
若在其它机器复现，`ipsw` 路径取主 cache 文件即可。

---

## 6. 置信度与限制

| 结论 | 置信度 | 依据 |
|---|---|---|
| 显示链最终读 `APPLE80211_IOC_PHY_MODE`(0x0E) 的 `PHYMODE_ACTIVE` | 高 | 反汇编 + key CFString 解码 + ioctl 名字表 |
| PHY 位值 a/b/g/n/ac/ax/be = 0x2/0x4/0x8/0x10/0x80/0x100/0x200 | 高 | `_CWFDescriptionForPHYMode` 逐位 CFString 解码 |
| `PHYMODE_ACTIVE` 必须是单位 0x100，否则 ac 抢先 | 高 | `CWInterface activePHYMode` 的 tbnz 顺序 |
| 扫描支路读 `scanRecord["PHY_MODE"]`，缺省回退 `RATES` | 高 | `NSDictionary(ScanRecord)` 反汇编 + CFString |
| CARD_CAPABILITIES(0x0C)/HE_CAP 会影响 supported 聚合 | 中 | 用户态只见 key/字符串；IO80211Family 内核二进制不在本机 |
| 与 IOReport 无关 | 高（对显示路径） | 链路上无 IOReport 调用；PHY 键来自 ioctl 字典 |

未覆盖：`IO80211Family.kext` 只有 Info.plist，内核实现位于 kernel collection（本机
`com.apple.kernelcaches/kernelcache` 为 restore/最小集合，未含 IO80211Family），
因此“kext 内部如何由驱动能力位合成 `PHYMODE_SUPPORTED`”未能直接反汇编。

---

## 7. 给 Hackintosh 驱动的最小改动清单

1. **当前 PHY（最关键）**
   - `APPLE80211_IOC_PHY_MODE` 响应中的 `PHYMODE_ACTIVE` 在 HE 关联时返回 **`0x100`（单个位）**。
   - 确认没有同时带上 `0x80`；否则 `activePHYMode` 先命中 ac。
2. **支持集合**
   - `PHYMODE_SUPPORTED` 包含 `0x100`（例如 `0x19E`），不要误置 `0x200`。
3. **扫描记录**
   - 每个 BSS 的 `PHY_MODE` 位掩码包含 `0x100`；否则会回退 `RATES`，大概率停在 ac。
4. **能力声明（建议）**
   - 同步检查 `APPLE80211_IOC_CARD_CAPABILITIES` / HE 能力声明，避免 kext 聚合 supported 时裁掉 ax。
5. **验证**
   - `system_profiler SPAirPortDataType` 的当前网络 `PHY Mode` 应变为 `802.11ax`；
   - 相关私有 API 可观察 `-[CWInterface activePHYMode]` 返回 6、`-[CWInterface(Private) physicalLayerMode]` 含 `0x40`。

---

### 一句话总结

系统显示 PHY Mode 的根判定是：**Wi-Fi 驱动对 `APPLE80211_IOC_PHY_MODE`(0x0E) 返回的
`PHYMODE_ACTIVE` 位值**。11ax 对应 `0x100`，且 `PHYMODE_ACTIVE` 必须是单独一位；
把它填成 `0x80|0x100` 或整个 supported 掩码，都会被 CoreWLAN 的“先命中先返回”逻辑判成
802.11ac（或更低）。
