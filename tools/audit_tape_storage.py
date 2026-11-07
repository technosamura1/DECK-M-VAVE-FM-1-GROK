"""Read-only audit of the address space exposed by the current FM-1 platform.
Not a hardware capacity or throughput measurement. Never accesses the synth.
"""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parents[1]
plat=(root/'base/include/plat.h').read_text(encoding='utf8')
app=(root/'deck/app.h').read_text(encoding='utf8')
def define(src,name):
    return int(re.search(r'#define\s+'+name+r'\s+(0x[0-9A-Fa-f]+|\d+)',src)[1],0)
lo,hi=define(plat,'PLAT_DATA_LO'),define(plat,'PLAT_DATA_HI')
start,span,count=[define(app,s) for s in ('DK_PROJ_AT','DK_PROJ_SPAN','DK_NPROJ')]
used=[(start+i*span,start+(i+1)*span,f'project {i+1}') for i in range(count)]
store=re.search(r'#define APP_STORE_MAP\s+\{([^}]+)',app)[1]
for i,offset in enumerate(re.findall(r'0x[0-9A-Fa-f]+',store)):
    a=int(offset,16)
    if lo<=a<hi:used.append((a,a+8192,f'store pair {i}'))
used.sort();free=[];p=lo
for a,b,name in used:
    assert lo<=a<b<=hi and a>=p,(a,b,name)
    if a>p:free.append((p,a))
    p=b
if p<hi:free.append((p,hi))
n=sum(b-a for a,b in free)
result=dict(kind='source audit, not a device measurement',data_bytes=hi-lo,
    reserved=[dict(start=hex(a),end=hex(b),name=name) for a,b,name in used],
    unassigned=[dict(start=hex(a),end=hex(b),bytes=b-a) for a,b in free],
    unassigned_bytes=n,pcm16_mono_44100_seconds=n/88200,
    pcm16_stereo_44100_seconds=n/176400,
    write_during_audio_supported=False,
    reason='plat.h documents blocking writes; device/unity.c st_erase calls audio_silence',
    writes_performed=0)
print(json.dumps(result,indent=2))
