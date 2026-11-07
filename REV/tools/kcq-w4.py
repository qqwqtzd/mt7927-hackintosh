from ghidra.app.decompiler import DecompInterface
import re
prog = getCurrentProgram()
symt = prog.getSymbolTable()
fm = prog.getFunctionManager()
di = DecompInterface(); di.openProgram(prog)
want = re.compile(r'AdaptiveRoam')
funcs=set()
it=symt.getAllSymbols(True)
while it.hasNext():
    s=it.next()
    if not want.search(s.getName()): continue
    for r in prog.getReferenceManager().getReferencesTo(s.getAddress()):
        f=fm.getFunctionContaining(r.getFromAddress())
        if f: funcs.add(f.getEntryPoint())
print('AdaptiveRoam referencing-functions:', len(funcs))
for a in sorted(funcs):
    f=fm.getFunctionAt(a)
    print('### func %s size=%d' % (a, f.getBody().getNumAddresses()))
    res=di.decompileFunction(f,90,monitor)
    if res.decompileCompleted():
        c=res.getDecompiledFunction().getC()
        print(c[:2000])
    print('#####')
