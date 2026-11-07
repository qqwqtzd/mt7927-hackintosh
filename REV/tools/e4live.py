import subprocess
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True).stdout
print("=== 1. M5 当前网络信息（原生 PHY Mode 显示）===")
print(sh("system_profiler SPAirPortDataType 2>/dev/null | sed -n '/Current Network Information/,+11p'"))
print("=== 2. M5 Wi-Fi 控制器属性 ===")
print(sh("ioreg -c IO80211Controller -l -w0 2>/dev/null | grep -E 'IOClass|compatible|CFBundleIdentifier|IONameMatched|PrimaryInterface|PHY' | head -14"))
print("=== 3. M5 IO80211Interface PHY/capability ===")
print(sh("ioreg -rc IO80211Interface -l -w0 2>/dev/null | grep -iE 'PHY|phyMode|Channel|MCS|capabil' | head -12"))
print("=== 4. M5 Wi-Fi 驱动名/kext ===")
print(sh("ioreg -l -w0 -c IO80211Interface 2>/dev/null | grep -E 'IOProviderClass|IOClass' | head -4"))
print(sh("kextstat 2>/dev/null | grep -iE '80211|AirPort|WCM' | head -6"))
