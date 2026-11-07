"""Apply the curated 18-preset collection without changing preset IDs or bank sizes."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# style, engine, bank, slot (zero based), old name, new name, parameters,
# envelope, insert (name, amount, A, B), reverb, echo, original trim, role
ROWS = [
 ("ambient",0,0,0,"MEADOW","MOSS PIANO",[38,35,60,35,55],[22,48,54],("NONE",0,0,0),62,16,0,"мягкое пиано"),
 ("ambient",3,0,0,"GLASS","DEW BELL",[0,28,68,0],[10,40,52],("NONE",0,0,0),68,23,-3,"стеклянные капли"),
 ("ambient",2,0,2,"WARM","HORIZON PAD",[34,48,18,2,48],[60,48,54],("NONE",0,0,0),72,8,1,"тёплый фон"),
 ("ambient",4,0,0,"SUB","EARTH SUB",[16,4,0,72],[18,38,34],("NONE",0,0,0),5,0,-1,"мягкий фундамент"),
 ("ambient",5,0,2,"BRUSH","PAPER KIT",[48,32,16,2],[8,28,20],("NONE",0,0,0),24,0,4,"тихая перкуссия"),
 ("ambient",7,0,0,"KALIMBA","SEED PLUCK",[44,32,58],[8,28,32],("NONE",0,0,0),48,18,1,"деревянный щипок"),
 ("synthwave",1,2,0,"DX KEYS","NEON DX",[72,10,8,64,40],[10,34,28],("CHORUS",24,20,38),28,14,1,"короткие FM-аккорды"),
 ("synthwave",9,2,7,"ARP SYNTH","MOTOR ARP",[0,48,24,42,8,0,16,6],[6,24,14],("NONE",0,0,0),20,24,20,"чёткое арпеджио"),
 ("synthwave",2,2,0,"SUPER PAD","SUNSET PAD",[46,36,18,5,44],[44,42,38],("NONE",0,0,0),44,8,0,"широкие аккорды"),
 ("synthwave",4,2,0,"NIGHT DRIVE","MOTOR BASS",[48,64,4,30],[6,24,18],("NONE",0,0,0),3,0,9,"упругие восьмые"),
 ("synthwave",5,2,0,"LINN","NIGHT KIT",[50,38,18,4],[6,28,18],("NONE",0,0,0),16,0,2,"электронные ударные"),
 ("synthwave",11,2,0,"OUTRUN SAW","NEON SOLO",[0,60,18,26,18,28,12],[12,40,32],("NONE",0,0,0),30,22,13,"лид над миксом"),
 ("8bit",9,1,1,"SQUARE LEAD","PIXEL SQUARE",[3,100,0,0,0,0,100,0],[6,25,12],("NONE",0,0,0),0,0,-3,"меандр для аккордов"),
 ("8bit",9,1,4,"CHIP ARP","PIXEL ARP",[1,100,0,0,0,0,28,0],[4,18,8],("NONE",0,0,0),0,0,16,"быстрое пульсовое арпеджио"),
 ("8bit",9,1,2,"TRIANGLE","PIXEL TRI",[4,92,0,0,0,0,100,0],[12,32,18],("NONE",0,0,0),6,0,6,"мягкий треугольник"),
 ("8bit",4,1,0,"NES BASS","PIXEL BASS",[48,0,5,42],[6,26,12],("NONE",0,0,0),0,0,5,"сухой треугольный бас"),
 ("8bit",5,1,0,"NES KIT","PIXEL KIT",[52,24,28,3],[4,20,12],("NONE",0,0,0),0,0,6,"короткие шумовые ударные"),
 ("8bit",11,1,1,"PULSE 25 LEAD","PIXEL SOLO",[2,100,0,0,0,12,0],[6,30,14],("NONE",0,0,0),0,0,8,"узкий пульсовый лид"),
]
MAC = {0:"FELT",1:"TINE",2:"PAD",3:"GLASS",4:"BASS",5:"KIT",7:"KAL",9:"SYN",11:"LD"}

if __name__ == "__main__":
    path = ROOT / "tapes/overworld/tape_dsp.c"
    src = path.read_text(encoding="utf8")
    manifest = []
    for style,e,b,i,old,new,p,env,fx,rev,echo,trim,role in ROWS:
        # Match exactly once: names shared by other engines are never overwritten.
        pattern = rf'^    \{{"{re.escape(old)}", {e}, .*?\}},$'
        values = lambda xs: "{" + ", ".join(map(str,xs)) + "}"
        line = f'    {{"{new}", {e}, {values(p)}, {values(env)}, FX_N_{fx[0]}, {fx[1]}, {fx[2]}, {fx[3]}, {rev}, {echo}, {MAC[e]}_M, {trim}}},'
        src,n = re.subn(pattern,lambda _:line,src,flags=re.M)
        if n != 1:
            raise SystemExit(f"Expected one original {e}:{old}; found {n}. Apply only once.")
        manifest.append(dict(style=style,engine=e,bank=b,slot=i,name=new,role=role))
    path.write_text(src,encoding="utf8")
    (ROOT/"out/style-presets.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf8")
