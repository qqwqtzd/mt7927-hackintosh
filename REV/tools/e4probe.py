import struct, sys
f='/System/Library/dyld/dyld_shared_cache_arm64e'
try:
    h=open(f,'rb').read(8192)
except Exception as e:
    print('open err', e); sys.exit(0)
print('magic:', h[:16])
mo,mc=struct.unpack_from('<2I', h, 16)
print('mappingOffset', mo, 'mappingCount', mc)
print('raw 16..160 (u32 > 0):')
for off in range(16, 160, 4):
    v=struct.unpack_from('<I', h, off)[0]
    if v: print(f'  off 0x{off:03x} = {v} (0x{v:x})')
# mapping 表
for i in range(min(mc,8)):
    e=mo+i*40
    a,s,fo,p,q=struct.unpack_from('<QQQII', h, e)
    print(f'm{i}: addr=0x{a:x} size=0x{s:x} fileoff=0x{fo:x} prot={p}/{q}')
