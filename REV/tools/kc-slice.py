# kc-slice.py: 从 BootKernelExtensions.kc 提取指定 kext 的可执行切片（kcext）
import struct, sys, pickle
K='/System/Library/KernelCollections/BootKernelExtensions.kc'
f=open(K,'rb')
h=f.read(64); ncmds,scmds=struct.unpack('<II',h[16:24])
f.seek(32); cmds=f.read(scmds)
p=0; segs=[]
while p+8<=len(cmds):
    c,siz=struct.unpack('<II',cmds[p:p+8])
    if siz==0: p+=4; continue
    if c==0x19:
        nm=cmds[p+8:p+24].rstrip(b'\0').decode()
        vm=struct.unpack('<Q',cmds[p+24:p+32])[0]
        fo=struct.unpack('<Q',cmds[p+40:p+48])[0]
        fs=struct.unpack('<Q',cmds[p+48:p+56])[0]
        segs.append((nm,vm,fo,fs))
    p+=siz
try: info=pickle.load(open('/tmp/kcinfo.pkl','rb'))
except FileNotFoundError:
    f.seek(11517952); blob=f.read(4538368)
    import plistlib
    info=plistlib.loads(blob[:blob.find(b'</plist>')+8])
    pickle.dump(info,open('/tmp/kcinfo.pkl','wb'))
arr=info['_PrelinkInfoDictionary']
want=sys.argv[1] if len(sys.argv)>1 else 'com.apple.iokit.IO80211Family'
e=next((x for x in arr if x.get('CFBundleIdentifier')==want),None)
if not e: print('bundle not found:', want); sys.exit(1)
src=e['_PrelinkExecutableSourceAddr'] & (2**64-1); sz=e['_PrelinkExecutableSize']
for nm,vm,fo,fs in segs:
    if vm<=src<vm+fs:
        f.seek(fo+(src-vm)); data=f.read(sz)
        out=sys.argv[2] if len(sys.argv)>2 else '/tmp/%s.bin' % want.split('.')[-1]
        open(out,'wb').write(data)
        print('OK %s (%d B) from %s' % (out,len(data),nm))
        break
