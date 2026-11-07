import os,glob,subprocess
for d in ['/System/Library/dyld','/System/Volumes/Preboot']:
    try: print(d, ':'); print('\n'.join(os.listdir(d)[:25]))
    except Exception as e: print(d,'err',e)
