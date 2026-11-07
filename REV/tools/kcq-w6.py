# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
import re
prog = getCurrentProgram()
af = prog.getAddressFactory()
fm = prog.getFunctionManager()
lo = af.getAddress('ffffff8002188000')
hi = af.getAddress('ffffff80024a0000')
di = DecompInterface(); di.openProgram(prog)
hits=[]
it=fm.getFunctions(True)
while it.hasNext():
    f=it.next()
    a=f.getEntryPoint()
    if a.compareTo(lo)<0 or a.compareTo(hi)>=0: continue
    res=di.decompileFunction(f,60,monitor)
    if not res.decompileCompleted(): continue
    c=res.getDecompiledFunction().getC()
    if ('0xd8' in c or '0x1ae' in c) and ('roam' in c.lower() or 'roam' in c):
        hits.append((str(a), f.getBody().getNumAddresses()))
for a,s in sorted(hits, key=lambda x:-x[1])[:10]:
    print('0x%s size=%d' % (a,s))
