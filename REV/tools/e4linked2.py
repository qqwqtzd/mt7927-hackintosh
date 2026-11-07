import subprocess
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True).stdout
full=sh("system_profiler SPAirPortDataType 2>/dev/null")
i=full.find('Current Network Information')
print("=== 1. Current Network Information ===")
print(full[i:i+420] if i>=0 else "(未找到)", )
print("...")
j=full.find('Supported PHY Modes')
print("=== Support/PHY ===")
for line in full.splitlines():
    if 'PHY Mode' in line or 'Supported PHY' in line or 'Transmit Rate' in line or 'MCS Index' in line or 'Channel:' in line:
        print(line.strip())
