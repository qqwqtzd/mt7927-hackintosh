import struct
f='/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_arm64e'
h=open(f,'rb').read(8192)
print('magic:', h[:20])
mo,mc=struct.unpack_from('<2I', h, 16)
print('mappingOffset', mo, 'mappingCount', mc)
for off in range(16, 200, 4):
    v=struct.unpack_from('<I', h, off)[0]
    if v: print(f'  off 0x{off:03x} = {v} (0x{v:x})')
