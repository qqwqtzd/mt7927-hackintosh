from ghidra.app.decompiler import DecompInterface
prog = getCurrentProgram()
af = prog.getAddressFactory()
fm = prog.getFunctionManager()
di = DecompInterface(); di.openProgram(prog)
T=[0xffffff80022d3494,0xffffff80022de434,0xffffff8002353c5c,0xffffff80021a1924]
for t in T:
    f = fm.getFunctionContaining(af.getAddress('%x' % t))
    if not f:
        print('---',hex(t),'nofunc'); continue
    res=di.decompileFunction(f,90,monitor)
    c=res.getDecompiledFunction().getC() if res.decompileCompleted() else '???'
    print('--- func @%s size=%d' % (f.getEntryPoint(), f.getBody().getNumAddresses()))
    print(c[:1500])
