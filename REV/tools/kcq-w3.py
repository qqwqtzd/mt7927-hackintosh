# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
import struct
prog = getCurrentProgram()
af = prog.getAddressFactory()
fm = prog.getFunctionManager()
rm = prog.getReferenceManager()
# 数据引用 0x22d3494（虚表槽）
target = af.getAddress('ffffff80022d3494')
di = DecompInterface(); di.openProgram(prog)
res = []
it = rm.getReferencesTo(target)
vt = None
for r in it:
    if r.getReferenceType().isData():
        vt = r.getFromAddress()
        break
print('vtable slot at:', vt)
if vt:
    # 打印该表前后各 8 个槽的函数地址
    store = vm = None
    # Ghidra 数据块线性读：从 vt-8*6 开始读 16*8 字节
    start = vt.subtract(6*8)
    for i in range(16):
        a = start.add(i*8)
        try:
            val = prog.getMemory().getInt(a) & 0xffffffff
            val2 = (prog.getMemory().getInt(a.add(4)) & 0xffffffff) << 32
            va = val2 | val
            f = fm.getFunctionContaining(af.getAddress('%x' % va))
            size = f.getBody().getNumAddresses() if f else 0
            print('  +%d: 0x%x size=%d' % (i-6, va, size))
        except Exception as e:
            print('  +%d: err %s' % (i-6, e))
