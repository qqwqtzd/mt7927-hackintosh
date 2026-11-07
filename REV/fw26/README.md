# REV/fw26 — 26.7.2 (25G332) 完整系统资源库（唯一活版；15.6.1 已移入 legacy 停用）

## 来源（全部 25G332，双源）
1. **pkg payload 树**（InstallAssistant-26.7.2.pkg → SharedSupport.dmg → payload.001~053，
   ipsw ota extract 抽取）：
   - usermode/：bluetoothd（23,555,568B，与活体 /usr/sbin/bluetoothd 逐字节一致）/
     useractivityd / pboard / remoted / InternetSharing / mediaremoted
   - fw26/：WiFiPolicy XPC 服务、WCM 共存表（CoEx-Table-*.plist）、框架资源
2. **活体系统 dyld cache 实体化**（ipsw dyld extract，x86_64h 25G332 真二进制）：
   - cache/CoreWiFi 6.7MB / Sharing 6.1MB / WiFiPolicy 2.6MB / CoreWLAN 822KB /
     UserActivity 703KB / IO80211 555KB / BluetoothManager 154KB / WirelessCoexManager 114KB
3. **内核层**（../kernel/）：BootKernelExtensions.kc REGION 切片（13 个 kext，kc-slice.py）

## 分析通道（ipsw 3.1.743，~/.rea-tools/bin/ipsw）
- 框架符号：ipsw dyld macho <cache路径> CoreWiFi -n | grep sel
- 反汇编：  ipsw dyld disass <cache路径> --vaddr 0x... | ipsw dyld a2s <cache> <addr>
- 用户态 cache = /System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_x86_64h
- 内核层：Ghidra 项目 ~/rea-tools/kcproj（kc 本体不入 git，脚本可重建）

## 下载规范
GitHub 下载一律走本地代理 127.0.0.1:7893（curl -x），见 ~/rea-tools/notes/net-mirrors.md。
