# USB 端口清单（示例与排查方法）

本文件是 **USB map 排查方法**，其中列出的端口/设备信息是**某台测试机的示例**，
不代表你的机器。**照着自己机器的 `ioreg -p IOUSB` 输出排查，不要照抄表格。**

## 方法一：列出所有当前 USB 设备

```bash
python3 usb-list.py
# 或
ioreg -p IOUSB -l -w0 | grep -E '"USB Product Name"|idVendor|idProduct'
```

找到你要关注的两个设备：

- **MediaTek 蓝牙**（MT7927 常见 `0489:e110` 或类似）
- 任何"USB Audio / input 设备"等

## 方法二：确认端口归属（哪组 root port / hub）

```bash
ioreg -p IOUSB -l -w0
```

看每个设备挂在哪组端口上；再对照 USBToolBox（Windows 下出 map 的工具）的端口号，
**macOS 的 USB2.0(HS) 与 USB3.x(SS) 端口是配对的**（同一物理口各占一个口）。

## 方法三：15 口上限

- macOS 一个 hub 最多 15 个有效端口。
- 端口数超了，系统会随机踢掉部分设备——**最常见症状就是蓝牙间歇性消失**。
- 解法：USB map 里去掉用不到的端口（如机箱没接的前置 USB2.0 header），
  保留：主板后置全部、前置在用的、蓝牙、音频等必须的。

## 示例布局（某测试机，仅供参考，不要照抄）

| 端口 | 设备示例 | 说明 |
|---|---|---|
| 1–5 + 6–10 | USB3.x 配对（SS/HS）| 后置/机箱可用的 3.0 口 |
| 11–14 | （被移除）| 机箱未接的前置 USB2.0 header |
| 15 | input 设备 | 某些主板的专用 input 口 |
| 16 | USB Audio | 音频设备（可选）|
| 17 | MediaTek Bluetooth | **必须保留** |

> 在你的机器上，这些端口号/设备完全不同；用 `usb-list.py` 实际输出为准。
> 调整后记得 `sudo kextcache -i /` 或重启验证。

## 相关文件

- `dist/UTBMap-MT7927.kext`：某测试机的成品 map（仅作二进制示例；**请为你的机器
  从 USBToolBox 重新生成**）。
