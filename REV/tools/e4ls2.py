import os
for d in ['/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld',
          '/System/Cryptexes/OS/System/Library/dyld']:
    try:
        files=os.listdir(d)
        print(d, '->', files[:12])
        for f in files:
            p=os.path.join(d,f)
            print('  ', f, os.path.getsize(p)//(1024*1024), 'MB' if os.path.getsize(p)>1024*1024 else os.path.getsize(p))
    except Exception as e:
        print(d, 'err', e)
