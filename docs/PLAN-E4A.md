# A 方案设计：E-4 活跃行显示修复（用户态 Lilu 插件）+ 发布形态升级

> 状态：已批准（2026-11-07）；机制前提本机已具备（Lilu 1.7.3 + AMFIPass 1.4.1 在载，
> csr Custom 0xAC0，EFI 注入体系在位）；Kext 注入 /Library/Extensions+AKC 先例已存在（NVRM 等）。

## 1. 目标
- 系统层面"当前活动链路 PHY Mode"从 `802.11ac` 修正为 `802.11ax`（System Settings / system_profiler）。
- 顺带：发布形态升级为"系统内安装（AKC）+ 一键卸载回滚"。

## 2. 机制（用户态 interpose，非内核补丁）

```
Lilu（已注入各进程）→ 我们的插件 kext（MtkPhyFix）注册 FunctionHook
  └─ hook 私有 API：_Apple80211CopyValue（IO80211.framework 导出）
       ├─ 当 key == 0x0E（PHY_MODE）且返回 CFDictionary：
       │    PHYMODE_ACTIVE   := 0x100（单一位！避免 ac 先命中规则）
       │    PHYMODE_SUPPORTED:= 0x19E（含 0x100；不含 0x200）
       └─ 其余调用原样透传
```

改写遵循 CoreWLAN `activePHYMode` 的"先命中先返回"位序（0x80 在 0x100 之前检查）：
ACTIVE 必须**只含 0x100**，否则仍显示 ac（M5 报告 §2.1 实证）。

## 3. 插件规格（MtkPhyFix.kext）
- 基础：Lilu 1.7.3+ 插件（普通 kext，链接 Lilu 导出符号；SDK 取自本机 EFI 的 Lilu.kext）
- Hook 目标：`_Apple80211CopyValue(Apple80211Ref, key, p1, &CFDictionary, flags)`
- 作用域：key==0x0E 且字典含 PHYMODE_ACTIVE/PHYMODE_SUPPORTED；其余透传
- 不碰：CARD_CAPABILITIES、扫描记录、其它 key

## 4. 加载方式（发布形态）
- M2 装机已完成（2026-11-07 20:17）：MtkPhyFix.kext 入 EFI/OC/Kexts + config.plist Kernel/Add:15
  （备份 config.before-mtkphyfix-20261107-201708）

## 4. 加载方式（发布形态）
- 一期（验证）：kext 放入 EFI/OC/Kexts（现有注入体系，无需 AKC 变更）
- 二期（发布形态）：`/Library/Extensions` + `kmutil rebuild`（AKC）+ 安装/卸载/回滚脚本
  （仿 NullMoth 1401 的可用性目标；本机已有 NVRM 等 AKC 先例）
- 卸载/回滚：移除插件的注入/AKC 项 + 重启 ✓

## 5. 验证矩阵
| 观察面 | 预期 |
|---|---|
| system_profiler SPAirPortDataType 当前网络行 | `PHY Mode: 802.11ax`、Supported 含 ax |
| 系统设置 → Wi-Fi → 当前网络详情 | 同上 |
| phyprobe（独立二进制）| 同步 ACTIVE=0x100 —— 内核钩子作用域=全部读方一致（对照组概念升级为全一致）|
| 扫描记录网络行 | 保持 ax（不受影响）|
| 卸载插件重启 | 回到 ac（可逆）|

> 说明：采用内核 routeFunction 后，内核读路径全部（Profiler/设置/phyprobe）统一为 ax；
> 这是"作用域=内核层"一致的验证口径。

## 6. 风险与回滚
- 风险：低（用户态 hook；最坏=Profiler/设置页读取异常，可回滚）
- 回滚：移除 EFI 注入项或 AKC 项 + 重启；全程不改系统二进制
- 兼容：Lilu 大版本更新时插件 API 微调（成本低）；系统大版本需重编译

## 7. 里程碑
- M1 插件工程（手工 skeleto：LiluAPI + FunctionHook，不做 Xcode 工程，沿用项目 clang kext 编译线）
- M2 装载 + 重启 + 验证矩阵（1 次重启）
- M3 发布形态（AKC 安装/卸载/回滚脚本 + 文档）
- M4 SECURITY 扫描 + 推送

## 8. 纪律
- 改动域：[K] kext（插件本身）+ 安装脚本；不碰系统二进制
- 实验 1 重启一测；失败即回滚

## 9. 万一进不了系统的救援预案（重启前就绪）
- 风险说明：Lilu KernelPatcher::routeFunction 失败自带容错（仅记录并回退，不会 panic）；
  插件 start() 无破坏性操作，panic 概率很低。但预置预案：
- **救援物已就位** `EFI/phyfix-restore/`：restore.sh（删 kext+还原 config 前的备份副本）+
  config.plist.before-20261107-201708 + README.txt。
- **三档恢复**：
  1) 系统照常进入（最可能）→ 无需动作；功能异常则删 config 条目+kext 后重启。
  2) 卡在加载/反复重启（panic 类）→ **重启前务必插上 1401 救援 U 盘**（其 EFI 不含
     MtkPhyFix，纯净可启动）→ 从 U 盘启动进系统 → 运行
     `bash /Volumes/EFI0/phyfix-restore/restore.sh` → 回主盘重启。
  3) 连 U 盘也没 → OpenCore 启动选择器选 Recovery → 终端挂载 EFI → 同 2 的脚本。
- Debug 位：现有 boot-args 含 `-v debug=0x100 keepsyms=1`（panic 时可见、留日志）。
