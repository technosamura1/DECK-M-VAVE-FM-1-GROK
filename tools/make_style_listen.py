"""Build a portable listening room from real DECK host renders."""
import html
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out"
ENG = ["PIANO","KEYS","PAD","GLASS","BASS","DRUMKIT","WORLD","KALIMBA","WINDS","SYNTH","PLUCK","LEAD"]
META = {
 "ambient": ("01", "AMBIENT", "Moss / Dew", "72 BPM · A minor", "Мягкое пиано, редкие капли и тёплые аккорды. Оставляй паузы: пространство здесь тоже играет.", "#a4c7a1"),
 "synthwave": ("02", "SYNTHWAVE", "Night Motor", "108 BPM · A minor", "Ровный бас, короткое арпеджио и лид над широким пэдом. Запиши сначала ритм и бас, затем аккорды.", "#e8a3bd"),
 "8bit": ("03", "8BIT", "Pixel Garden", "132 BPM · A minor", "Сухие пульсы, меандр, треугольник и шумовые ударные. Начни с баса и мелодии, добавь быстрые арпеджио.", "#e6cd80"),
}
rows = json.loads((OUT/"style-presets.json").read_text(encoding="utf8"))
curation_path=OUT/'preset-curation.json'
curation=json.loads(curation_path.read_text(encoding='utf8')) if curation_path.exists() else None
def shown_slot(r):
    if not curation:return r['slot']+1
    mask=curation['masks'][r['engine']*4+r['bank']]
    return (mask&((1<<(r['slot']+1))-1)).bit_count() if mask&(1<<r['slot']) else 0
parts=[]
for style,(number,title,track,bpm,desc,color) in META.items():
    patches=[r for r in rows if r['style']==style]
    cards=[]
    for i,r in enumerate(patches):
        if not shown_slot(r):continue
        bank=["AMB","8BIT","SYN","CINE"][r['bank']]
        cards.append(f'''<article class="patch"><small>{i+1:02} / {ENG[r['engine']]}</small>
        <h3>{html.escape(r['name'])}</h3><p>{r['role']}</p>
        <audio controls preload="none" src="styles/{style}/solo_{i+1}.wav"></audio>
        <footer>Банк {bank} · пресет {shown_slot(r):02}</footer></article>''')
    parts.append(f'''<section id="{style}" style="--accent:{color}"><div class="sectionhead"><span>{number}</span><h2>{title}</h2></div>
    <div class="demo"><div><small>ШЕСТЬ ДОРОЖЕК / ДЕМО</small><h3>{track}</h3><p>{bpm} · Am → F → C → G</p></div>
    <audio controls preload="metadata" src="styles/{style}/demo.wav"></audio></div>
    <details><summary>Сравнить с прежними настройками</summary><p>Та же композиция и уровни дорожек; прежние пресеты в тех же слотах. Громкость файлов дополнительно не нормализована.</p><audio controls preload="none" src="styles-before/{style}/demo.wav"></audio></details>
    <p class="intro">{desc}</p><div class="grid">{''.join(cards)}</div></section>''')
