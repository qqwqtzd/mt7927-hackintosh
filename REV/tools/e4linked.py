import subprocess
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True).stdout
print("=== 1. M5 当前网络信息（PHY Mode 显示）===")
print(sh("system_profiler SPAirPortDataType 2>/dev/null | sed -n '/Current Network Information/,+12p'"))
print("=== 2. 关联接口的 PHY/模式属性（IOUserNetworkWLAN）===")
print(sh("ioreg -rc IOUserNetworkWLAN -l -w0 2>/dev/null | grep -iE 'PHY|Mode|MCS|Channel|capab|802.11' | head -20"))
print("=== 3. 关联接口的角色/基属性 ===")
print(sh("ioreg -rc IOUserNetworkWLAN -l -w0 2>/dev/null | grep -E 'IOUserClass|IO80211InterfaceRole=\"|MAC|BSD Name' | head -8"))
print("=== 4. en1/en0 Wi-Fi 状态 ===")
print(sh("ifconfig 2>/dev/null | grep -A3 '^en1:' | head -6; networksetup -getairportnetwork en0 2>/dev/null || true"))
