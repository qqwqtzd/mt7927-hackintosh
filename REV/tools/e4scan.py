import struct, glob, os
D='/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld'
files=sorted(glob.glob(D+'/dyld_shared_cache_arm64e*'))
def head(f):
    with open(f,'rb') as fh: return fh.read(512)
for f in files[:60]:
    h=head(f)
    if h[:4]!=b'dyld': 
        continue
    try:
        mo,mc,io,ic=struct.unpack_from('<4I', h, 16)
    except Exception: continue
    if io or ic:
        print(os.path.basename(f), f'size={os.path.getsize(f)//(1024*1024)}MB map={mo}/{mc} images@0x{io:x} count={ic}')
