import re
F='/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_arm64e.26.dyldreadonly'
OFF=81057070-200
with open(F,'rb') as fh:
    fh.seek(OFF); data=fh.read(96*1024)
# 提取所有标识符片段（方法名/类名）
toks=re.findall(rb'[A-Za-z_][A-Za-z_0-9]{4,60}', data)
uniq=[]
for t in toks:
    s=t.decode(errors='ignore')
    if s not in uniq: uniq.append(s)
# 找 phy 相关 & class 名 & 邻居
phy=[t for t in uniq if 'phy' in t.lower() or 'mode' in t.lower() or 'channel' in t.lower() or 'rate' in t.lower()]
print('PHY/信道相关方法（候选类名需识别，下面为相邻 token 序列）:')
print(' | '.join(t for t in uniq[:90]))
print()
print('PHY相关:', ' | '.join(phy[:40]))
