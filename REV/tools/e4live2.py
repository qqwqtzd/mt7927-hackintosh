import subprocess
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True).stdout
print("=== A. Wi-Fi 相关 kext/驱动 ===")
print(sh("kextstat 2>/dev/null | grep -iE '80211|wlan|wifi|BCM|WCM|Skywalk' | head -8"))
print("=== B. 支持 PHY 模式（系统级）===")
print(sh("system_profiler SPAirPortDataType 2>/dev/null | grep -iE 'Supported PHY|PHY Mode|Interfaces|en1:|en0:|802.11' | head -10"))
print("=== C. IO80211 相关 ioreg（避免平台）===")
print(sh("ioreg -rc IO80211NetworkInterface -l -w0 2>/dev/null | grep -E 'IOClass|IOProviderClass|MACAddress|BSD Name' | head -8"))
print(sh("ioreg -rc IO80211Controller -w0 2>/dev/null | grep -E '\\\"IOClass\\\"|\\\"compatible\\\"|\\\"CFBundleIdentifier\\\"' | head -8"))
print("=== D. 当前/最近网络接口 ===")
print(sh("ifconfig en0 2>/dev/null | head -3; echo ---; networksetup -listallhardwareports 2>/dev/null | grep -A2 -i wifi | head -6"))
