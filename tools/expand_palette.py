"""Append forty factory sounds in vacant slots, preserving every existing ID."""
import json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
# engine, bank, name, engine knobs, envelope, rev, echo, seed trim
ROWS=[
(0,3,'STUDIO WOOD',[64,8,48,22,28],[8,36,24],22,2,1),
(0,3,'PAPER HAMMER',[28,82,28,76,22],[5,20,14],12,0,1),
(0,3,'SOFT PEDAL',[22,22,82,8,42],[35,56,60],58,12,-2),
(0,3,'TOY ATTIC',[88,64,22,42,20],[4,18,16],28,10,1),
(0,2,'CITY STAB',[78,4,22,12,56],[5,18,12],15,16,2),
(0,2,'VHS CHORD',[36,72,62,8,72],[28,46,48],45,24,-1),
(0,2,'DIAMOND KEY',[94,2,38,0,44],[8,28,26],32,20,0),
(0,2,'VELVET DRIVE',[22,58,44,4,58],[36,42,36],28,12,0),
(1,3,'EMBER KEYS',[18,8,6,12,90],[42,58,64],64,12,0),
(1,3,'FROZEN TINE',[100,0,0,100,12],[4,20,36],55,28,0),
(1,3,'BRONZE DUST',[68,88,6,78,22],[8,26,22],25,6,0),
(1,3,'SLOW CURRENT',[48,32,52,18,84],[65,58,70],72,20,0),
(2,1,'POCKET ORGAN',[58,8,0,8,0],[8,24,16],12,0,0),
(2,1,'PIXEL CHOIR',[26,38,0,6,0],[30,36,32],38,8,-8),
(2,1,'CART STRINGS',[72,12,0,4,0],[12,28,20],20,4,-8),
(2,1,'TINY CLOUD',[38,30,0,1,0],[42,36,38],40,10,-6),
(3,3,'BAMBOO HIT',[2,4,88,0],[3,14,10],15,0,12),
(3,3,'FELT VIBES',[4,18,6,0],[18,40,36],36,8,8),
(3,3,'TIN LANTERN',[0,12,14,2],[5,20,30],28,14,0),
(3,3,'ICE ORBIT',[1,58,90,2],[14,42,64],68,26,0),
(4,3,'CEDAR BASS',[42,20,2,18],[5,24,16],10,0,6),
(4,3,'BOWED EARTH',[18,8,2,94],[66,52,50],28,0,4),
(4,3,'ROUND HUM',[8,0,0,92],[32,46,40],6,0,2),
(4,3,'BRONZE STEP',[88,84,1,12],[4,14,10],6,0,6),
(7,3,'DRY THUMB',[22,8,94],[4,16,14],12,0,0),
(7,3,'AMBER SEED',[74,76,46],[8,36,32],38,12,0),
(7,3,'WATER TINES',[98,6,6],[12,44,56],62,24,0),
(7,3,'HOLLOW LOG',[8,100,100],[6,26,22],24,4,0),
(8,1,'POCKET PIPE',[54,0,0,0,8],[6,24,14],8,0,0),
(8,1,'ROUND OCARINA',[12,0,8,2,18],[12,32,22],22,8,0),
(8,1,'REED SPRITE',[42,4,24,3,10],[8,26,16],10,4,0),
(8,1,'PIXEL HORN',[48,0,8,8,8],[6,24,18],12,8,0),
(10,2,'NEON NYLON',[2,82,26,22,72,0],[6,24,20],24,14,-4),
(10,2,'STEEL MOTOR',[3,94,18,58,18,0],[4,18,12],15,18,-4),
(10,2,'KOTO SPARK',[4,88,32,12,58,0],[5,22,22],28,20,-5),
(10,2,'HARP AURA',[1,26,86,72,12,0],[32,52,60],66,16,-12),
(11,2,'CHROME REED',[2,52,16,22,0,16,0],[12,36,26],22,20,8),
(11,2,'SOFT COMET',[4,86,0,0,0,22,0],[22,44,36],38,24,8),
(11,2,'FM CRYSTAL',[5,72,0,16,8,8,0],[6,30,28],30,26,8),
(11,2,'STREET SUPER',[3,62,10,28,32,16,8],[8,32,22],18,18,10),
]
MAC={0:'FELT',1:'TINE',2:'PAD',3:'GLASS',4:'BASS',7:'KAL',8:'WIND',10:'PLK',11:'LD'}
if __name__=='__main__':
 path=ROOT/'tapes/overworld/tape_dsp.c';s=path.read_text(encoding='utf8')
 if 'STUDIO WOOD' in s:raise SystemExit('Palette already appended')
 a=s.index('static const dk_preset_t PRESETS2');b=s.index('};',a)
 old=re.findall(r'^    \{"[^"\n]+", \d+, .*?\},$',s[a:b],re.M)
 ca=s.index('static const uint8_t PCOUNT');cb=s.index('};',ca)
 counts=[int(n) for n in re.findall(r'\d+',s[s.index('{',ca)+1:cb])]
 assert len(counts)==48
 groups={};k=0
 for e in range(12):
  for bank in range(1,4):
   n=counts[e*4+bank];groups[e,bank]=old[k:k+n];k+=n
 assert k==len(old)
 manifest=[];fmt=lambda xs:'{'+', '.join(map(str,xs))+'}'
 for e,bank,name,p,env,rev,echo,trim in ROWS:
  slot=len(groups[e,bank]);assert slot<12
  line=f'    {{"{name}", {e}, {fmt(p)}, {fmt(env)}, FX_N_NONE, 0, 0, 0, {rev}, {echo}, {MAC[e]}_M, {trim}}},'
  groups[e,bank].append(line);counts[e*4+bank]+=1
  manifest.append(dict(engine=e,bank=bank,slot=slot,name=name))
 body='static const dk_preset_t PRESETS2[] = {\n'
 for (e,bank),lines in groups.items():
  if lines:body+=f'    /* engine {e}, bank {bank} */\n'+'\n'.join(lines)+'\n'
 s=s[:a]+body+s[b:]
 ca=s.index('static const uint8_t PCOUNT');cb=s.index('};',ca)
 body='static const uint8_t PCOUNT[12 * DK_NBLK] = {\n'
 for e in range(12):body+='    '+', '.join(map(str,counts[e*4:e*4+4]))+',\n'
 s=s[:ca]+body+s[cb:];path.write_text(s,encoding='utf8')
 (ROOT/'out/palette-presets.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
 print('Added',len(manifest),'presets. Total:',sum(counts))
