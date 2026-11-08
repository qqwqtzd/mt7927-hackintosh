# 全项目重新分析（2026-11-08，新模型视角）

> 目的：以新视角重审所有"判死"项，定位真实可行的新路径。
> 前提：昨日 E-4 尝试失败但**暴露出可修复的根因**（见下）。

## 一、昨日失败的真实根因（新发现，关键）

### E-4 内核补丁为何导致关机挂起

昨天 V8 在 `kmod _start` 里直接打补丁，代码已做入口字节校验（地址错会跳过）。
那为什么还会挂起？**新分析找到答案**：

```c
KernelPatcher::kernelWriteLock   // Lilu 在 KernelPatcher::init() 里分配
IOSimpleLockLock(kernelWriteLock);  // ← 若为 NULL，锁操作崩溃/挂起
```

**关键**：我们的 kext 在 `_start`（加载最早期）执行，此时 **Lilu 可能尚未完成
`KernelPatcher::init()`** → `kernelWriteLock == NULL` → `IOSimpleLockLock(NULL)` →
内核异常 → shutdown stall。

**这不是"内核补丁不可行"，而是"时机不对"**。延迟到 Lilu 初始化完成后再补丁即可。

### Lilu 插件机制为何从未工作

昨天排查到 `__DATA,__lilu` section 存在但 `pluginStart` 不执行。新发现：

```
Lilu 插件的 IOService 子类实现来自 kern_start.cpp（编译时 PRODUCT_NAME=插件名）
我们的 build.sh 只链接了 Lilu 导出的 API 符号（_lilu / KernelPatcher / MachInfo），
从未编译 kern_start.cpp → 插件类无实现 → 从未实例化 → pluginStart 永不被调用
```

**修正**：插件必须把 Lilu 的 `kern_start.cpp`（及其依赖）一起编译，或
定义 `LILU_CUSTOM_IOKIT_INIT` 自己实现 IOService（昨天的 V7 尝试，但 IOKit 匹配失败）。

---

## 二、E-4 新方案（3 条路径，按安全性排序）

### E4-1：延迟内核补丁（推荐，1 次重启，风险中低）

```
_start() 里只做一件事：thread_call_enter_delayed(回调, 30 秒)
30 秒后回调：
  ① 读 0xffffff80021a4bc2 处 8 字节
  ② 与 55 48 89 e5 41 57 41 56 比对（不匹配 → 打印并放弃，零风险）
  ③ KernelPatcher::kernelWriteLock 此时已就绪（Lilu 早已初始化）
  ④ 打 4 处补丁（3 处常量 + 1 处 JS→JMP）
```

- **为什么这次更可能成功**：解决"锁未初始化"的时机问题；字节校验保证不会改坏
- **失败形态**：PHY 不变（校验失败跳过），系统正常
- **最坏情况**：补丁生效但下游异常 → 关机挂起（**重启即恢复，因为只改内存**）
- **验证**：重启后 `phyprobe` 看 PHY

### E4-2：用户态 interpose（最安全，0 重启验证）

```
编译 Lilu 插件（含 kern_start.cpp）→ pluginStart 执行
  → lilu.onProcLoad 注册目标进程（System Settings / system_profiler）
  → 注入 dylib
dylib 用 __DATA,__interpose hook _Apple80211CopyValue
  → key==0x0E 且返回成功 → 改写字典 PHYMODE_ACTIVE=0x100 / SUPPORTED=0x19F
```

- **优点**：只影响目标进程；失败无系统影响；**可即时验证（无需重启）**
- **缺点**：需编译 Lilu 多个源文件；注入系统进程依赖 AMFI 放行（本机已配置）
- **工作量**：中等

### E4-3：接受现状（0 风险）

只影响 System Profiler 一行文字；实际链路 HE/160MHz 正常；扫描行已显示 ax。

---

## 三、其余"判死"项复核（结论维持，但补充新证据）

### 剪贴板 Mac→iPhone：结构性（端到端 SE）

新分析确认：即使让 CopyPaste 位=1（hook 谓词），上传仍需 **SE 会话签名**；
对端 iPhone 校验签名 → 失败。**缺的是端到端可信身份，无法伪造**。
→ 维持不可修。

