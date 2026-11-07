"""One-time curated chip collection; preserves factory bank/slot IDs."""
from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1]
# Engine, bank-1 slot, name, params, envelope, rev, echo; trim calibrated after rendering.
rows=[
 (4,0,'DUST BASS',[46,0,5,38],[7,24,13],0,0),
 (5,0,'POCKET KIT',[50,28,24,3],[7,24,14],0,0),
 (9,0,'SMALL SOUL',[1,94,4,0,0,0,100,38],[9,35,22],8,8),
 (9,1,'BONE PULSE',[3,100,0,0,0,0,88,0],[5,20,9],0,0),
 (9,2,'SAVE POINT',[4,86,0,0,0,0,100,28],[38,46,60],42,14),
 (9,3,'RED HEART',[2,100,0,0,0,0,100,12],[7,30,18],4,5),
 (9,4,'DUST ROAD',[1,96,8,16,0,0,12,24],[4,18,9],8,16),
 (9,5,'ODD FRIEND',[6,48,18,12,0,16,8,35],[5,12,8],8,0),
 (9,6,'GOLD HALL',[0,66,16,18,4,0,78,12],[12,36,30],25,12),
 (9,7,'BLUE GHOST',[7,88,0,0,0,0,76,60],[48,42,70],62,20),
 (9,8,'STAR ROOM',[2,88,4,32,0,0,18,68],[5,24,26],32,28),
 (9,9,'LAST HEART',[3,58,16,12,0,0,62,34],[24,46,48],28,12),
 (9,10,'SNOW TOWN',[1,84,4,0,0,0,90,88],[12,40,34],22,10),
 (9,11,'DEEP DREAM',[4,54,0,0,0,0,100,10],[65,50,64],48,8),
 (11,0,'BONE SOLO',[1,100,0,0,0,28,0],[7,35,16],4,6),
 (11,1,'SOUL SOLO',[2,100,0,0,0,22,0],[9,35,20],8,10),
 (11,2,'GHOST CALL',[4,94,0,0,0,46,0],[32,50,54],40,16),
 (11,3,'TINY HERO',[4,100,0,0,0,8,0],[6,28,12],0,0),
 (11,4,'ODD VOICE',[5,56,5,18,12,18,0],[14,34,28],16,10),
 (11,5,'RED ALERT',[0,82,12,28,0,16,12],[6,26,14],8,12),
 (11,6,'MOON SLIDE',[1,90,4,0,0,32,0],[10,38,25],18,16),
 (11,7,'LAST BOSS',[2,82,18,32,0,28,28],[8,40,20],12,12),
 (3,0,'RUIN BELL',[0,44,32,0],[12,44,58],55,18),
 (3,1,'TINY STEPS',[2,12,44,0],[4,15,12],10,4),
 (3,2,'LOST TOY',[3,35,24,1],[7,30,40],38,22),
 (3,3,'MEMORY DUST',[4,66,18,0],[26,48,64],62,12),
]
if __name__=='__main__':
 p=root/'tapes/overworld/tape_dsp.c';src=p.read_text(encoding='utf8')
 start=src.index('static const dk_preset_t PRESETS2');prefix=src[:start];body=src[start:]
 manifest=[]
 for eng,slot,name,params,env,rev,echo in rows:
  matches=list(re.finditer(r'^    \{"[^"\n]+", '+str(eng)+r', .*?\},$',body,re.M))
  m=matches[slot];old=m[0];trim=int(re.search(r', (-?\d+)\},$',old)[1])
  macro={9:'SYN',11:'LD',3:'GLASS',4:'BASS',5:'KIT'}[eng]
  a=lambda x:'{'+', '.join(map(str,x))+'}'
  line=f'    {{"{name}", {eng}, {a(params)}, {a(env)}, FX_N_NONE, 0, 0, 0, {rev}, {echo}, {macro}_M, {trim}}},'
  body=body[:m.start()]+line+body[m.end():]
  manifest.append(dict(engine=eng,bank=1,slot=slot,name=name))
 p.write_text(prefix+body,encoding='utf8')
 (root/'out/underground-presets.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
 # Existing style recipes keep the same IDs and follow the renamed slots.
 p=root/'out/style-presets.json';styles=json.loads(p.read_text(encoding='utf8'))
 for s in styles:
  for r in manifest:
   if all(s[k]==r[k] for k in ('engine','bank','slot')):s['name']=r['name']
 p.write_text(json.dumps(styles,ensure_ascii=False,indent=2),encoding='utf8')