chip_manifest=OUT/'underground-presets.json'
if chip_manifest.exists():
    chip=json.loads(chip_manifest.read_text(encoding='utf8'))
    cards=[]
    for r in chip:
        if not shown_slot(r):continue
        eng=ENG[r['engine']]
        rel=f"underground/{eng}/b1_{r['slot']+1:02}_{r['name'].replace(' ','_')}.wav"
        cards.append(f'''<article class="patch"><small>{eng} / 8BIT {shown_slot(r):02}</small>
        <h3>{r['name']}</h3><audio controls preload="none" src="{rel}"></audio>
        <footer>Банк 8BIT · пресет {shown_slot(r):02}</footer></article>''')
    parts.insert(0,f'''<section id="underground" style="--accent:#90d9d0"><div class="sectionhead"><span>NEW</span><h2>SMALL WORLDS</h2></div>
    <p class="intro">Отобранные чип-тембры в духе Undertale: полые пульсы, маленькие голоса, странные колокольчики,
    мягкие треугольники и шумовые ударные. Новые настройки волн и огибающих, названия на экране — как на карточках.</p>
    <div class="demo"><div><small>ШЕСТЬ ДОРОЖЕК / 92 BPM</small><h3>Snow Walk</h3><p>Спокойный игровой эпизод</p></div>
    <audio controls preload="metadata" src="underground/snow_walk.wav"></audio></div>
    <div class="demo"><div><small>ШЕСТЬ ДОРОЖЕК / 148 BPM</small><h3>Last Corridor</h3><p>Быстрый игровой эпизод</p></div>
    <audio controls preload="none" src="underground/last_corridor.wav"></audio></div>
    <p class="intro">Это собственные короткие музыкальные примеры для проверки тембров. Отдельные звуки ниже
    записаны на одинаковой громкости дорожки; в композициях инструменты сведены по ролям.</p>
    <div class="grid">{''.join(cards)}</div></section>''')
palette_manifest=OUT/'palette-presets.json'
if palette_manifest.exists():
    palette=json.loads(palette_manifest.read_text(encoding='utf8'))
    cards=[]
    for r in palette:
        if not shown_slot(r):continue
        eng=ENG[r['engine']];bank=['AMB','8BIT','SYN','CINE'][r['bank']]
        rel=f"palette/sounds/{eng}/b{r['bank']}_{r['slot']+1:02}_{r['name'].replace(' ','_')}.wav"
        cards.append(f'''<article class="patch"><small>{eng} / {bank} {shown_slot(r):02}</small><h3>{r['name']}</h3>
        <audio controls preload="none" src="{rel}"></audio><footer>Банк {bank} · пресет {shown_slot(r):02}</footer></article>''')
    demos=[]
    for style,label,tempo in [('ambient','AMBER WATER',72),('synthwave','STEEL MOTOR',108),('8bit','POCKET CITY',132),('acoustic','CEDAR ROOM',96)]:
        demos.append(f'''<div class="demo"><div><small>ШЕСТЬ ДОРОЖЕК / {tempo} BPM</small><h3>{label}</h3><p>{style.upper()}</p></div>
        <audio controls preload="none" src="palette/{style}/demo.wav"></audio></div>''')
    parts.insert(0,f'''<section id="palette" style="--accent:#e5bf8b"><div class="sectionhead"><span>SELECT</span><h2>EXPANDED PALETTE</h2></div>
    <p class="intro">Отобранные пресеты из расширения: деревянные клавиши и басы, мягкие электрические тембры,
    игровые аккорды и духовые, щипковые и synthwave-лиды. Прежние слоты сохранены; новые занимают свободные места.
    REC и сохранение дублей работают как прежде.</p>{''.join(demos)}
    <p class="intro">Каждый тембр отдельно: нота, аккорд, фраза. Для басов — басовый рисунок.
    Уровень дорожки одинаковый; громкость пресетов выровнена. Названия банков и номера — как в прошивке.</p>
    <div class="grid">{''.join(cards)}</div></section>''')
if curation:
    retired=''.join(f"<li>{ENG[r['engine']]} · {html.escape(r['name'])} → {html.escape(r['replacement']['name'])}</li>" for r in curation['removed'])
    parts.insert(0,f"<section><h2>354 выбранных тембра</h2><p>66 близких вариантов убраны из выбора, 60 из них — из прежних наборов. Старые проекты продолжают загружать свои звуки. Ниже — избранные тембры и композиции; номера карточек соответствуют сокращённому меню.</p><details><summary>Какие повторы убраны</summary><ul>{retired}</ul></details></section>")