### AWDL / AirDrop / 随航：理论可行但成本极高

新证据（有价值）：
- MT7927 在 Linux（mt76）**支持 monitor mode 和 frame injection**
- AWDL = 私有 IBSS 风格组网（OUI 00:17:F2），**物理层我们有能力**
- 但需要：Apple AWDL 私有协议完整逆向（帧格式、同步、选举）+ 驱动实现专用模式
- 工作量：**月级**；成功率：**<20%**；且用户已明确不做
→ 维持不做（档案完整）

### WCM：已闭环（双机 vmmap 证明全平台一致）
→ 无需动作。

### 固件版本：**我们已最新**
```
WIFI_RAM_CODE_MT6639: 现用 2026-04-14 > ASUS 包 2025-06-06
WIFI_MT6639_PATCH_MCU: 现用 297120B(更新) vs ASUS 299296B(旧版)
BT_RAM_CODE_MT6639: 完全相同
```
→ ASUS 驱动包（asus-driver/）无使用价值，可清理；固件不需更新。

---

## 四、结论与建议

| 项 | 可解性 | 新证据 |
|---|---|---|
| **E-4 显示（ax 标签）** | **可能有解** | 找到昨日失败根因（kernelWriteLock 未初始化），延迟补丁可绕过 |
| 剪贴板 | 不可解 | 端到端 SE 签名（确认）|
| AWDL 族 | 理论可行/不推荐 | 物理层可行（mt76 支持注入），但协议逆向月级 |
| WCM | 无需 | 全平台设计行为 |
| 固件 | 已最新 | ASUS 包是旧版 |

**唯一值得再投入的：E-4 的 E4-1（延迟补丁）**，因为根因明确、风险可控（只改内存）、
验证成本低（1 次重启）。

建议：先做 E4-1；若成功即收工；若失败，转 E4-2 或接受现状。

## 五、E4-1 执行结果（2026-11-08）：内核补丁路径彻底失败，已回滚

### 三次尝试，全部挂起
| 版本 | 设计 | 结果 |
|---|---|---|
| V8 | `_start` 直接打补丁 | 关机挂起 |
| E4-1a | `_start` 里 `thread_call` 延迟 30s | 启动挂起（早期服务未就绪）|
| E4-1b | `IOService::start` + `IOTimerEventSource` | 启动挂起 |
| E4-1c | `IOService::start` 极简（锁检查+memcpy）| **启动挂起** |

### 逐层排除的根因（都有日志实证）
1. ✅ OC 注入成功（`Prelinked injection - Success`）
2. ✅ 依赖正确（删掉不存在的 `com.apple.kernel.6.0` 后无依赖错误）
3. ✅ IOKit 匹配条件补齐（`IOResourceMatch=IOBSD` + 完整 kpi 依赖）
4. ✅ `_start` 清空（早期调用 thread_call 会挂 → 已证实）
5. ❌ **只要 kext 真正执行"读/写 0xffffff80021a4bc2"，启动即挂起**

### 最终结论
**在本环境（Hackintosh + 25G332 kernel collection）中，运行时读写该内核地址会
导致系统挂起（非 panic，无 crash 日志）。** 可能原因：
- kc `__TEXT` 映射在运行时不可写/不可读（setKernelWriting 未真正解除保护）
- 或 `MachInfo::setKernelWriting` 在此环境不生效
- 或地址推算有偏差（kextstat 的 load address 与实际映射不符）

**E-4 内核补丁路径：关闭。** 已完整回滚（kext + config 条目清零）。

### 剩余选项
| 选项 | 可行性 | 成本 |
|---|---|---|
| E4-2 用户态 interpose | 中（需修 Lilu 插件机制 + 写 dylib）| 中，0 系统风险 |
| E4-3 接受现状 | — | 0 |

### 重要工程资产（本次沉淀）
- OC 日志机制（`Target=0x41` 静默写文件）——后续排障必备
- 6 个 kext 工程陷阱清单（见 PLAN-E4A §11）
- `IOResourceMatch=IOBSD` 是 IOKit 服务匹配的必要条件
- `com.apple.kernel.6.0` 依赖在 macOS 26 不存在
