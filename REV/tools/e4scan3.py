import glob, os
D='/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld'
needle=b'physicalLayerMode'
found=[]
for f in sorted(glob.glob(D+'/dyld_shared_cache_arm64e.*')):
    try:
        with open(f,'rb') as fh: data=fh.read(8192*1024*1024)
        i=data.find(needle)
        if i>=0:
            found.append((os.path.basename(f), i))
    except Exception as e:
        pass
print('命中分片:', found[:6])
# 取第一个命中的上下文
if found:
    f=os.path.join(D, found[0][0]); off=found[0][1]
    with open(f,'rb') as fh: fh.seek(max(0,off-200)); ctx=fh.read(420)
    # 打印可打印上下文
    import re
    txt=''.join(chr(c) if 32<=c<127 else '.' for c in ctx)
    print('上下文@', hex(off), ':', txt)
    # 收集该片内所有相关方法名
    with open(f,'rb') as fh: data=fh.read()
    names=set()
    start=0
    while True:
        i=data.find(needle,start)
        if i<0: break
        # 方法名可能前缀和方法后缀（- [CWF...]）
        s=max(0,i-80); e=min(len(data),i+160)
        seg=data[s:e]
        m=re.findall(rb'[A-Za-z_:]+physicalLayerMode[A-Za-z_:]*', seg)
        names.update(m)
        start=i+1
    print('相关方法名样本:', sorted(set(x.decode() for x in names))[:12])
