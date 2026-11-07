import subprocess, glob, os
D='/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld'
hits=[]
for f in glob.glob(D+'/dyld_shared_cache_arm64e'):
    try:
        with open(f,'rb') as fh:
            chunk=fh.read()
        if b'CoreWiFi.framework' in chunk:
            hits.append(os.path.basename(f))
    except Exception as e:
        print('err', f, e); break
print('主文件含 CoreWiFi:', hits if hits else '否')
import struct
if hits:
    data=open(D+'/dyld_shared_cache_arm64e','rb').read()
    pos=data.find(b'CoreWiFi.framework')
    print('offset of CoreWiFi path:', hex(pos))
    # 附近 400 字节文本（相邻路径=image 路径表区）
    print(repr(data[pos-100:pos+160]))
