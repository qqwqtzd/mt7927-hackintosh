from ghidra.program.model.symbol import SymbolType
import re
prog = getCurrentProgram()
symt = prog.getSymbolTable()
want = re.compile(r'ScanCacheStore|ScanSanitizer|WCLAdaptiveRoam|WCLRoamManager|WCLRoamProfile|WCLScanManager|WCLBGScan')
print("W1 targets, string -> referencing function entries:")
rows=[]
it=symt.getAllSymbols(True)
while it.hasNext():
    s=it.next()
    if not want.search(s.getName()): continue
    addr=s.getAddress()
    refs=prog.getReferenceManager().getReferencesTo(addr)
    if not refs: continue
    fm=prog.getFunctionManager()
    seen=set()
    for r in refs:
        f=fm.getFunctionContaining(r.getFromAddress())
        if f and f.getEntryPoint() not in seen:
            seen.add(f.getEntryPoint())
            rows.append((s.getName(False)[:36], str(f.getEntryPoint())))
for nm,ep in rows[:32]:
    print("  %-36s <- %s" % (nm, ep))
print("total refs:", len(rows))
