# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
import re
prog = getCurrentProgram()
af = prog.getAddressFactory()
fm = prog.getFunctionManager()
di = DecompInterface(); di.openProgram(prog)
for tgt in ('ffffff8002370026','ffffff800236fe38'):
    t = af.getAddress(tgt)
    callers=set()
    for r in prog.getReferenceManager().getReferencesTo(t):
        f=fm.getFunctionContaining(r.getFromAddress())
        if f: callers.add(f.getEntryPoint())
    print('== callers of', tgt, '->')
    for c in sorted(callers):
        f=fm.getFunctionAt(c)
        res=di.decompileFunction(f,90,monitor)
        code=res.getDecompiledFunction().getC() if res.decompileCompleted() else ''
        print('### caller %s size=%d' % (c, f.getBody().getNumAddresses()))
        print(code[:2600])
        print('#####')