page='''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DECK — Three Worlds</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#17191b;color:#eeeae0;--accent:#a4c7a1}
*{box-sizing:border-box}body{margin:0}main{max-width:1080px;margin:auto;padding:48px 24px 70px}
.eyebrow,small,footer,nav{font:12px ui-monospace,monospace;letter-spacing:1px;color:#b1b4b5}
h1{font-size:clamp(42px,8vw,82px);font-weight:500;letter-spacing:-4px;margin:20px 0}h1 span{color:#a4c7a1}
.lede{max-width:680px;font-size:18px;line-height:1.6;color:#c2c4c3}nav{display:flex;flex-wrap:wrap;gap:12px;margin:28px 0 50px}
nav a{border:1px solid #515758;padding:12px 18px;border-radius:24px;color:#eeeae0;text-decoration:none}
section{scroll-margin-top:20px;margin:60px 0}.sectionhead{display:flex;align-items:center;gap:16px;color:var(--accent)}
.sectionhead span{font:14px monospace;border:1px solid var(--accent);padding:10px;border-radius:50%}
h2{font-size:30px;letter-spacing:3px;font-weight:500}.demo{padding:24px;background:#24282a;border-left:3px solid var(--accent);display:flex;align-items:center;justify-content:space-between;gap:20px}
h3{margin:12px 0;font-weight:500;font-size:21px}.demo h3{font-size:30px}.demo p,.patch p{color:#bbc0bf}
.intro{max-width:780px;line-height:1.6;margin:24px 0}details{padding:16px 0;color:#bbc0bf}summary{cursor:pointer}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}
.patch{background:#202426;padding:20px;border:1px solid #363c3c;border-radius:10px}.patch p{font-size:14px;min-height:40px;line-height:1.5}
audio{width:100%;height:40px}.demo audio{max-width:420px}footer{margin-top:16px;font-size:11px;letter-spacing:0}
.guide{border-top:1px solid #4b5252;padding-top:25px;line-height:1.7}.guide h2{font-size:24px;letter-spacing:0}
@media(max-width:800px){.grid{grid-template-columns:repeat(2,1fr)}.demo{display:block}.demo audio{max-width:none}}
@media(max-width:480px){main{padding:28px 16px}.grid{grid-template-columns:1fr}h1{letter-spacing:-2px}h2{font-size:24px}}
</style><main><div class="eyebrow">DECK / OVERWORLD · SOUND COLLECTION 01</div>
<h1>Small <span>Worlds.</span></h1><p class="lede">Выразительные игровые тембры и три основных банка одного синта. Звуки и композиции записаны программным рендерером прошивки.</p>
<nav><a href="#palette">EXPANDED PALETTE</a><a href="#underground">SMALL WORLDS</a><a href="#ambient">AMBIENT</a><a href="#synthwave">SYNTHWAVE</a><a href="#8bit">8BIT</a></nav>
'''+''.join(parts)+'''<div class="guide"><h2>Как играть на FM-1</h2>
<p>В сборке с этими пресетами: выбери дорожку → HOME + PRESET (SOUNDS).
OCT меняет инструмент, чёрные клавиши — банк, белые — пресет. Номера указаны на карточках.
Сначала запиши бас и ударные; следующим дублем — аккорды, затем мелодию и украшения.</p>
<p>Демо: восемь тактов, мелодия вступает с третьего. Громкости настроены для совместной игры;
отдельные карточки показывают звуки с теми же уровнями микшера. Хвосты дописаны до затухания.
В 8BIT сухой звук намеренный — ревербератор можно добавить позже.</p>
<p>Это предпрослушивание до установки: на настоящем FM-1 новые звуки ещё не проверены.
Файлы WAV должны оставаться рядом со страницей в папке styles. Общий лимит синта — 32 голоса вместе с хвостами.</p></div>
</main><script>document.querySelectorAll('audio').forEach(a=>a.addEventListener('play',()=>{document.querySelectorAll('audio').forEach(b=>{if(a!==b)b.pause()})}));</script></html>'''
(OUT/"listen.html").write_text(page,encoding="utf8")
print(OUT/"listen.html")
