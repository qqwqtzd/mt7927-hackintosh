import subprocess
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True).stdout
full=sh("system_profiler SPAirPortDataType 2>/dev/null")
i=full.find('Current Network Information')
print("=== A. Current Network Information 段 ===")
print(full[i:i+520] if i>=0 else full[:400])
print()
print("=== B. AppleWLANInterfaceSTA 关键属性 ===")
out=sh("ioreg -rc AppleWLANInterfaceSTA -l -w0 2>/dev/null")
for line in out.splitlines():
    kl=line.lower()
    if any(k in kl for k in ['phy','mode','mcs','802.11','channel','band','station','rssi','rate','capab','interfaceunit','mac']) and 'ioreport' not in kl:
        print(line.strip()[:130])
