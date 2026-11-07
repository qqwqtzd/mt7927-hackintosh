# REV/tools — 逆向分析工具集（全部公开通用版，无个人数据）

## 内核侧
| 工具 | 用途 |
|---|---|
| kc-slice.py | 从 BootKernelExtensions.kc（kc2 格式）PRELINK 提取指定 kext 的可执行切片 |
| kcq-w1.py | Ghidra headless：字符串→引用函数映射（类方法定位模板） |
| kcq-w2.py | Ghidra headless：批量函数反编译模板 |
| kcq-w3.py | Ghidra headless：数据引用/虚表槽探查模板 |
| kcq-w4.py | Ghidra headless：类字符串引用函数枚举+反编译 |
| kcq-w5.py | Ghidra headless：调用方枚举+反编译 |
| kcq-w6.py | Ghidra headless：常量特征函数搜索（0xd8/0x1ae 漫游命令）|
| dsc_extract.py | dyld shared cache 镜像工具 |

## 实机探针（零重启）
| 工具 | 用途 |
|---|---|
| phyprobe.c | dlopen 私有 IO80211 → Apple80211CopyValue(0x0E) PHY 字典（PHYMODE_ACTIVE/SUPPORTED）复测 |

## E-4 分析期脚本（串→xref 定位管道）
e4linked*.py（Mach-O 段解析+LEA/SEL 引用扫描）、e4live*.py（本机实测）、
e4ls*.py（符号/串列表）、e4probe*.py（cache 探针）、e4scan*.py（段/引用扫）——
用途：独立 Mach-O/私有框架的字符串定位与交叉引用管线；模板可改路径复用。

## 用法约定
- Ghidra 项目：kcproj（kc 本体不入 git，`kc-slice.py` + analyzeHeadless 命令见 SYS-SIDE-WORKPLAN）
- Jython 脚本需 `# -*- coding: utf-8 -*-`，print 不支持 flush=True
- 下载规范：GitHub 一律走本地代理（见 ~/rea-tools/notes/net-mirrors.md，不入库）
